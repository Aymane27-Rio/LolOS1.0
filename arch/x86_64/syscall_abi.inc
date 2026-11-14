; Keep these offsets synchronized with struct x64_syscall_cpu_local in syscall.c.
X64_CPU_KERNEL_RSP       equ 0
X64_CPU_USER_RSP         equ 8
X64_CPU_KERNEL_RESUME_RSP equ 16

X64_SYSCALL_FRAME_USER_RSP equ 0
X64_SYSCALL_FRAME_R15      equ 8
X64_SYSCALL_FRAME_RAX      equ 104

X64_SYSCALL_EXIT_SENTINEL_LOW  equ 0
X64_SYSCALL_EXIT_SENTINEL_HIGH equ 0x80000000
