#include "../include/syscall.h"
#include "../include/terminal.h"
#include "../include/graphics.h"

extern void start_desktop();
// kernel gatekeeper
void syscall_handler(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    (void)ecx;
    (void)edx; // -Werror are now pissing me off
    if (eax == 1) {
        char* str = (char*)ebx;
        print_string(str);
    }
    else if (eax == 2) {
        print_string("\n\n[Kernel] Process terminated.\n"); // that was cringe
        __asm__ volatile (
            "mov $0x90000, %%esp\n\t" 
            "sti\n\t"                 
            "jmp start_desktop\n\t"   
            : : : "memory"
        );
    }
    // (devs) you can easily add more later 
    // eax == 2 for file read, eax == 3 for mem allocation, etc.
}