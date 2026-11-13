#include <stdint.h>

#include "exceptions.h"
#include "serial.h"

#define X64_EXCEPTION_COUNT 32u
#define X64_KERNEL_CODE_SELECTOR 0x08u
#define X64_IDT_INTERRUPT_GATE 0x8Eu

struct x64_idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct x64_descriptor_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct x64_tss {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t io_map_base;
} __attribute__((packed));

struct x64_exception_frame {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
};

typedef void (*x64_exception_stub)(void);

extern x64_exception_stub x64_exception_stub_table[X64_EXCEPTION_COUNT];
extern unsigned char x64_boot_stack_top[];
extern unsigned char __x64_double_fault_stack_top[];
extern unsigned char __x64_nmi_stack_top[];
extern unsigned char __x64_machine_check_stack_top[];

static struct x64_idt_entry x64_idt[X64_EXCEPTION_COUNT]
    __attribute__((aligned(16)));
static struct x64_tss x64_tss __attribute__((aligned(16)));
static uint64_t x64_gdt[5] __attribute__((aligned(16)));
static volatile uint32_t exception_handler_active;

_Static_assert(sizeof(struct x64_tss) == 104u, "invalid x86_64 TSS size");
_Static_assert(sizeof(struct x64_idt_entry) == 16u,
               "invalid x86_64 IDT entry size");

static void set_idt_entry(uint32_t vector, x64_exception_stub handler,
                          uint8_t ist)
{
    struct x64_idt_entry *entry = &x64_idt[vector];
    uint64_t address = (uint64_t)(uintptr_t)handler;

    entry->offset_low = (uint16_t)address;
    entry->selector = X64_KERNEL_CODE_SELECTOR;
    entry->ist = (uint8_t)(ist & 0x07u);
    entry->attributes = X64_IDT_INTERRUPT_GATE;
    entry->offset_middle = (uint16_t)(address >> 16);
    entry->offset_high = (uint32_t)(address >> 32);
    entry->reserved = 0u;
}

static void set_tss_descriptor(void)
{
    uint64_t base = (uint64_t)(uintptr_t)&x64_tss;
    uint64_t limit = sizeof(x64_tss) - 1u;

    x64_gdt[3] = (limit & 0xFFFFu) |
                 ((base & 0xFFFFFFu) << 16) |
                 (0x89ULL << 40) |
                 (((limit >> 16) & 0x0Fu) << 48) |
                 (((base >> 24) & 0xFFu) << 56);
    x64_gdt[4] = base >> 32;
}

void x64_exception_init(void)
{
    struct x64_descriptor_pointer idt_pointer;
    struct x64_descriptor_pointer gdt_pointer;
    uint32_t vector;

    x64_tss.rsp0 = (uint64_t)(uintptr_t)x64_boot_stack_top;
    x64_tss.ist1 = (uint64_t)(uintptr_t)__x64_double_fault_stack_top;
    x64_tss.ist2 = (uint64_t)(uintptr_t)__x64_nmi_stack_top;
    x64_tss.ist3 = (uint64_t)(uintptr_t)__x64_machine_check_stack_top;
    x64_tss.io_map_base = (uint16_t)sizeof(x64_tss);

    x64_gdt[0] = 0u;
    x64_gdt[1] = 0x00AF9B000000FFFFULL;
    x64_gdt[2] = 0x00CF93000000FFFFULL;
    set_tss_descriptor();

    for (vector = 0u; vector < X64_EXCEPTION_COUNT; vector++) {
        set_idt_entry(vector, x64_exception_stub_table[vector], 0u);
    }

    idt_pointer.limit = (uint16_t)(sizeof(x64_idt) - 1u);
    idt_pointer.base = (uint64_t)(uintptr_t)x64_idt;
    gdt_pointer.limit = (uint16_t)(sizeof(x64_gdt) - 1u);
    gdt_pointer.base = (uint64_t)(uintptr_t)x64_gdt;

    /* Install handlers before loading the replacement GDT, without IST use. */
    __asm__ volatile ("lidt %0" : : "m"(idt_pointer) : "memory");
    __asm__ volatile (
        "lgdt %0\n"
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%ss\n"
        "movw $0x18, %%ax\n"
        "ltr %%ax\n"
        :
        : "m"(gdt_pointer)
        : "rax", "memory");

    for (vector = 0u; vector < X64_EXCEPTION_COUNT; vector++) {
        uint8_t ist = 0u;

        if (vector == 8u) {
            ist = 1u;
        } else if (vector == 2u) {
            ist = 2u;
        } else if (vector == 18u) {
            ist = 3u;
        }
        set_idt_entry(vector, x64_exception_stub_table[vector], ist);
    }
    __asm__ volatile ("lidt %0" : : "m"(idt_pointer) : "memory");
}

static const char *exception_name(uint64_t vector)
{
    switch (vector) {
    case 0u: return "divide error (#DE)";
    case 1u: return "debug (#DB)";
    case 2u: return "non-maskable interrupt (#NMI)";
    case 3u: return "breakpoint (#BP)";
    case 4u: return "overflow (#OF)";
    case 5u: return "bound range exceeded (#BR)";
    case 6u: return "invalid opcode (#UD)";
    case 7u: return "device not available (#NM)";
    case 8u: return "double fault (#DF)";
    case 9u: return "coprocessor segment overrun";
    case 10u: return "invalid TSS (#TS)";
    case 11u: return "segment not present (#NP)";
    case 12u: return "stack-segment fault (#SS)";
    case 13u: return "general protection fault (#GP)";
    case 14u: return "page fault (#PF)";
    case 16u: return "x87 floating-point exception (#MF)";
    case 17u: return "alignment check (#AC)";
    case 18u: return "machine check (#MC)";
    case 19u: return "SIMD floating-point exception (#XM)";
    case 20u: return "virtualization exception (#VE)";
    case 21u: return "control protection exception (#CP)";
    case 29u: return "VMM communication exception (#VC)";
    case 30u: return "security exception (#SX)";
    default: return "reserved or unknown exception";
    }
}

static void halt_after_exception(void) __attribute__((noreturn));

static void halt_after_exception(void)
{
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void x64_exception_dispatch(struct x64_exception_frame *frame)
    __attribute__((noreturn));

void x64_exception_dispatch(struct x64_exception_frame *frame)
{
    uint64_t fault_address = 0u;

    if (exception_handler_active != 0u) {
        halt_after_exception();
    }
    exception_handler_active = 1u;

    if (frame->vector == 14u) {
        __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_address));
    }

    serial_write("x86_64 exception: ");
    serial_write(exception_name(frame->vector));
    serial_write("\nvector=");
    serial_write_u32((uint32_t)frame->vector);
    serial_write(" error=");
    serial_write_hex64(frame->error_code);
    serial_write(" RIP=");
    serial_write_hex64(frame->rip);
    serial_write(" CS=");
    serial_write_hex64(frame->cs);
    serial_write(" RFLAGS=");
    serial_write_hex64(frame->rflags);
    serial_write("\n");

    if (frame->vector == 14u) {
        serial_write("CR2=");
        serial_write_hex64(fault_address);
        serial_write("\n");
    }

    serial_write("CPU halted after exception.\n");
    halt_after_exception();
}
