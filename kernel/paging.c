#include "../include/paging.h"
#include "../include/pmm.h"

uint32_t* page_directory;

void map_page(uint32_t physical_addr, uint32_t virtual_addr) {
    // 1. Calculate which Directory and which Table this address belongs to
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x03FF;
    
    // 2. Check if a Page Table already exists here. (The lowest bit '1' means Present)
    if ((page_directory[pd_index] & 1) == 0) {
        // We need a new Page Table! Ask the PMM for a raw 4KB block of RAM.
        uint32_t* new_table = (uint32_t*)pmm_alloc_block();
        
        // Initialize the new table as Not Present & Read/Write (0x02)
        for(int i = 0; i < 1024; i++) {
            new_table[i] = 2; 
        }
        
        // Put the new table into the Directory (0x03 means Present & Read/Write)
        page_directory[pd_index] = ((uint32_t)new_table) | 3; 
    }
    
    // 3. Get the Page Table, and map the exact physical block to the virtual index!
    uint32_t* page_table = (uint32_t*)(page_directory[pd_index] & ~0xFFF);
    page_table[pt_index] = (physical_addr & ~0xFFF) | 3;
}

void init_paging(uint32_t framebuffer_addr) {
    // Allocate the main Page Directory
    page_directory = (uint32_t*)pmm_alloc_block();
    for(int i = 0; i < 1024; i++) {
        page_directory[i] = 2; // Not Present, Read/Write
    }
    
    // 1. Identity Map the Kernel (First 4MB of RAM)
    // We map Physical 0x000000 to Virtual 0x000000 so our code doesn't crash when paging turns on!
    for (uint32_t i = 0; i < 4 * 1024 * 1024; i += 4096) {
        map_page(i, i);
    }
    
    // 2. Map the VESA Framebuffer!
    // Our screen is 800x600 pixels * 4 bytes per pixel = 1,920,000 bytes. 
    // We map a 2MB chunk to be safe.
    for (uint32_t i = 0; i < 2 * 1024 * 1024; i += 4096) {
        map_page(framebuffer_addr + i, framebuffer_addr + i);
    }
    
    // 3. Load the Directory into the CR3 register and flip the CR0 Paging Switch!
    __asm__ volatile (
        "mov %0, %%cr3\n\t"
        "mov %%cr0, %%eax\n\t"
        "or $0x80000000, %%eax\n\t" // Flip Bit 31 (Paging Enable)
        "mov %%eax, %%cr0"
        : 
        : "r" (page_directory) 
        : "eax"
    );
}