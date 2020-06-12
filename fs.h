#ifndef FS_H
#define FS_H

#include "inode.h"
#include "superblock.h"
#include<stdint.h>

#define MAX_FILES 2000

#define SUPERBLOCK_INDEX 0

#define DIRECT_BLOCKS 12
#define INDIRECT_BLOCKS 1

#define TYPE_DIRECTORY 2
#define TYPE_REGULAR  1
#define TYPE_EMPTY 0

#define MAX_FD_ENTRIES 256

#define SEEK_S 0
#define SEEK_C 1
#define SEEK_E 2

// file open modes
#define rd 0          // read mode
#define wr 1          // write mode
#define app 2         // append mode
#define rdwr 3        // read write mode


//function prototypes ----------
struct tm get_Time();
void set_Inode(Inode* inode, short type);
void init_superBlock(Superblock* superBlock);
int make_fs();
int find_Free_Inode(short type);

int open_file(char* froute,int mode);
int lseek_file(int fd, int offset,int whence);
int read_file(int fd, char* buff, int count);
int write_file(int fd, char* buff, int count);
int close_file(int fd);

int cd_Dir(char* froute);
int make_Dir(char* dirname);
int rm_Dir(char* dirname);

int add_Dir_Entry(int dir,int newInode, char* fname);
int find_Entry(char filepath[][30],int f_len,int* lastEntryParent, int* lastEntry);
int rm_Dir_Entry(int dir,int entryPos);


int find_Free_Block();
int create_Bitmap(int bsize);
int read_Bitmap_Block(int idx);
void mark_Block_free(int dBlock);
void mark_Block_Used(int dBlock);
int writeBitmap();
int reserve_Block();


int fd_open(int inode,int mode);
void fd_close(int fd);

// ---------------------------

//file descriptor 
typedef struct{
    int isOpen;
    int cursor;
    int inodeIdx;
    int mode;
} FileDescriptor;

#endif