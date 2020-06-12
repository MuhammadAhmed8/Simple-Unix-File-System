#ifndef INODE_H
#define INODE_H

#include<time.h>

struct Inode{
    int id;
    int size; 
    struct tm dateCreated;
    short int blocksUsed;
    short int ftype;                // file type (0 for empty | 1 for regular file | 2 for directory)
    int fd_count;                   // file descriptors count
    short int directBlocks[12];     // 12 direct data block pointers
    short int singleIndirectBlock;        // a single indirect block which would contain other data block addresses.

    //this could contain other attributes such as datetime for file accesses,
    //file permissions, file owner, protection, groups, ACL, etc. I am omitting
    //for the sake of simplicity.
};

typedef struct Inode Inode;

#endif