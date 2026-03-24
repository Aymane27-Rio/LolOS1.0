#ifndef VFS_H
#define VFS_H

#include <stdint.h>

#define FS_FILE        0x01
#define FS_DIRECTORY   0x02
#define FS_CHARDEVICE  0x03
#define FS_BLOCKDEVICE 0x04
#define FS_PIPE        0x05
#define FS_SYMLINK     0x06
#define FS_MOUNTPOINT  0x08

struct vfs_node; 
struct dirent {
    char name[128]; 
    uint32_t ino; //inode number
};
//abstraction layer
typedef struct vfs_node {
    char name[128];     // requested filename
    uint32_t flags;     // file or dir
    uint32_t length;    //size in bytes
    // no class in C no probs
    uint32_t (*read)(struct vfs_node*, uint32_t offset, uint32_t size, uint8_t* buffer);
    uint32_t (*write)(struct vfs_node*, uint32_t offset, uint32_t size, uint8_t* buffer);
    void (*open)(struct vfs_node*);
    void (*close)(struct vfs_node*);
    struct dirent* (*readdir)(struct vfs_node*, uint32_t index);
    struct vfs_node* (*finddir)(struct vfs_node*, char* name);
    
    struct vfs_node* ptr;
} vfs_node_t;

//global root (usually the slash /)
extern vfs_node_t* fs_root; 

uint32_t vfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
uint32_t vfs_write(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
void vfs_open(vfs_node_t* node);
void vfs_close(vfs_node_t* node);
struct dirent* vfs_readdir(vfs_node_t* node, uint32_t index);
vfs_node_t* vfs_finddir(vfs_node_t* node, char* name);

#endif