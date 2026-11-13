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
global x64_boot_stack_top
extern x64_kmain
extern __kernel_text_start
extern __kernel_text_end
extern __kernel_rodata_start
extern __kernel_rodata_end
extern __kernel_writable_start
extern __kernel_end

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
    test edx, 1 << 20          ; NX is required for writable, non-executable RAM.
    jz .no_long_mode
    mov eax, 1                   ; PAE is reported by the standard feature leaf.
    cpuid
    test edx, 1 << 6
    jz .no_long_mode

    ; Use 4 KiB pages for the kernel's first 2 MiB so linker sections can
    ; receive distinct permissions. Map the rest with supervisor RW+NX pages.
    mov dword [pml4], pdpt + 0x03
    mov dword [pml4 + 4], 0
    mov dword [pdpt], page_directory + 0x03
    mov dword [pdpt + 4], 0
    mov dword [page_directory], page_table + 0x03
    mov dword [page_directory + 4], 0

    mov edi, page_table
    mov ecx, 512
    xor eax, eax
.map_kernel_pages:
    ; Default low memory and the boot handoff area to supervisor RW+NX.
    mov edx, eax
    or edx, 0x03
    mov ebx, 0x80000000

    ; Kernel text is supervisor read-only and executable.
    cmp eax, __kernel_text_start
    jb .check_writable
    cmp eax, __kernel_text_end
    jae .check_writable
    mov edx, eax
    or edx, 0x01
    xor ebx, ebx
    jmp .store_kernel_page

.check_writable:
    ; Read-only data is supervisor read-only and non-executable.
    cmp eax, __kernel_rodata_start
    jb .check_writable_data
    cmp eax, __kernel_rodata_end
    jae .check_writable_data
    mov edx, eax
    or edx, 0x01
    jmp .store_kernel_page

.check_writable_data:
    ; Writable data, BSS, page tables, and stack remain RW+NX.
    cmp eax, __kernel_writable_start
    jb .store_kernel_page
    cmp eax, __kernel_end
    jae .store_kernel_page
    mov edx, eax
    or edx, 0x03

.store_kernel_page:
    mov [edi], edx
    mov [edi + 4], ebx
    add eax, 0x1000
    add edi, 8
    loop .map_kernel_pages

    mov edi, page_directory
    add edi, 8
    mov ecx, 511
    mov eax, 0x00200000
.map_remaining_pages:
    mov edx, eax
    or edx, 0x83                ; Present, writable, 2 MiB page; supervisor only.
    mov [edi], edx
    mov dword [edi + 4], 0x80000000 ; NX
    add eax, 0x200000
    add edi, 8
    loop .map_remaining_pages

    lgdt [gdt_pointer]
    mov eax, cr4
    or eax, 1 << 5              ; PAE
    mov cr4, eax
    mov eax, pml4
    mov cr3, eax

    mov ecx, 0xC0000080         ; EFER
    rdmsr
    or eax, (1 << 11) | (1 << 8) ; NXE and LME
    wrmsr

    mov eax, cr0
    or eax, (1 << 31) | (1 << 16) ; Paging and supervisor write protection.
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
    ; COM1: 115200 baud, 8N1, FIFO disabled for predictable polling.
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
    xor al, al
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
    dq 0x00AF9B000000FFFF     ; Accessed bit is pre-set for the read-only GDT.
gdt_data:
    dq 0x00CF93000000FFFF     ; Accessed bit is pre-set for the read-only GDT.
gdt_end:
gdt_pointer:
    dw gdt_end - gdt - 1
    dd gdt
no_long_mode_message:
    db "LolOS x86_64: CPU lacks long mode, PAE, or NX support", 13, 10, 0

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
page_table:
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
x64_boot_stack_top:

section .note.GNU-stack noalloc noexec nowrite progbits
