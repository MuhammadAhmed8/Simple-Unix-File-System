// ---------*********************------------
// ******** FILE SYSTEM FUNCTIONS ***********
// ---------*********************------------

#include"fs.h"
#include"superblock.h"
#include"inode.h"
#include"disk.h"
#include"directory_entry.h"
#include"utility.h"
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<math.h>
#include<string.h>
#include<time.h>
#include<stdint.h>

static FileDescriptor fd_table[MAX_FD_ENTRIES];

// superblock (global)
char superBlockBuffer[BLOCK_SIZE];
Superblock* sBlock;

char bitmapBuffer[BLOCK_SIZE];
uint8_t* bitmap;

int workingDir = 0;
Inode* workingDirInode,*rootDirInode;
const int INODE_BLOCK_ENTRIES = BLOCK_SIZE/sizeof(Inode);

/*  ---------------------------------------------- HELPER FUNCTIONS -----------------------------------------*/

void init_superBlock(Superblock* superBlock){
    superBlock->magicNumber = 4;
    superBlock->fileSystemSize = DISK_SPACE;
    superBlock->inodeRegion = 1;
    superBlock->maxInodesBlocks = ceil(MAX_FILES / (BLOCK_SIZE / sizeof(Inode)));
    superBlock->freeRegion = superBlock->inodeRegion + 
                             superBlock->maxInodesBlocks;
    superBlock->maxFreeBlocks = 1;
    
    superBlock->dataRegion = superBlock->freeRegion + superBlock->maxFreeBlocks;
    superBlock->maxDataBlocks = TOTAL_BLOCKS -  superBlock->freeRegion - superBlock->maxFreeBlocks;

}


void set_Inode(Inode* inode, short type){
    inode->fd_count = 0;
    inode->dateCreated = get_Time();
    inode->ftype = type;
    inode->size = 0;
    inode->blocksUsed = 0;
}

Inode* read_Inode(int inodeIdx, char* buff){
    int block = sBlock->inodeRegion + inodeIdx/INODE_BLOCK_ENTRIES;
    int pos = inodeIdx % INODE_BLOCK_ENTRIES;
    readBlock(block,buff);
    Inode* inodes = (Inode*)buff;
    return &inodes[pos];
}


void inode_write(int inodeIdx, char *block_buf) {
    int block = sBlock->inodeRegion + inodeIdx/INODE_BLOCK_ENTRIES;
    writeBlock(block, block_buf);
}


//returns the free inode number and sets the free Inode
int find_Free_Inode(short type){
    char inodeBuffer[BLOCK_SIZE];
    
    Inode* inodeEntries = (Inode*)inodeBuffer; 
    int start = sBlock->inodeRegion;

    for(int i=0; i<sBlock->maxInodesBlocks; i++){
        if(readBlock(start+i,inodeBuffer) <= 0){
            perror("Cant read in find_free_node");
        }
        for(int j=0; j < INODE_BLOCK_ENTRIES; j++){
            if(inodeEntries[j].ftype == 0){
                set_Inode(&inodeEntries[j],type);
        
                if(writeBlock(start + i,inodeBuffer) <= 0){
                    perror("cant write free inode in find_free_inode");
                }

                return i * INODE_BLOCK_ENTRIES + j;    // return inode number
            }
        }
    }

    return -1;
}


