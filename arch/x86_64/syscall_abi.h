#ifndef LOLOS_X86_64_SYSCALL_ABI_H
#define LOLOS_X86_64_SYSCALL_ABI_H

#include <stdint.h>

#define X64_SYSCALL_EXIT 0u
#define X64_SYSCALL_WRITE 1u

#define X64_SYSCALL_ERRNO_EBADF 9
#define X64_SYSCALL_ERRNO_E2BIG 7
#define X64_SYSCALL_ERRNO_EFAULT 14
#define X64_SYSCALL_ERRNO_ENOSYS 38

#define X64_SYSCALL_EXIT_SENTINEL 0x8000000000000000ULL
#define X64_USER_STACK_GUARD 0x0000000040001000ULL
#define X64_USER_STACK_PAGE 0x0000000040002000ULL
#define X64_USER_STACK_TOP 0x0000000040003000ULL
#define X64_SYSCALL_MAX_WRITE 128u

struct x64_syscall_frame {
    uint64_t user_rsp;
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t r9;
    uint64_t r8;
    uint64_t r10;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rax;
    uint64_t user_rip;
    uint64_t user_rflags;
};

#endif
