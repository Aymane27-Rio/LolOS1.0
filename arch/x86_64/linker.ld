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
    .text ALIGN(4K) : { *(.text .text.*) } :text
    .rodata ALIGN(4K) : { *(.rodata .rodata.*) } :rodata
    .data ALIGN(4K) : { *(.data .data.*) } :data
    .bss ALIGN(4K) (NOLOAD) : {
        *(COMMON)
        *(.bss .bss.*)
    } :data
    /DISCARD/ : { *(.comment) *(.eh_frame) *(.note .note.*) }
}
