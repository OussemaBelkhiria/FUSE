#define FUSE_USE_VERSION 32
#include <fuse.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
typedef struct TreesList{
    struct Tree *child;
    struct TreesList *next;
} TreesList;
typedef struct Tree{
    char *filename; //must not exceed 255
    char* content; // must not exceed 512
    char* linkpath; 
    mode_t mode;
    nlink_t nlink;
    off_t size;
    int filetype;
    time_t atime;
    time_t mtime;
    struct TreesList *children;
} Tree;
typedef struct CurrentTree {
    struct Tree* current_directory;
    struct Tree* parent;
    char* filename; 
} CurrentTree;
int search_tree(char *);
int add_file(mode_t ,int);
