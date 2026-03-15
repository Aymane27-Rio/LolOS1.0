#include "../include/paging.h"
#include "../include/pmm.h"

uint32_t* page_directory;

void map_page(uint32_t physical_addr, uint32_t virtual_addr) {
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x03FF;
    
    // lowest bit means "present"
    if ((page_directory[pd_index] & 1) == 0) {
        uint32_t* new_table = (uint32_t*)pmm_alloc_block();
        for(int i = 0; i < 1024; i++) {
            new_table[i] = 6; 
        }
        page_directory[pd_index] = ((uint32_t)new_table) | 7; 
    }
    uint32_t* page_table = (uint32_t*)(page_directory[pd_index] & ~0xFFF);
    page_table[pt_index] = (physical_addr & ~0xFFF) | 7;
}

void init_paging(uint32_t framebuffer_addr){
    page_directory = (uint32_t*)pmm_alloc_block();
    for(int i = 0; i < 1024; i++) {
        page_directory[i] = 6; // not Present, Read/Write
    }
    for (uint32_t i = 0; i < 4 * 1024 * 1024; i += 4096) {
        map_page(i, i);
    }
    //the screen is 800x600 pixels * 4 bytes per pixel = 1,920,000 bytes. 
    // map a 2mb to be safe, even though we won't use all of it
    for (uint32_t i = 0; i < 2 * 1024 * 1024; i += 4096) {
        map_page(framebuffer_addr + i, framebuffer_addr + i);
    }
    __asm__ volatile (
        "mov %0, %%cr3\n\t"
        "mov %%cr0, %%eax\n\t"
        "or $0x80000000, %%eax\n\t"
        "mov %%eax, %%cr0"
        : : "r" (page_directory) : "eax"
    );
}