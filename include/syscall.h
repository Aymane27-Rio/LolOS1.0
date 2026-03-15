#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>

void syscall_handler(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx);

#endif