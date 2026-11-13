section .text
bits 64

extern x64_exception_dispatch

%macro ISR_NO_ERROR 1
global x64_isr_%1
x64_isr_%1:
    push qword 0
    push qword %1
    jmp x64_exception_common
%endmacro

%macro ISR_ERROR 1
global x64_isr_%1
x64_isr_%1:
    push qword %1
    jmp x64_exception_common
%endmacro

ISR_NO_ERROR 0
ISR_NO_ERROR 1
ISR_NO_ERROR 2
ISR_NO_ERROR 3
ISR_NO_ERROR 4
ISR_NO_ERROR 5
ISR_NO_ERROR 6
ISR_NO_ERROR 7
ISR_ERROR    8
ISR_NO_ERROR 9
ISR_ERROR    10
ISR_ERROR    11
ISR_ERROR    12
ISR_ERROR    13
ISR_ERROR    14
ISR_NO_ERROR 15
ISR_NO_ERROR 16
ISR_ERROR    17
ISR_NO_ERROR 18
ISR_NO_ERROR 19
ISR_NO_ERROR 20
ISR_ERROR    21
ISR_NO_ERROR 22
ISR_NO_ERROR 23
ISR_NO_ERROR 24
ISR_NO_ERROR 25
ISR_NO_ERROR 26
ISR_NO_ERROR 27
ISR_NO_ERROR 28
ISR_ERROR    29
ISR_ERROR    30
ISR_NO_ERROR 31

x64_exception_common:
    cld
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    and rsp, -16
    call x64_exception_dispatch

.halt:
    cli
    hlt
    jmp .halt

section .rodata
align 8
global x64_exception_stub_table
x64_exception_stub_table:
    dq x64_isr_0,  x64_isr_1,  x64_isr_2,  x64_isr_3
    dq x64_isr_4,  x64_isr_5,  x64_isr_6,  x64_isr_7
    dq x64_isr_8,  x64_isr_9,  x64_isr_10, x64_isr_11
    dq x64_isr_12, x64_isr_13, x64_isr_14, x64_isr_15
    dq x64_isr_16, x64_isr_17, x64_isr_18, x64_isr_19
    dq x64_isr_20, x64_isr_21, x64_isr_22, x64_isr_23
    dq x64_isr_24, x64_isr_25, x64_isr_26, x64_isr_27
    dq x64_isr_28, x64_isr_29, x64_isr_30, x64_isr_31

section .note.GNU-stack noalloc noexec nowrite progbits
