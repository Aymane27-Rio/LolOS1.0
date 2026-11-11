#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "memory_map.h"
#include "serial.h"

#define CONSOLE_LINE_CAPACITY 128u

static int swallow_line_feed;

static int is_space(char character)
{
    return character == ' ' || character == '\t';
}

static char *trim_line(char *line)
{
    char *end;

    while (is_space(*line)) {
        line++;
    }
    end = line;
    while (*end != '\0') {
        end++;
    }
    while (end > line && is_space(end[-1])) {
        *--end = '\0';
    }
    return line;
}

static int match_command(const char *line, const char *name,
                         const char **arguments)
{
    while (*name != '\0' && *line == *name) {
        line++;
        name++;
    }
    if (*name != '\0' || (*line != '\0' && !is_space(*line))) {
        return 0;
    }
    while (is_space(*line)) {
        line++;
    }
    *arguments = line;
    return 1;
}

static int read_line(char *buffer, size_t capacity)
{
    size_t length = 0;
    int overflowed = 0;

    for (;;) {
        char character = serial_read_char();

        if (swallow_line_feed) {
            swallow_line_feed = 0;
            if (character == '\n') {
                continue;
            }
        }

        if (character == '\r' || character == '\n') {
            if (character == '\r') {
                swallow_line_feed = 1;
            }
            serial_write("\n");
            buffer[length] = '\0';
            if (overflowed) {
                serial_write("Input too long; command discarded.\n");
                return 2;
            }
            return 0;
        }
        if (character == 0x03) {
            buffer[0] = '\0';
            serial_write("^C\n");
            return 1;
        }
        if (overflowed) {
            continue;
        }
        if (character == '\b' || (uint8_t)character == 0x7Fu) {
            if (length != 0u) {
                length--;
                serial_write("\b \b");
            }
            continue;
        }
        if ((uint8_t)character < 0x20u || (uint8_t)character > 0x7Eu) {
            continue;
        }
        if (length + 1u < capacity) {
            buffer[length++] = character;
            serial_write_char(character);
        } else {
            overflowed = 1;
            serial_write_char('\a');
        }
    }
}

static const char *memory_type_name(uint32_t type)
{
    switch (type) {
    case 1u: return "available";
    case 2u: return "reserved";
    case 3u: return "ACPI reclaimable";
    case 4u: return "ACPI NVS";
    case 5u: return "bad memory";
    default: return "unknown";
    }
}

static void show_memory_map(void)
{
    uint32_t index;

    serial_write("Raw Multiboot memory map entries: ");
    serial_write_u32(x64_memory_map_entry_count);
    serial_write("\n");
    for (index = 0; index < x64_memory_map_entry_count; index++) {
        const struct x64_memory_region *region =
            &x64_memory_map_entries[index];

        serial_write("[");
        serial_write_u32(index);
        serial_write("] base=");
        serial_write_hex64(region->base);
        serial_write(" length=");
        serial_write_hex64(region->length);
        serial_write(" type=");
        serial_write_u32(region->type);
        serial_write(" (");
        serial_write(memory_type_name(region->type));
        serial_write(")\n");
    }
    serial_write("These raw ranges are not yet normalized or used for allocation.\n");
}

static void execute_command(char *line)
{
    const char *arguments;

    line = trim_line(line);
    if (*line == '\0') {
        return;
    }
    if (match_command(line, "help", &arguments)) {
        if (*arguments != '\0') {
            serial_write("Usage: help\n");
            return;
        }
        serial_write("Commands:\n"
                     "  help          Show this command list\n"
                     "  info          Show checkpoint status\n"
                     "  memmap        List validated raw memory ranges\n"
                     "  echo TEXT     Print TEXT\n"
                     "  clear         Clear the serial terminal\n"
                     "  halt          Halt the CPU\n");
        return;
    }
    if (match_command(line, "info", &arguments)) {
        if (*arguments != '\0') {
            serial_write("Usage: info\n");
            return;
        }
        serial_write("LolOS x86_64 long-mode migration console\n"
                     "Input/output: polled COM1 serial\n"
                     "Memory-map entries retained: ");
        serial_write_u32(x64_memory_map_entry_count);
        serial_write("\nThis kernel runs in ring 0 and has no user isolation.\n");
        return;
    }
    if (match_command(line, "memmap", &arguments)) {
        if (*arguments != '\0') {
            serial_write("Usage: memmap\n");
            return;
        }
        show_memory_map();
        return;
    }
    if (match_command(line, "echo", &arguments)) {
        serial_write(arguments);
        serial_write("\n");
        return;
    }
    if (match_command(line, "clear", &arguments)) {
        if (*arguments != '\0') {
            serial_write("Usage: clear\n");
            return;
        }
        serial_write("\033[2J\033[H");
        return;
    }
    if (match_command(line, "halt", &arguments)) {
        if (*arguments != '\0') {
            serial_write("Usage: halt\n");
            return;
        }
        serial_write("CPU halted. Reset the VM to continue.\n");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    serial_write("Unknown command: ");
    serial_write(line);
    serial_write(". Type 'help' for available commands.\n");
}

void x64_console_run(void)
{
    char line[CONSOLE_LINE_CAPACITY];

    serial_write("Type 'help' to see available commands.\n");
    for (;;) {
        serial_write("lolos> ");
        if (read_line(line, sizeof(line)) == 0) {
            execute_command(line);
        }
    }
}
