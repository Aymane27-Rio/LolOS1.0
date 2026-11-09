#include <stdint.h>

#define COM1 0x3F8u

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port) : "memory");
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port) : "memory");
    return value;
}

static void serial_putc(char character)
{
    uint32_t timeout = 1000000;
    while ((inb(COM1 + 5) & 0x20u) == 0 && --timeout != 0) {
        __asm__ volatile ("pause");
    }
    if (timeout != 0) {
        outb(COM1, (uint8_t)character);
    }
}

static void serial_write(const char *text)
{
    while (*text != '\0') {
        if (*text == '\n') {
            serial_putc('\r');
        }
        serial_putc(*text++);
    }
}

void x64_kmain(void)
{
    serial_write("LolOS x86_64: long mode entered. Supervisor-only bootstrap map active.\n");
    serial_write("No user code or interrupts are enabled in this migration checkpoint.\n");

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
