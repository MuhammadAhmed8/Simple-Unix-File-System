#ifndef SBLOCK_H
#define SBLOCK_H

typedef struct
{
    int magicNumber;
    int fileSystemSize; 

    int inodeRegion;    //start of inode region
    int freeRegion;     //start of free blocks mapping region
    int dataRegion;     //start of data blocks region

    int maxInodesBlocks;      //number of inodes that can be made
    int maxDataBlocks;   //number of data blocks that could be used
    int maxFreeBlocks;  //number of block set aside for storing free block indexes

}Superblock;

#endif

