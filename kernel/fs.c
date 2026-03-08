#include "../include/fs.h"
#include "../include/ata.h"
#include "../include/string.h"
#include "../include/terminal.h"

FileEntry directory[MAX_FILES];
uint32_t next_free_lba = 2; // LBA 1 is the directory, so file data starts at LBA 2

void fs_init() {
    // first load the directory table from the hard drive into memory
    ata_read_sector(1, (uint8_t*)directory);
    // then scan the directory to figure out where the next available data sector is
    next_free_lba = 2;
    for (int i = 0; i < MAX_FILES; i++) {
        // checking if a file exists
        if (directory[i].name[0] != 0) {
            if (directory[i].start_lba >= next_free_lba) {
                next_free_lba = directory[i].start_lba + 1;
            }
        }
    }
}

void fs_create_file(char* name) {
    // blassa blassa blasa
    for (int i = 0; i < MAX_FILES; i++) {
        if (directory[i].name[0] == 0) { 
            // to prevent overflow
            int j = 0;
            while (name[j] != '\0' && j < 23) {
                directory[i].name[j] = name[j];
                j++;
            }
            directory[i].name[j] = '\0';
            directory[i].size = 0;
            directory[i].start_lba = next_free_lba;
            next_free_lba++;
            // save to sector 1
            ata_write_sector(1, (uint8_t*)directory);
            print_string("File created: ");
            print_string(directory[i].name);
            print_char('\n');
            return;
        }
    }
    print_string("Error: Directory is full (Max 16 files).\n");
}

void fs_list_files() {
    int found = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (directory[i].name[0] != 0) {
            print_string("- ");
            print_string(directory[i].name);
            print_string(" (Sector: ");
            print_time_number(directory[i].start_lba); // here we reuse our printer number
            print_string(")\n");
            found = 1;
        }
    }
    if (!found) {
        print_string("Directory is empty.\n");
    }
}


void fs_write_file(char* name, char* data) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (strcmp(directory[i].name, name) == 0) {
            uint8_t buffer[512];
            memset(buffer, 0, 512); // clean first to avoid leftover data
            int data_len = strlen(data);
            if (data_len > 512) data_len = 512; // hard limit to 1 sector for now
            for (int j = 0; j < data_len; j++) {
                buffer[j] = data[j];
            }
            // data written in a dedicated sector
            ata_write_sector(directory[i].start_lba, buffer);
            // update and save
            directory[i].size = data_len;
            ata_write_sector(1, (uint8_t*)directory);
            print_string("Data written to ");
            print_string(name);
            print_char('\n');
            return;
        }
    }
    print_string("Error: File not found.\n");
}

void fs_read_file(char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (strcmp(directory[i].name, name) == 0) {
            if (directory[i].size == 0) {
                print_string("[File is empty]\n");
                return;
            }
            uint8_t buffer[512];
            ata_read_sector(directory[i].start_lba, buffer);
            // prints the exact number of chars in the file, not the whole sector
            for (uint32_t j = 0; j < directory[i].size; j++) {
                print_char(buffer[j]);
            }
            print_char('\n');
            return;
        }
    }
    print_string("Error: File not found.\n");
}

void fs_delete_file(char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (strcmp(directory[i].name, name) == 0) {
            // delete here erases the metadata
            directory[i].name[0] = '\0';
            directory[i].size = 0;
            directory[i].start_lba = 0; 
            // save the new directory state to disk
            ata_write_sector(1, (uint8_t*)directory);
            print_string("File deleted: ");
            print_string(name);
            print_char('\n');
            return;
        }
    }
    print_string("Error: File not found.\n");
}