#include "../include/syscall.h"
#include "../include/terminal.h"

// kernel gatekeeper
void syscall_handler(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    (void)ecx;
    (void)edx; // -Werror are now pissing me off
    if (eax == 1) {
        char* str = (char*)ebx;
        print_string(str);
    }
    
    // (devs) you can easily add more later 
    // eax == 2 for file read, eax == 3 for mem allocation, etc.
}