int find_Entry(char filepath[][30],int f_len,int* lastEntryParent, int* lastEntry){
    
    char buffer[BLOCK_SIZE];
    int currDir;                // inode number 
    int currDirBlock;           // inode block number (has to add inode group start address)
    int f_idx = 0;              // index for filepath

    if(strcmp(filepath[0],"/") == 0){
        currDir = 0;
        f_idx = 1;
    }else{
        currDir = workingDir;
    }

    currDirBlock = currDir / INODE_BLOCK_ENTRIES;
    int totalInodeBlocks = sBlock->maxInodesBlocks; 
    
    char dataBuffer[BLOCK_SIZE];
    Inode* inodeEntries = (Inode*)buffer;
    Directory_entry* directoryEntries = (Directory_entry*)dataBuffer;
    int blockEntries = BLOCK_SIZE/sizeof(Directory_entry);
    
    int prevDirBlock = currDirBlock - 1;
    int prevDir = -1;
    int entryPos;

    while(f_idx < f_len && currDirBlock < sBlock->maxInodesBlocks){

        //first inode block index either root or working directory is set to currDirBlock
        //reads the block and get the currDirInode

        if(currDirBlock != prevDirBlock)
            readBlock(sBlock->inodeRegion + currDirBlock, buffer);

        Inode* currDirInode = &inodeEntries[currDir%INODE_BLOCK_ENTRIES];

        int totalEntries = currDirInode->size / sizeof(Directory_entry);

        int j = 0;
        int dirFound = 0;

        //traverse all the data blocks which list directory entries of current Inode
        //and finds the desired entry.

        entryPos = 0;
        prevDir = currDir;

        while( j < currDirInode->blocksUsed && !dirFound){
    
            int dBlock = currDirInode->directBlocks[j];
            readBlock(sBlock->dataRegion + dBlock,dataBuffer);

            int entriesListed = min(blockEntries, totalEntries - j * blockEntries);
            
            int k;

            for(k=0; k<entriesListed && !dirFound; k++){
                
                if(strcmp(filepath[f_idx],directoryEntries[k].name) == 0){
                    
                    currDir = directoryEntries[k].inode_ref;
                    currDirBlock = currDir / blockEntries;
                    dirFound = 1;
                    f_idx += 1;

                }
            }

            j++;
            entryPos += k - 1;

        }

        if(f_idx == f_len - 1 && !dirFound){
            currDir = -1; 
            break;
        }
        else if(!dirFound){
            fprintf(stderr,"\nCan't find %s. INVALID NAME \n",filepath[f_idx]);
            return -1;                                                              // FAILURE   
        }
                
    }

    if(lastEntryParent != NULL)
        *lastEntryParent = prevDir;

    if(lastEntry != NULL)
        *lastEntry = currDir;

    return entryPos;                                                                       // SUCCESS
}


int rm_Dir_Entry(int dir,int entryPos){ 
    char dirBuff[BLOCK_SIZE];
    Inode* dirInode = read_Inode(dir, dirBuff);
    
    int totalEntries = dirInode->size / sizeof(Directory_entry);
    int blockEntries = BLOCK_SIZE / sizeof(Directory_entry);

    int blockIdx = entryPos / blockEntries;
    int dataBlock = sBlock->dataRegion + dirInode->directBlocks[blockIdx];

    char blockBuff[BLOCK_SIZE],inodeBuff[BLOCK_SIZE];
    char lastBlockBuff[BLOCK_SIZE];

    readBlock(dataBlock,blockBuff);

    int offset = entryPos % blockEntries;


    //move the last entry to the position where deleted

    int lastBlockIdx = dirInode->blocksUsed - 1;
    int lastBlock = sBlock->dataRegion + dirInode->directBlocks[lastBlockIdx];
    int lastEntryOffset = (totalEntries - 1) % blockEntries;
    readBlock(lastBlock, lastBlockBuff);
    
    memcpy(blockBuff + offset, lastBlockBuff + lastEntryOffset, sizeof(Directory_entry));

    writeBlock(dataBlock, blockBuff);

    dirInode->size -= sizeof(Directory_entry);
    
    if( lastEntryOffset == 0){
        mark_Block_free(lastBlock);
        dirInode->blocksUsed--;
    }

    inode_write(dir,dirBuff);

    return 0;

}


