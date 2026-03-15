#include "../include/elf.h"
#include "../include/terminal.h"
#include "../include/pmm.h"
#include "../include/paging.h"

// dynamic ring 3 drop
void jump_to_ring3(uint32_t entry_point) {
    __asm__ volatile(
        "cli\n\t"
        "mov $0x23, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%esp, %%eax\n\t" 
        "pushl $0x23\n\t"      
        "pushl %%eax\n\t"      
        "pushf\n\t"           
        "popl %%eax\n\t"
        "or $0x200, %%eax\n\t" 
        "pushl %%eax\n\t"
        "pushl $0x1B\n\t"      
        "pushl %0\n\t"
        "iret\n\t"
        : : "r" (entry_point) : "eax", "memory"
    );
}

void load_elf_and_execute(uint8_t* file_buffer) {
    elf_header_t* header = (elf_header_t*)file_buffer;
    if (header->magic != ELF_MAGIC) {
        print_string("ERROR: File is not a valid ELF executable!\n");
        return;
    }
    uint8_t* ph_addr = file_buffer + header->phoff;
    for (int i = 0; i < header->phnum; i++) {
        elf_program_header_t* ph = (elf_program_header_t*)(ph_addr + (i * header->phentsize));
        if (ph->type == 1) {
            uint32_t pages = (ph->memsz / 4096) + 1;
            for (uint32_t p = 0; p < pages; p++) {
                uint32_t phys_addr = (uint32_t)pmm_alloc_block();
                map_page(phys_addr, ph->vaddr + (p * 4096));
            }
            uint8_t* dest = (uint8_t*)ph->vaddr;
            uint8_t* src = file_buffer + ph->offset;
            for (uint32_t b = 0; b < ph->filesz; b++) {
                dest[b] = src[b];
            }
            for (uint32_t b = ph->filesz; b < ph->memsz; b++) {
                dest[b] = 0;
            }
        }
    }
    jump_to_ring3(header->entry);
}