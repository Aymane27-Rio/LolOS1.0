BITS 64

%include "arch/x86_64/syscall_abi.inc"

extern x64_syscall_dispatch

global x64_syscall_entry
global x64_enter_user

section .text
x64_syscall_entry:
    ; SYSCALL does not switch RSP. Swap to the kernel GS area before touching
    ; memory, preserve user RSP, and move to the guarded privilege stack.
    swapgs
    mov [gs:X64_CPU_USER_RSP], rsp
    mov rsp, [gs:X64_CPU_KERNEL_RSP]

    push r11
    push rcx
    push rax
    push rdi
    push rsi
    push rdx
    push r10
    push r8
    push r9
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    push qword [gs:X64_CPU_USER_RSP]

    ; Kernel C must not inherit untrusted user DS/ES selectors.
    mov ax, 0x10
    mov ds, ax
    mov es, ax

    mov rdi, rsp
    call x64_syscall_dispatch

    mov rdx, 0x8000000000000000
    cmp rax, rdx
    je .resume_kernel

    ; The C dispatcher validates the return RIP/RSP. Preserve its result in
    ; the saved user RAX slot, then restore every other general register.
    mov [rsp + X64_SYSCALL_FRAME_RAX], rax
    add rsp, 8
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    pop r9
    pop r8
    pop r10
    pop rdx
    pop rsi
    pop rdi
    ; Load user data selectors before restoring the syscall result in RAX.
    ; AX is part of RAX, so doing this afterward corrupts negative errno values.
    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    pop rax
    pop rcx
    pop r11

    ; SYSRET must never restore privileged or interrupt-control flags from
    ; user state. Retain only arithmetic condition flags and the fixed bit.
    and r11, 0x8D5
    or r11, 0x2
    mov rsp, [gs:X64_CPU_USER_RSP]
    swapgs
    o64 sysret

.resume_kernel:
    ; exit() returns to the kernel caller saved by x64_enter_user().
    mov rsp, [gs:X64_CPU_KERNEL_RESUME_RSP]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

x64_enter_user:
    ; Preserve the C ABI's callee-saved registers so exit() can resume here.
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov [gs:X64_CPU_KERNEL_RESUME_RSP], rsp

    ; Establish a flat ring-3 data environment and an IF=0 initial RFLAGS.
    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    push qword 0x1B
    push rsi
    push qword 0x2
    push qword 0x23
    push rdi
    swapgs
    iretq

section .note.GNU-stack noalloc noexec nowrite progbits
