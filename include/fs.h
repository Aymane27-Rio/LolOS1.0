#ifndef FS_H
#define FS_H

#include <stdint.h>
#define MAX_FILES 16

// 32 bytes. 16 of them fit perfectly in a 512-byte sector
typedef struct {
    char name[24];
    uint32_t size;
    uint32_t start_lba;
} FileEntry;

void fs_init();
void fs_create_file(char* name);
void fs_list_files();
void fs_write_file(char* name, char* data);
void fs_read_file(char* name);
void fs_delete_file(char* name);

#endif