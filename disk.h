#ifndef DISK_H
#define DISK_H

#define BLOCK_SIZE 4096
#define TOTAL_BLOCKS 8192
#define DISK_SPACE 33554432 //size in bytes ( = 32 MB )
 
#define DEFAULT_DISK_NAME "virtual disk";


int initDisk(char* name);

int openDisk(char* name);

int closeDisk(char* name);
int isBlockValid(int block);

int writeBlock(int blockNumber, char* buffer);

int readBlock(int blockNumber,char* buffer);

#endif