int add_Dir_Entry(int dir,int newInode, char* fname){

    char dirBuffer[BLOCK_SIZE];
    Inode* inodeEntries = (Inode*)dirBuffer;

    short fb;                                                 // free block 
    int dirBlock = dir / INODE_BLOCK_ENTRIES;                 // dir is directory inode index 
    
    readBlock(sBlock->inodeRegion + dirBlock,dirBuffer);
    
   
    Inode* dirInode = &inodeEntries[dir % INODE_BLOCK_ENTRIES];
    int blockIdx = dirInode->blocksUsed;
    int isBlockHasSpace = BLOCK_SIZE - (( dirInode->size % BLOCK_SIZE ) + sizeof(Directory_entry));

    char dataBuffer[BLOCK_SIZE];
    Directory_entry* dirEntries = (Directory_entry* )dataBuffer;

    char indrBuffer[BLOCK_SIZE];
    short* indirectBlockEntries = (short*)indrBuffer;
    int pos = 0;

    if(blockIdx >= DIRECT_BLOCKS){
        readBlock(dirInode->singleIndirectBlock, indrBuffer);
    }

    //if block has space
    if(isBlockHasSpace >= 0 && dirInode->blocksUsed != 0){
        blockIdx -= 1;
        if(blockIdx >= DIRECT_BLOCKS){
            int indrBlockIdx =  blockIdx - DIRECT_BLOCKS;
            fb = indirectBlockEntries[indrBlockIdx];
        }
        else{
            fb = dirInode->directBlocks[blockIdx];
        }

        pos = ( dirInode->size % BLOCK_SIZE ) /  sizeof(Directory_entry);
    }
    else{
        fb = reserve_Block();
        if(fb == -1){
            perror("\nFree block not found in create file\n");
            return -1;
        }
        if(blockIdx >= DIRECT_BLOCKS){
            int indrBlockIdx =  blockIdx - DIRECT_BLOCKS;
            indirectBlockEntries[indrBlockIdx] = fb;
            writeBlock(dirInode->singleIndirectBlock,indrBuffer);
        }
        else
        {
            dirInode->directBlocks[blockIdx] = fb;
        }
        dirInode->blocksUsed++;
        
    }   

    readBlock(sBlock->dataRegion + fb,dataBuffer);

    strcpy(dirEntries[pos].name,fname);

    dirEntries[pos].inode_ref = newInode;

    writeBlock(sBlock->dataRegion + fb,dataBuffer);     //writing dir data block that holds entries
   
    dirInode->size += sizeof(Directory_entry);
    
    writeBlock(sBlock->inodeRegion + dirBlock,dirBuffer);    //writing dir inode 


    if(writeBitmap() <= 0){
        perror("\ncant write bitvector in create file\n");
        return -1;
    }


    return 0;                                             // SUCCESS

    
}


/* ----------------------------------------| DIRECTORY OPERATIONS |----------------------------------------------*/


int make_Dir(char* dirname){
    
    char filepath[1][30];                                     // splitted filename path string wrt '/' delimeter
    strcpy(filepath[0],dirname);

    int dirParent = 0, newDir = -1;                          // inodes for last and second last entry of filepath

    if(find_Entry(filepath,1,&dirParent,&newDir) == -1){
        perror("\n Invalid name. \n");
        return -1;
    }
    if(newDir != -1){
        printf("\n A directory with that name already exists. \n");
        return 0;
    }
    
    newDir = find_Free_Inode(TYPE_DIRECTORY);
    if(add_Dir_Entry(dirParent,newDir, dirname) == -1) {
        printf("\nFile failed to create\n");
        return -1;
    }
    char bu[BLOCK_SIZE];
    readBlock(sBlock->inodeRegion + newDir/INODE_BLOCK_ENTRIES, bu);
    Inode* i = (Inode*)bu;

    add_Dir_Entry(newDir,newDir,".");                          // self reference         
    add_Dir_Entry(newDir,dirParent,"..");                      // parent reference
 
    printf("\n%s Directory created\n",dirname);
    return 0;

}

int rm_Dir(char* dirname){
    char filepath[1][30];
    char buff[BLOCK_SIZE];

    strcpy(filepath[0],dirname);

    int dirParent = 0, dir, pos;                         

    if((pos = find_Entry(filepath,1,&dirParent,&dir)) == -1){
        perror("\n Invalid name. \n");
        return -1;
    }
    Inode *dirToDelete = read_Inode(dir,buff);
    printf("pos: %d   ",pos);
    if(dirToDelete->ftype != TYPE_DIRECTORY){
        perror("can't find a directory with that name\n");
        return -1;
    }

    printf("size : %d\n",dirToDelete->size);
    if(dirToDelete->size > 64){
        perror("Can't delete a non-empty directory\n");
        return -1;
    }


    rm_Dir_Entry(dirParent, pos);
    

    return 0;
    

}

int cd_Dir(char* froute){
    char filepath[30][30];                                     // splitted filename path string wrt '/' delimeter
    
    int len = parse_Filename(froute,filepath);

    int destinationDir = -1;                                        // inodes for last and second last entry of filepath
    
    if(find_Entry(filepath,len,NULL,&destinationDir) == -1){
        perror("\n Invalid route in cd_Dir \n");
        return -1;
    }

    if(destinationDir == -1){
        fprintf(stderr,"\n %s isn't found \n",filepath[len -1]);
        return -1;
    }

    workingDir = destinationDir;
    printf("working directory = %s",filepath[len-1]);

    return 0;

}


/* ----------------------------------------------  | FILE OPERATIONS | ---------------------------------------- */


