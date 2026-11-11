OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start32)

PHDRS
{
    multiboot PT_LOAD FLAGS(4);
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 1M;
    .multiboot2 ALIGN(8) : { KEEP(*(.multiboot2)) } :multiboot
    .text ALIGN(4K) : {
        __kernel_text_start = .;
        *(.text .text.*)
        __kernel_text_end = .;
    } :text
    .rodata ALIGN(4K) : {
        __kernel_rodata_start = .;
        *(.rodata .rodata.*)
        __kernel_rodata_end = .;
    } :rodata
    .data ALIGN(4K) : {
        __kernel_writable_start = .;
        *(.data .data.*)
    } :data
    .bss ALIGN(4K) (NOLOAD) : {
        *(COMMON)
        *(.bss .bss.*)
        __kernel_end = .;
    } :data
    ASSERT(__kernel_end <= 0x200000,
           "x86_64 bootstrap kernel exceeds its protected 2 MiB mapping")
    /DISCARD/ : { *(.comment) *(.eh_frame) *(.note .note.*) }
}
