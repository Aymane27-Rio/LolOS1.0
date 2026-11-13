#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "memory_map.h"
#include "pmm.h"
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

static int string_equals(const char *left, const char *right)
{
    while (*left != '\0' && *left == *right) {
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
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
    serial_write("Raw ranges may overlap; use 'memmap usable' for normalized ranges.\n");
}

static void show_usable_ranges(void)
{
    uint32_t index;

    serial_write("Normalized usable page ranges after reservations: ");
    serial_write_u32(x64_usable_range_count);
    serial_write("\n");
    for (index = 0u; index < x64_usable_range_count; index++) {
        const struct x64_usable_range *range = &x64_usable_ranges[index];

        serial_write("[");
        serial_write_u32(index);
        serial_write("] base=");
        serial_write_hex64(range->base);
        serial_write(" length=");
        serial_write_hex64(range->length);
        serial_write("\n");
    }
    serial_write("The allocator uses ranges below 1 GiB; page tables are still bootstrapped separately.\n");
}

static void show_boot_modules(void)
{
    uint32_t index;

    serial_write("Boot modules reserved: ");
    serial_write_u32(x64_boot_module_range_count);
    serial_write("\n");
    for (index = 0u; index < x64_boot_module_range_count; index++) {
        const struct x64_boot_module_range *module =
            &x64_boot_module_ranges[index];

        serial_write("module[");
        serial_write_u32(index);
        serial_write("] base=");
        serial_write_hex64(module->base);
        serial_write(" length=");
        serial_write_hex64(module->length);
        serial_write("\n");
    }
}

static void show_page_allocator(void)
{
    struct x64_pmm_stats stats;

    x64_pmm_get_stats(&stats);
    serial_write("Physical page allocator: initialized\n"
                 "Identity-mapped allocation limit: ");
    serial_write_hex64(X64_PMM_IDENTITY_LIMIT);
    serial_write("\nManaged pages: ");
    serial_write_u32(stats.managed_pages);
    serial_write("\nFree pages: ");
    serial_write_u32(stats.free_pages);
    serial_write("\nAllocated pages: ");
    serial_write_u32(stats.allocated_pages);
    serial_write("\nNew allocations are zeroed; allocator is single-CPU only.\n");
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
                     "  memmap [usable] List raw or normalized usable ranges\n"
                     "  modules       List reserved Multiboot modules\n"
                     "  pmm [selftest] Show or test the physical page allocator\n"
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
        serial_write("Boot modules reserved: ");
        serial_write_u32(x64_boot_module_range_count);
        serial_write("\n");
        serial_write("Normalized usable page ranges: ");
        serial_write_u32(x64_usable_range_count);
        serial_write("\n");
        {
            struct x64_pmm_stats stats;

            x64_pmm_get_stats(&stats);
            serial_write("Free physical pages: ");
            serial_write_u32(stats.free_pages);
            serial_write("\n");
        }
        return;
    }
    if (match_command(line, "memmap", &arguments)) {
        if (*arguments == '\0') {
            show_memory_map();
        } else if (string_equals(arguments, "usable")) {
            show_usable_ranges();
        } else {
            serial_write("Usage: memmap [usable]\n");
            return;
        }
        return;
    }
    if (match_command(line, "modules", &arguments)) {
        if (*arguments != '\0') {
            serial_write("Usage: modules\n");
            return;
        }
        show_boot_modules();
        return;
    }
    if (match_command(line, "pmm", &arguments)) {
        if (*arguments == '\0') {
            show_page_allocator();
        } else if (string_equals(arguments, "selftest")) {
            if (x64_pmm_self_test()) {
                serial_write("PMM self-test passed; pages returned and reallocation zeroed.\n");
            } else {
                serial_write("PMM self-test failed.\n");
            }
        } else {
            serial_write("Usage: pmm [selftest]\n");
            return;
        }
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