int make_fs(){
    //writing the superblock at the first block
    sBlock = (Superblock*)malloc(sizeof(Superblock));
    init_superBlock(sBlock);
    if(writeBlock(SUPERBLOCK_INDEX,(char*)sBlock) < 0){
        perror("make_fs failed to write superblock");
        return -1;
    }

    // writing Inode for root on Index 0 of inodes region. 
    Inode rootInode;
    set_Inode(&rootInode,TYPE_DIRECTORY);
    int root = 0;
    if(writeBlock(sBlock->inodeRegion + root,(char*)&rootInode) < 0){
        perror("\nCant write the root inode in make_fs\n");
        return -1;
    }

    //add_Dir_Entry(root,root,".");
    //add_Dir_Entry(root,root,"..");

    // it writes the empty inodes all over the inode region.
    int i = sBlock->maxInodesBlocks;
    char buffer[BLOCK_SIZE];
    memset(buffer,0,BLOCK_SIZE);
    while(i--)
        writeBlock(sBlock->inodeRegion + 1, buffer);

    create_Bitmap(sBlock->maxDataBlocks);

    free(sBlock);
    sBlock = 0;

    return 0;
}

// Opens the file by setting file inode number in fd_table and returns fd, 
// and creates a new file if file not found.
int open_file(char* froute,int mode){

    if(!froute){
        perror("\nfile name can't be null\n");
        return -1;
    }

    char filepath[10][30];                                           // splitted filename path string wrt '/' delimeter
    int f_len = parse_Filename(strdup(froute),filepath);             // length of fpath
    char* fname = filepath[f_len - 1];

    int dir = 0, file = -1;                                        // inodes for last and second last entry of filepath

    if(find_Entry(filepath,f_len,&dir,&file) == -1){
        perror("\nFile not opened\n");
        return -1;
    }
    
    // if file not present, create a new file
    if(file == -1){
        file = find_Free_Inode(TYPE_REGULAR);

        if(add_Dir_Entry(dir,file, fname) == -1) {
            perror("\nFile failed to create\n");
            return -1;
        }
        printf("%s","\nNew File created \n");

    }

    //save inode entry in file descriptor table
    int fd;
    if(( fd = fd_open(file,mode)) != -1 ){
        printf("\n %s File is opened\n",fname);
        return fd;
    }
    
    return -1;
}


// write_file will write bytes specified by count to the file.
// returns number of bytes written or -1 if failed.
//write_file(fd,"hello world",9)
int write_file(int fd, char* buff, int count){

    if(buff == NULL || count <= 0)
        return 0;

    if(fd < 0 || fd >= MAX_FD_ENTRIES || !fd_table[fd].isOpen )
        return -1;

    if(fd_table[fd].mode == rd)
        return -1;

    char inodeBuffer[BLOCK_SIZE];
    char dataBuffer[BLOCK_SIZE];

    int inodeBlock = sBlock->inodeRegion + (fd_table[fd].inodeIdx / INODE_BLOCK_ENTRIES);
    int inodePos = (fd_table[fd].inodeIdx) % INODE_BLOCK_ENTRIES;
  
    readBlock(inodeBlock, inodeBuffer);

    Inode* inodes = (Inode*)inodeBuffer;
    Inode* inode = &inodes[inodePos];

    int cursor = fd_table[fd].cursor;

    int blockIdx = cursor / BLOCK_SIZE;                     // index of inode->blocks
    int bytesWritten = 0;
    int dataRegion = sBlock->dataRegion;
    
    while(bytesWritten < count && blockIdx < 12){
        
        // grow file by allocating a new block
        if(blockIdx >= inode->blocksUsed){
            int fb = reserve_Block();
            inode->directBlocks[blockIdx] = fb;
            inode->blocksUsed++ ;    
            
        }
        
        int datablock = dataRegion + inode->directBlocks[blockIdx];      // actual data block number

        readBlock(datablock,dataBuffer);
        
        int blockOffset = cursor % BLOCK_SIZE;
        int toWrite = min(count - bytesWritten, BLOCK_SIZE - blockOffset);

        memcpy(&dataBuffer[blockOffset], &buff[bytesWritten], toWrite);       

        if(writeBlock(datablock, dataBuffer) < BLOCK_SIZE){
            return -1;
        }

        cursor += toWrite;
        bytesWritten += toWrite;

        // increase file size if necessary
        if(inode->size < cursor)
            inode->size = cursor;

        
        blockIdx++;    
    }

    fd_table[fd].cursor = cursor;

    writeBlock(inodeBlock, inodeBuffer);

    return bytesWritten;
}

