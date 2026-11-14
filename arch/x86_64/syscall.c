#include <stddef.h>
#include <stdint.h>

#include "page_table.h"
#include "pmm.h"
#include "serial.h"
#include "syscall.h"
#include "syscall_abi.h"

#define X64_MSR_EFER 0xC0000080u
#define X64_MSR_STAR 0xC0000081u
#define X64_MSR_LSTAR 0xC0000082u
#define X64_MSR_FMASK 0xC0000084u
#define X64_MSR_GS_BASE 0xC0000101u
#define X64_MSR_KERNEL_GS_BASE 0xC0000102u
#define X64_EFER_SCE 0x0000000000000001ULL
#define X64_SYSCALL_FMASK 0x0000000000047700ULL
#define X64_SYSCALL_KERNEL_CS 0x08u
#define X64_SYSRET_USER_BASE 0x10u

struct x64_syscall_cpu_local {
    uint64_t kernel_rsp;
    uint64_t user_rsp;
    uint64_t kernel_resume_rsp;
    uint64_t user_exit_status;
    uint64_t user_write_seen;
};

_Static_assert(offsetof(struct x64_syscall_cpu_local, kernel_rsp) == 0u,
               "syscall assembly kernel RSP offset mismatch");
_Static_assert(offsetof(struct x64_syscall_cpu_local, user_rsp) == 8u,
               "syscall assembly user RSP offset mismatch");
_Static_assert(offsetof(struct x64_syscall_cpu_local, kernel_resume_rsp) == 16u,
               "syscall assembly resume RSP offset mismatch");
_Static_assert(offsetof(struct x64_syscall_frame, user_rflags) == 120u,
               "syscall assembly frame layout mismatch");
_Static_assert(sizeof(struct x64_syscall_frame) == 128u,
               "syscall assembly frame size mismatch");

extern unsigned char __x64_syscall_stack_top[];
extern unsigned char __x64_user_test_text_start[];
extern unsigned char __x64_user_test_text_end[];
extern void x64_syscall_entry(void);
extern void x64_user_test_entry(void);
extern void x64_enter_user(uint64_t user_rip, uint64_t user_rsp);

static struct x64_syscall_cpu_local syscall_cpu_local
    __attribute__((aligned(16)));

static void write_msr(uint32_t msr, uint64_t value)
{
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);

    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high) : "memory");
}

static uint64_t read_msr(uint32_t msr)
{
    uint32_t low;
    uint32_t high;

    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

int x64_syscall_init(void)
{
    uint64_t stack_physical = x64_pmm_allocate_page();
    uint64_t star;
    uint64_t efer;

    if (stack_physical == 0u ||
        !x64_page_table_map_user_page(X64_USER_STACK_PAGE,
                                      stack_physical, 1, 0)) {
        return 0;
    }

    syscall_cpu_local.kernel_rsp =
        (uint64_t)(uintptr_t)__x64_syscall_stack_top;
    syscall_cpu_local.user_rsp = 0u;
    syscall_cpu_local.kernel_resume_rsp = 0u;
    syscall_cpu_local.user_exit_status = UINT64_MAX;
    syscall_cpu_local.user_write_seen = 0u;

    write_msr(X64_MSR_GS_BASE,
              (uint64_t)(uintptr_t)&syscall_cpu_local);
    write_msr(X64_MSR_KERNEL_GS_BASE, 0u);

    /* SYSCALL enters with CS=0x08/SS=0x10; SYSRET uses CS=0x23/SS=0x1B. */
    star = ((uint64_t)X64_SYSRET_USER_BASE << 48) |
           ((uint64_t)X64_SYSCALL_KERNEL_CS << 32);
    write_msr(X64_MSR_STAR, star);
    write_msr(X64_MSR_LSTAR, (uint64_t)(uintptr_t)x64_syscall_entry);
    write_msr(X64_MSR_FMASK, X64_SYSCALL_FMASK);

    efer = read_msr(X64_MSR_EFER);
    write_msr(X64_MSR_EFER, efer | X64_EFER_SCE);
    return 1;
}

static void halt_invalid_user_return(void) __attribute__((noreturn));

static void halt_invalid_user_return(void)
{
    serial_write("Syscall rejected invalid user return state; halting.\n");
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

static void validate_syscall_return(const struct x64_syscall_frame *frame)
{
    uint64_t code_start = (uint64_t)(uintptr_t)__x64_user_test_text_start;
    uint64_t code_end = (uint64_t)(uintptr_t)__x64_user_test_text_end;

    if (frame->user_rip < code_start || frame->user_rip >= code_end ||
        frame->user_rsp <= X64_USER_STACK_GUARD ||
        frame->user_rsp > X64_USER_STACK_TOP ||
        !x64_page_table_validate_user_range(frame->user_rsp - 1u, 1u, 1)) {
        halt_invalid_user_return();
    }
}

static int64_t syscall_write(uint64_t descriptor, uint64_t user_buffer,
                             uint64_t length)
{
    char kernel_buffer[X64_SYSCALL_MAX_WRITE];
    uint64_t index;

    if (descriptor != 1u) {
        return -X64_SYSCALL_ERRNO_EBADF;
    }
    if (length > X64_SYSCALL_MAX_WRITE) {
        return -X64_SYSCALL_ERRNO_E2BIG;
    }
    if (length == 0u) {
        return 0;
    }
    if (!x64_page_table_validate_user_range(user_buffer, length, 0)) {
        return -X64_SYSCALL_ERRNO_EFAULT;
    }

    for (index = 0u; index < length; index++) {
        kernel_buffer[index] =
            ((const volatile char *)(uintptr_t)user_buffer)[index];
    }
    for (index = 0u; index < length; index++) {
        serial_write_char(kernel_buffer[index]);
    }
    syscall_cpu_local.user_write_seen = 1u;
    return (int64_t)length;
}

uint64_t x64_syscall_dispatch(struct x64_syscall_frame *frame)
{
    uint64_t number;

    validate_syscall_return(frame);
    number = frame->rax;
    switch (number) {
    case X64_SYSCALL_EXIT:
        syscall_cpu_local.user_exit_status = frame->rdi;
        serial_write("Ring-3 syscall exit status=");
        serial_write_u32((uint32_t)frame->rdi);
        serial_write("\n");
        return X64_SYSCALL_EXIT_SENTINEL;
    case X64_SYSCALL_WRITE:
        return (uint64_t)syscall_write(frame->rdi, frame->rsi, frame->rdx);
    default:
        return (uint64_t)-X64_SYSCALL_ERRNO_ENOSYS;
    }
}

int x64_user_test_run(void)
{
    syscall_cpu_local.user_exit_status = UINT64_MAX;
    syscall_cpu_local.user_write_seen = 0u;
    x64_enter_user((uint64_t)(uintptr_t)x64_user_test_entry,
                   X64_USER_STACK_TOP);
    return syscall_cpu_local.user_exit_status == 42u &&
           syscall_cpu_local.user_write_seen != 0u &&
           x64_page_table_validate_user_range(X64_USER_STACK_PAGE, 1u, 1) &&
           !x64_page_table_validate_user_range(X64_USER_STACK_GUARD, 1u, 1);
}
