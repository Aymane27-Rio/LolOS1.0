#include "serial.h"

#define COM1 0x3F8u
#define COM1_INTERRUPT_ENABLE (COM1 + 1u)
#define COM1_LINE_CONTROL (COM1 + 3u)
#define COM1_LINE_STATUS (COM1 + 5u)

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

void serial_write_char(char character)
{
    while ((inb(COM1_LINE_STATUS) & 0x20u) == 0u) {
        __asm__ volatile ("pause");
    }
    outb(COM1, (uint8_t)character);
}

void serial_init(void)
{
    outb(COM1_INTERRUPT_ENABLE, 0x00u);
    outb(COM1_LINE_CONTROL, 0x80u);
    outb(COM1, 0x01u);
    outb(COM1_INTERRUPT_ENABLE, 0x00u);
    outb(COM1_LINE_CONTROL, 0x03u);
    outb(COM1 + 2u, 0x00u);
}

void serial_write(const char *text)
{
    while (*text != '\0') {
        if (*text == '\n') {
            serial_write_char('\r');
        }
        serial_write_char(*text++);
    }
}

void serial_write_hex64(uint64_t value)
{
    static const char digits[] = "0123456789abcdef";
    int shift;

    serial_write("0x");
    for (shift = 60; shift >= 0; shift -= 4) {
        serial_write_char(digits[(value >> (uint32_t)shift) & 0x0Fu]);
    }
}

void serial_write_u32(uint32_t value)
{
    char digits[10];
    uint32_t count = 0;

    do {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u);

    while (count != 0u) {
        serial_write_char(digits[--count]);
    }
}

char serial_read_char(void)
{
    while ((inb(COM1_LINE_STATUS) & 0x01u) == 0u) {
        __asm__ volatile ("pause");
    }
    return (char)inb(COM1);
}
