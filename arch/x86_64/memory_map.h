#ifndef LOLOS_X86_64_MEMORY_MAP_H
#define LOLOS_X86_64_MEMORY_MAP_H

#include <stdint.h>

#define X64_MEMORY_MAP_MAX_ENTRIES 256u
#define X64_USABLE_RANGE_MAX_ENTRIES (X64_MEMORY_MAP_MAX_ENTRIES * 2u + 8u)
#define X64_BOOT_MODULE_MAX_ENTRIES 256u

struct x64_memory_region {
    uint64_t base;
    uint64_t length;
    uint32_t type;
};

struct x64_usable_range {
    uint64_t base;
    uint64_t length;
};

struct x64_boot_module_range {
    uint64_t base;
    uint64_t length;
};

/* Raw, validated Multiboot 2 entries; not yet an allocatable frame list. */
extern struct x64_memory_region
    x64_memory_map_entries[X64_MEMORY_MAP_MAX_ENTRIES];
extern uint32_t x64_memory_map_entry_count;

/* Page-aligned ranges after reserved regions have been removed. */
extern struct x64_usable_range
    x64_usable_ranges[X64_USABLE_RANGE_MAX_ENTRIES];
extern uint32_t x64_usable_range_count;

extern struct x64_boot_module_range
    x64_boot_module_ranges[X64_BOOT_MODULE_MAX_ENTRIES];
extern uint32_t x64_boot_module_range_count;

void x64_memory_map_reset_boot_modules(void);
int x64_memory_map_add_boot_module(uint64_t start, uint64_t end);

int x64_normalize_usable_ranges(uint64_t boot_info_base,
                                uint64_t boot_info_length,
                                uint64_t kernel_base,
                                uint64_t kernel_length);

#endif
