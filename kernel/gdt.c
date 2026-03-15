#include "../include/gdt.h"

struct tss_entry_struct {
    uint32_t prev_tss; 
    uint32_t esp0;     
    uint32_t ss0;      
    uint32_t esp1, ss1, esp2, ss2, cr3, eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs, ldt;
    uint16_t trap, iomap_base;
} __attribute__((packed));

struct tss_entry_struct tss_entry;


uint64_t gdt[6];

//the pointer structure the CPU requires
struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdtp;

void init_gdt() {
    gdt[0] = 0; 
    // kernel mode
    gdt[1] = 0x00CF9A000000FFFF;
    gdt[2] = 0x00CF92000000FFFF;
    // user mode
    gdt[3] = 0x00CFFA000000FFFF;
    gdt[4] = 0x00CFF2000000FFFF;
    // initialize the TSS memory to zero
    uint8_t* tss_ptr = (uint8_t*)&tss_entry;
    for(uint32_t i = 0; i < sizeof(tss_entry); i++) tss_ptr[i] = 0;
    tss_entry.ss0 = 0x10; 
    tss_entry.esp0 = 0x90000;
    tss_entry.iomap_base = sizeof(tss_entry);
    uint32_t base = (uint32_t)&tss_entry;
    uint32_t limit = sizeof(tss_entry) - 1;
    gdt[5] = ((uint64_t)(base & 0xFF000000) << 32) |
             ((uint64_t)(0x0089) << 40) | // 0x89 = Present, Ring 0, TSS
             ((uint64_t)(limit & 0xF0000) << 32) |
             ((uint64_t)(base & 0x00FFFFFF) << 16) |
             ((uint64_t)(limit & 0x0000FFFF));
    gdtp.limit = (sizeof(uint64_t) * 6) - 1;
    gdtp.base = (uint32_t)&gdt;

    // load the GDT, then perform a "Far Jump" to flush the CPU pipeline
    // and force it to use our brand new Code Segment (0x08)
    __asm__ volatile (
        "lgdt %0\n\t"
        "pushl $0x08\n\t"
        "pushl $1f\n\t"
        "retf\n\t"
        "1:\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "mov $0x2B, %%ax\n\t"
        "ltr %%ax\n\t"
        : : "m" (gdtp) : "eax", "memory"
    );
}