int read_file(int fd, char* buff, int count){
    if(buff == NULL || count <= 0)
        return 0;

    if(fd < 0 || fd >= MAX_FD_ENTRIES || !fd_table[fd].isOpen )
        return -1;
    
    if(fd_table[fd].mode == wr )
        return -1;


    char inodeBuffer[BLOCK_SIZE];
    char dataBuffer[BLOCK_SIZE];

    int inodeBlock = sBlock->inodeRegion + (fd_table[fd].inodeIdx / INODE_BLOCK_ENTRIES);
    int inodePos = (fd_table[fd].inodeIdx) % INODE_BLOCK_ENTRIES;

    readBlock(inodeBlock, inodeBuffer);
    Inode* inodes = (Inode*)inodeBuffer;
    Inode* inode = &inodes[inodePos];

    int cursor = fd_table[fd].cursor;

    int blockIdx = cursor / BLOCK_SIZE;                     // index of inode->blocks
    int bytesRead = 0;
    int dataRegion = sBlock->dataRegion;

    while(bytesRead < count && blockIdx < 12){
        int datablock = dataRegion + inode->directBlocks[blockIdx];      // actual data block number
        
        readBlock(datablock,dataBuffer);
        
        int blockOffset = cursor % BLOCK_SIZE;
        int toRead = min(count - bytesRead, BLOCK_SIZE - blockOffset);

        memcpy(buff + bytesRead,dataBuffer + blockOffset, toRead);       

       
        cursor += toRead;
        bytesRead += toRead;

        // increase file size if necessary
        if(inode->size < cursor)
            inode->size = cursor;

        
        blockIdx++;    
    }
    buff[bytesRead] = '\0';
    fd_table[fd].cursor = cursor;

    return bytesRead;
}
int endOfFile(int i){
    char buff[BLOCK_SIZE];
    return read_Inode(i,buff)->size; 
}

int lseek_file(int fd, int offset,int whence) {
    FileDescriptor *file;

    if (fd < 0 || fd >= MAX_FD_ENTRIES) {
        return -1;
    }
    
    file = &fd_table[fd];
    if (!file->isOpen) {
        return -1;
    }
    int cur = file->cursor;

    if(whence == SEEK_S){
        cur = offset;
    }
    else if( whence == SEEK_C){
        cur += offset;
    }
    else if( whence == SEEK_E){
        char buff[BLOCK_SIZE];
        cur =  endOfFile(file->inodeIdx) - abs(offset);  
    }

    if(cur < 0)
        cur = 0;

    file->cursor = cur;

    return offset;
}





/*  ---------------------------------  Bit vector -------------------------------------------------*/

// BIT VECTOR to keep track of free data blocks. Each bit
// will represent a data block. 
int create_Bitmap(int bsize){
    bsize /= 8; 
    bitmap = (uint8_t*) calloc(bsize, sizeof(uint8_t));
    if(bitmap == NULL){
        perror("\nbitmap can't be created in createBitmap\n");
        return -1;
    }
    writeBlock(sBlock->freeRegion,(char*)bitmap);
    free(bitmap);
    return 0;
}

int read_Bitmap_Block(int idx){
    int b = readBlock(idx,bitmapBuffer);
    bitmap = (uint8_t*)bitmapBuffer;     //sets the global bitvector variable
    return b;                           // return number of bytes read
}

//00000111000100001

// 2^4 = 16
// 00010000
//00000111 |  00010000 = 000011 00
void mark_Block_free(int dBlock){
    int byte = dBlock / 8;
    int bit = dBlock % 8;
    bitmap[byte] = bitmap[byte] & ~power_2(bit);
}

void mark_Block_Used(int dBlock){
    int byte = dBlock / 8;
    int bit = dBlock % 8;
    bitmap[byte] = bitmapBuffer[byte] | power_2(bit);
}

int writeBitmap(){
    return writeBlock(sBlock->freeRegion, (char*)bitmap);
}

// finds and zeros a new data block and returns block number
int reserve_Block(){
    int fb = find_Free_Block();

    if(fb == -1){
        perror("\n Can't reserve a new block \n");
        return -1;
    }

    mark_Block_Used(fb);

    // zeroes the new block
    char zeroBuff[BLOCK_SIZE];
    memset(zeroBuff,0,BLOCK_SIZE);
    writeBlock(sBlock->dataRegion + fb,zeroBuff);

    return fb;
}

