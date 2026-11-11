#ifndef LOLOS_X86_64_SERIAL_H
#define LOLOS_X86_64_SERIAL_H

#include <stdint.h>

void serial_init(void);
void serial_write_char(char character);
void serial_write(const char *text);
void serial_write_hex64(uint64_t value);
void serial_write_u32(uint32_t value);
char serial_read_char(void);

#endif
