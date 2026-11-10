#ifndef LOLOS_X86_64_MEMORY_MAP_H
#define LOLOS_X86_64_MEMORY_MAP_H

#include <stdint.h>

#define X64_MEMORY_MAP_MAX_ENTRIES 256u

struct x64_memory_region {
    uint64_t base;
    uint64_t length;
    uint32_t type;
};

/* Raw, validated Multiboot 2 entries; not yet an allocatable frame list. */
extern struct x64_memory_region
    x64_memory_map_entries[X64_MEMORY_MAP_MAX_ENTRIES];
extern uint32_t x64_memory_map_entry_count;

#endif