void printBitmap(){
    for(int i=0;i<sBlock->maxDataBlocks;i++){
        char b[9];
        int i=0;
        uint8_t n  = bitmap[i];
        while (i<8) {
            if (n & 1)
                b[i++] = '1';
           else
                b[i++] = '0';

            n >>= 1;
        }
        printf("%s",b);
    }
}
// returns datablock index if free
int find_Free_Block(){
    for(int byte=0; byte<sBlock->maxDataBlocks/8; byte++){

        //if all bits are not 1
        if(~bitmap[byte] != 0 ){

            int bit = 0;
            
            while(bit < 8){
                // 11110111 & 00001000 = !0 = 1  - 4th bit is free
                if( !(bitmap[byte] & power_2(bit)) ){
                    return (byte * 8 ) + bit;       
                }

                bit++;
            }
        }

    }

    return -1;              // NOT FOUND
    
}


/* ------------------------------------ FILE DESCRIPTOR TABLE ------------------------------------------ */

void fd_init(){
    for (int i = 0; i < MAX_FD_ENTRIES; i++) {
        if (!fd_table[i].isOpen) {
            fd_table[i].isOpen = 0;
        }
    }
}
int fd_open(int inode,int mode) {

    for (int i = 0; i < MAX_FD_ENTRIES; i++) {
        if (!fd_table[i].isOpen) {
            if(mode == app){
                fd_table[i].cursor = endOfFile(inode);
            }
            else{
                fd_table[i].cursor = 0;
            }

            fd_table[i].isOpen = 1;
            fd_table[i].inodeIdx = inode;
            fd_table[i].mode = mode;
            return i;       // return file descriptor index 
        }
    }

    return -1;      // failure
}

void fd_close(int fd) {
    fd_table[fd].isOpen = 0;
}

int close_file(int fd){
    int inode_index,inode_blk,inode_pos;

    Inode *inode;
    char inodeBuf[BLOCK_SIZE];

    // Fail if given bad file descriptor
    if (fd < 0 || fd >= MAX_FD_ENTRIES) {
        return -1;
    }

    // Fail if fd table entry is already closed
    if (!fd_table[fd].isOpen) {
        return -1;
    }

    // Read the inode from disk
    inode_index = fd_table[fd].inodeIdx;
    inode_blk = sBlock->inodeRegion + inode_index/INODE_BLOCK_ENTRIES;
    inode_pos = inode_index % INODE_BLOCK_ENTRIES;

    readBlock(inode_blk, inodeBuf);
    inode = &((Inode*)inodeBuf)[inode_pos];

    // Close fd table entry
    fd_close(fd);

    // Decrement open fd count and delete file if necessary
    inode->fd_count -= 1;
    if (inode->fd_count == 0) {
        //free_Inode(inode_index);
    } else {
        // Write updated inode to disk
        readBlock(inode_blk, inodeBuf);  
    }

    return 0;

}



int main(){
                        
    char c;
    printf("enter r to reset or make disk: ");
    scanf(" %c",&c);
    if(c == 'r'){
        initDisk("mydisk.bin");
        openDisk("mydisk.bin");
        make_fs();
    }else{
        openDisk("mydisk.bin");
    }

    fd_init();
    
    readBlock(SUPERBLOCK_INDEX,superBlockBuffer);
    sBlock = (Superblock*)superBlockBuffer;
    read_Bitmap_Block(sBlock->freeRegion);

    // ------------ test code ---------------------------
    int fd;

    make_Dir("books");
    make_Dir("lectures");
    cd_Dir("lectures");
    make_Dir("OperatingSystems");
    make_Dir("Statistics");
    make_Dir("Psychology");
    cd_Dir("OperatingSystems");
    make_Dir("Project");
    cd_Dir("Project");
    make_Dir("UnixFileSystem");
    cd_Dir("UnixFileSystem");
    fd = open_file("fs.c",wr);
    close_file(fd);
    fd = open_file("fs.h",rd);
    close_file(fd);


    cd_Dir("..");
    cd_Dir("..");
    cd_Dir("..");
    cd_Dir("..");
    make_Dir("images");



    
    fd = open_file("/lectures/OperatingSystems/Project/UnixFileSystem/report.pdf",app);
    char data[100] = "The file system I implemented is divided into four logical sections.";
    
    int n = write_file(fd, data, strlen(data));
    printf("\nbytes written: %d\n",n);
    
    lseek_file(fd,0,SEEK_S);

    char readbuff[100];
    read_file(fd,readbuff,80);
    printf("%s\n",readbuff);



    printf("\nwd: %d",workingDir);

    close_file(fd);
   // closeDisk("mydisk.bin");
}

