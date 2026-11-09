; GRUB Multiboot 2 enters in 32-bit protected mode. Build minimal page tables,
; enable long mode, then enter a tiny serial-only C kernel.

MB2_MAGIC     equ 0xE85250D6
MB2_ARCH      equ 0
MB2_LENGTH    equ mb2_header_end - mb2_header
MB2_CHECKSUM  equ -(MB2_MAGIC + MB2_ARCH + MB2_LENGTH)

section .multiboot2
align 8
mb2_header:
    dd MB2_MAGIC
    dd MB2_ARCH
    dd MB2_LENGTH
    dd MB2_CHECKSUM
    dw 0                         ; end tag
    dw 0
    dd 8
mb2_header_end:

section .text
bits 32
global _start32
extern x64_kmain

_start32:
    cli
    cld
    mov esp, stack_top
    ; Preserve the Multiboot 2 handoff values before CPUID changes EAX/EBX.
    mov [boot_magic], eax
    mov [boot_info], ebx
    call serial_init

    ; Confirm extended CPUID leaves and long-mode support.
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode
    mov eax, 1                   ; PAE is reported by the standard feature leaf.
    cpuid
    test edx, 1 << 6
    jz .no_long_mode

    ; Identity-map the first GiB with 2 MiB supervisor-only pages.
    mov dword [pml4], pdpt + 0x03
    mov dword [pml4 + 4], 0
    mov dword [pdpt], page_directory + 0x03
    mov dword [pdpt + 4], 0
    mov edi, page_directory
    mov ecx, 512
    xor eax, eax
.map_pages:
    mov edx, eax
    or edx, 0x83                ; Present, writable, 2 MiB page; U/S stays clear.
    mov [edi], edx
    mov dword [edi + 4], 0
    add eax, 0x200000
    add edi, 8
    loop .map_pages

    lgdt [gdt_pointer]
    mov eax, cr4
    or eax, 1 << 5              ; PAE
    mov cr4, eax
    mov eax, pml4
    mov cr3, eax

    mov ecx, 0xC0000080         ; EFER
    rdmsr
    or eax, 1 << 8              ; LME
    wrmsr

    mov eax, cr0
    or eax, 1 << 31             ; Paging (PE is already set by GRUB).
    mov cr0, eax
    jmp 0x08:long_mode_entry

.no_long_mode:
    mov esi, no_long_mode_message
    call serial_puts
.halt:
    cli
    hlt
    jmp .halt

serial_init:
    ; COM1: 115200 baud, 8N1, FIFO enabled.
    mov dx, 0x3F9
    xor al, al
    out dx, al
    mov dx, 0x3FB
    mov al, 0x80
    out dx, al
    mov dx, 0x3F8
    mov al, 0x01
    out dx, al
    mov dx, 0x3F9
    xor al, al
    out dx, al
    mov dx, 0x3FB
    mov al, 0x03
    out dx, al
    mov dx, 0x3FA
    mov al, 0xC7
    out dx, al
    ret

serial_puts:
    lodsb
    test al, al
    jz .done
    mov ah, al
    mov ecx, 100000
.wait:
    mov dx, 0x3FD
    in al, dx
    test al, 0x20
    jnz .send
    loop .wait
    jmp serial_puts
.send:
    mov al, ah
    mov dx, 0x3F8
    out dx, al
    jmp serial_puts
.done:
    ret

section .rodata
align 8
gdt:
    dq 0x0000000000000000
gdt_code64:
    dq 0x00AF9A000000FFFF
gdt_data:
    dq 0x00CF92000000FFFF
gdt_end:
gdt_pointer:
    dw gdt_end - gdt - 1
    dd gdt
no_long_mode_message:
    db "LolOS x86_64: CPU lacks required long mode or PAE support", 13, 10, 0

section .text
bits 64
long_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    xor ax, ax
    mov fs, ax
    mov gs, ax
    lea rsp, [rel stack_top]
    and rsp, -16
    mov edi, [rel boot_magic]
    mov esi, [rel boot_info]
    call x64_kmain
.halt64:
    cli
    hlt
    jmp .halt64

section .bss
align 4096
pml4:
    resb 4096
pdpt:
    resb 4096
page_directory:
    resb 4096
align 4
boot_magic:
    resd 1
boot_info:
    resd 1
alignb 16
stack_bottom:
    resb 16384
stack_top:

section .note.GNU-stack noalloc noexec nowrite progbits
