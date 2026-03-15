MBALIGN  equ  1 << 0
MEMINFO  equ  1 << 1
VIDINFO  equ  1 << 2
FLAGS    equ  MBALIGN | MEMINFO | VIDINFO
MAGIC    equ  0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    dd 0, 0, 0, 0, 0
    dd 0
    dd 800
    dd 600
    dd 32

section .bss
align 16
stack_bottom:
    resb 16384 ; Reserve 16 Kilobytes of memory for our stack
stack_top:

section .text
global _start
extern kmain

_start:
    cli             ; disable interrupts
    mov esp, stack_top ; set the cpu's stack pointer to the new memory
    push ebx
    push eax
    call kmain      ; jump to the C code
    hlt             ; halt the CPU if the C code finishes

global isr32
extern timer_handler

isr32:
    pusha             ; save all general-purpose registers
    call timer_handler ; jump to our C code
    popa              ; restore all registers perfectly
    iretd             ; special 'Interrupt Return' instruction

global isr128
extern syscall_handler

isr128:
    pusha                 ; save all the registers
    
    push edx
    push ecx
    push ebx
    push eax
    
    call syscall_handler  ; jump into the Kernel's C code
    
    add esp, 16           ; clean up the pushed 4 arguments (4 bytes each)
    popa                  ; restore all registers
    iretd                 ; return to user mode