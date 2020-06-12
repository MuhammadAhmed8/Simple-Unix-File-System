#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include<fcntl.h>
#include "disk.h"

#define abc main
int isDiskOpened = 0;
int diskHandle = -1;  

int initDisk(char* name){
    if(name == 0){
        //default disk name 
        name = DEFAULT_DISK_NAME;
    }
    int fd = open(name,O_WRONLY | O_CREAT | O_TRUNC,0644);

    if(fd < 0){
        fprintf(stderr,"\nCan't Initialize disk\n");
        return -1;
    }

    char buffer[BLOCK_SIZE];
    memset(buffer,0,BLOCK_SIZE);
    for(int i=0;i<TOTAL_BLOCKS;i++){
        write(fd,buffer,BLOCK_SIZE);
    }
    
    close(fd);
    return 0;
}

int openDisk(char* name){
    if(name == NULL){
        name = DEFAULT_DISK_NAME;
    }
    if(isDiskOpened){
        fprintf(stderr,"Disk is already opened");
        return -1;
    }
    int fd = open(name,O_RDWR,0644);
    if(fd < 0){
        fprintf(stderr,"Can't open disk.");
        return -1;
    }
    diskHandle = fd;
    isDiskOpened = 1;
    return 0;

}

int closeDisk(char* name){
    if(isDiskOpened != 0){
        close(diskHandle);
    }
    isDiskOpened = 0;
    diskHandle = -1;
    return 0;
}

int isBlockValid(int block){
    return block >=0 && block < TOTAL_BLOCKS;
}

int writeBlock(int blockNumber, char* buffer){
    if(!isBlockValid){
        fprintf(stderr,"Invalid Block Number");
        return -1;
    }
    if(lseek(diskHandle, BLOCK_SIZE * blockNumber, SEEK_SET) < 0){
        fprintf(stderr, "Seek error in write block");
        return -1;
    }
    int i = write(diskHandle, buffer, BLOCK_SIZE);
    if(i <= 0)
        perror("\nCant write in writeBlock disk.c\n");
    return i;

}

int readBlock(int blockNumber,char* buffer){
    if(!isBlockValid){
        fprintf(stderr,"Invalid Block Number");
        return -1;
    }
    if(lseek(diskHandle, BLOCK_SIZE * blockNumber, SEEK_SET) < 0){
        fprintf(stderr, "Seek error in write block");
        return -1;
    }
    int i = read(diskHandle,buffer,BLOCK_SIZE);
    if(i <= 0)
        perror("\nCant read in readBlock disk.c\n");
    return i;
}

