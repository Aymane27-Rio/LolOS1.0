#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "memory_map.h"
#include "serial.h"

struct x64_memory_region
    x64_memory_map_entries[X64_MEMORY_MAP_MAX_ENTRIES];
uint32_t x64_memory_map_entry_count;

void serial_write(const char *text)
{
    (void)text;
}

static void set_region(uint32_t index, uint64_t base, uint64_t length,
                       uint32_t type)
{
    x64_memory_map_entries[index].base = base;
    x64_memory_map_entries[index].length = length;
    x64_memory_map_entries[index].type = type;
}

static void test_merge_and_reservations(void)
{
    static const struct x64_usable_range expected[] = {
        {0x00200000u, 0x00100000u},
        {0x00301000u, 0x000FF000u},
        {0x00403000u, 0x000FD000u},
        {0x00502000u, 0x012FE000u},
    };
    uint32_t index;

    x64_memory_map_entry_count = 3u;
    set_region(0u, 0u, 0x01000000u, 1u);
    set_region(1u, 0x00800000u, 0x01000000u, 1u);
    set_region(2u, 0x00500123u, 0x00001000u, 2u);
    x64_memory_map_reset_boot_modules();

    assert(x64_memory_map_add_boot_module(0x00400123u,
                                          0x00402122u));
    assert(!x64_memory_map_add_boot_module(0x00600000u,
                                           0x00500000u));
    assert(x64_boot_module_range_count == 1u);
    assert(x64_normalize_usable_ranges(0x00300000u, 0x1000u,
                                       0x00100000u, 0x00100000u));
    assert(x64_usable_range_count ==
           (uint32_t)(sizeof(expected) / sizeof(expected[0])));

    for (index = 0u; index < x64_usable_range_count; index++) {
        assert(x64_usable_ranges[index].base == expected[index].base);
        assert(x64_usable_ranges[index].length == expected[index].length);
    }
}

static void test_overflow_fails_closed(void)
{
    x64_memory_map_reset_boot_modules();
    x64_memory_map_entry_count = 1u;
    set_region(0u, UINT64_MAX - 0x1000u, 0x2000u, 1u);
    assert(!x64_normalize_usable_ranges(0x200000u, 0x1000u,
                                        0x100000u, 0x1000u));

    x64_memory_map_entry_count = 1u;
    set_region(0u, 0x00100000u, 0x00100000u, 1u);
    assert(!x64_normalize_usable_ranges(UINT64_MAX - 0x10u, 0x20u,
                                        0x00100000u, 0x1000u));
    assert(x64_usable_range_count == 0u);
}

int main(void)
{
    test_merge_and_reservations();
    test_overflow_fails_closed();
    puts("x86_64 memory-map tests passed");
    return 0;
}
