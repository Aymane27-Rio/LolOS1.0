OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start32)

PHDRS
{
    multiboot PT_LOAD FLAGS(4);
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    usertext PT_LOAD FLAGS(5);
    userrodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 1M;
    __kernel_start = .;
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
    .user_test_text ALIGN(4K) : {
        __x64_user_test_text_start = .;
        *(.user_test_text .user_test_text.*)
        __x64_user_test_text_end = .;
    } :usertext
    .user_test_rodata ALIGN(4K) : {
        __x64_user_test_rodata_start = .;
        *(.user_test_rodata .user_test_rodata.*)
        __x64_user_test_rodata_end = .;
    } :userrodata
    .data ALIGN(4K) : {
        __kernel_writable_start = .;
        *(.data .data.*)
    } :data
    .bss ALIGN(4K) (NOLOAD) : {
        *(COMMON)
        *(.bss .bss.*)
    } :data
    .exception_stacks ALIGN(4K) (NOLOAD) : {
        __x64_double_fault_guard_page = .;
        . += 0x1000;
        __x64_double_fault_stack_bottom = .;
        . += 0x1000;
        __x64_double_fault_stack_top = .;

        __x64_nmi_guard_page = .;
        . += 0x1000;
        __x64_nmi_stack_bottom = .;
        . += 0x1000;
        __x64_nmi_stack_top = .;

        __x64_machine_check_guard_page = .;
        . += 0x1000;
        __x64_machine_check_stack_bottom = .;
        . += 0x1000;
        __x64_machine_check_stack_top = .;

        __x64_syscall_stack_guard_page = .;
        . += 0x1000;
        __x64_syscall_stack_bottom = .;
        . += 0x4000;
        __x64_syscall_stack_top = .;

        __kernel_end = .;
    } :data
    ASSERT(__x64_user_test_text_end > __x64_user_test_text_start &&
           __x64_user_test_text_end - __x64_user_test_text_start <= 0x1000,
           "x86_64 ring-3 test text must fit in one page")
    ASSERT(__x64_user_test_rodata_end > __x64_user_test_rodata_start &&
           __x64_user_test_rodata_end - __x64_user_test_rodata_start <= 0x1000,
           "x86_64 ring-3 test rodata must fit in one page")
    ASSERT(__kernel_end <= 0x200000,
           "x86_64 bootstrap kernel exceeds its protected 2 MiB mapping")
    /DISCARD/ : { *(.comment) *(.eh_frame) *(.note .note.*) }
}
