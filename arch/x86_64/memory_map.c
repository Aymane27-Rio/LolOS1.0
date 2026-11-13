#include <stdint.h>

#include "memory_map.h"
#include "serial.h"

#define PAGE_SIZE 4096u
#define PAGE_MASK ((uint64_t)PAGE_SIZE - 1u)
#define LOW_MEMORY_END 0x100000u
#define MAX_EXCLUSIONS (X64_MEMORY_MAP_MAX_ENTRIES + \
                        X64_BOOT_MODULE_MAX_ENTRIES + 3u)

struct interval {
    uint64_t start;
    uint64_t end;
};

struct x64_usable_range
    x64_usable_ranges[X64_USABLE_RANGE_MAX_ENTRIES];
uint32_t x64_usable_range_count;
struct x64_boot_module_range
    x64_boot_module_ranges[X64_BOOT_MODULE_MAX_ENTRIES];
uint32_t x64_boot_module_range_count;

static struct interval available_intervals[X64_MEMORY_MAP_MAX_ENTRIES];
static struct interval exclusion_intervals[MAX_EXCLUSIONS];

void x64_memory_map_reset_boot_modules(void)
{
    x64_boot_module_range_count = 0u;
}

int x64_memory_map_add_boot_module(uint64_t start, uint64_t end)
{
    struct x64_boot_module_range *module;

    if (end < start) {
        return 0;
    }
    if (start == end) {
        return 1;
    }
    if (x64_boot_module_range_count >= X64_BOOT_MODULE_MAX_ENTRIES) {
        return 0;
    }
    module = &x64_boot_module_ranges[x64_boot_module_range_count++];
    module->base = start;
    module->length = end - start;
    return 1;
}

static uint64_t align_up_saturating(uint64_t value)
{
    uint64_t remainder = value & PAGE_MASK;

    if (remainder == 0u) {
        return value;
    }
    if (value > UINT64_MAX - (PAGE_SIZE - remainder)) {
        return UINT64_MAX;
    }
    return value + PAGE_SIZE - remainder;
}

static int append_interval(struct interval *intervals, uint32_t capacity,
                           uint32_t *count, uint64_t start, uint64_t end)
{
    if (start >= end) {
        return 1;
    }
    if (*count >= capacity) {
        return 0;
    }
    intervals[*count].start = start;
    intervals[*count].end = end;
    (*count)++;
    return 1;
}

static uint32_t sort_and_merge(struct interval *intervals, uint32_t count)
{
    uint32_t index;
    uint32_t merged_count;

    for (index = 1u; index < count; index++) {
        struct interval current = intervals[index];
        uint32_t position = index;

        while (position != 0u &&
               intervals[position - 1u].start > current.start) {
            intervals[position] = intervals[position - 1u];
            position--;
        }
        intervals[position] = current;
    }

    if (count == 0u) {
        return 0u;
    }
    merged_count = 1u;
    for (index = 1u; index < count; index++) {
        struct interval *previous = &intervals[merged_count - 1u];
        struct interval current = intervals[index];

        if (current.start <= previous->end) {
            if (current.end > previous->end) {
                previous->end = current.end;
            }
        } else {
            intervals[merged_count++] = current;
        }
    }
    return merged_count;
}

static int add_outward_aligned_exclusion(uint64_t base, uint64_t length,
                                         uint32_t *count)
{
    uint64_t end;

    if (length == 0u) {
        return 1;
    }
    if (length > UINT64_MAX - base) {
        return 0;
    }
    end = align_up_saturating(base + length);
    return append_interval(exclusion_intervals, MAX_EXCLUSIONS, count,
                           base & ~PAGE_MASK, end);
}

static int append_normalized_range(uint64_t start, uint64_t end)
{
    if (start >= end) {
        return 1;
    }
    if (x64_usable_range_count >= X64_USABLE_RANGE_MAX_ENTRIES) {
        return 0;
    }
    x64_usable_ranges[x64_usable_range_count].base = start;
    x64_usable_ranges[x64_usable_range_count].length = end - start;
    x64_usable_range_count++;
    return 1;
}

int x64_normalize_usable_ranges(uint64_t boot_info_base,
                                uint64_t boot_info_length,
                                uint64_t kernel_base,
                                uint64_t kernel_length)
{
    uint32_t available_count = 0u;
    uint32_t exclusion_count = 0u;
    uint32_t index;

    x64_usable_range_count = 0u;

    for (index = 0u; index < x64_memory_map_entry_count; index++) {
        const struct x64_memory_region *region =
            &x64_memory_map_entries[index];
        uint64_t end;

        if (region->length == 0u) {
            continue;
        }
        if (region->length > UINT64_MAX - region->base) {
            serial_write("Memory map normalization: overflowing raw range\n");
            return 0;
        }
        end = region->base + region->length;

        if (region->type == 1u) {
            uint64_t start;

            if (region->base > UINT64_MAX - PAGE_MASK) {
                continue;
            }
            start = (region->base + PAGE_MASK) & ~PAGE_MASK;
            end &= ~PAGE_MASK;
            if (!append_interval(available_intervals,
                                 X64_MEMORY_MAP_MAX_ENTRIES,
                                 &available_count, start, end)) {
                serial_write("Memory map normalization: too many available ranges\n");
                return 0;
            }
        } else if (!add_outward_aligned_exclusion(region->base,
                                                  region->length,
                                                  &exclusion_count)) {
            serial_write("Memory map normalization: too many or invalid exclusions\n");
            return 0;
        }
    }

    for (index = 0u; index < x64_boot_module_range_count; index++) {
        const struct x64_boot_module_range *module =
            &x64_boot_module_ranges[index];

        if (!add_outward_aligned_exclusion(module->base, module->length,
                                           &exclusion_count)) {
            serial_write("Memory map normalization: invalid boot module reservation\n");
            return 0;
        }
    }

    available_count = sort_and_merge(available_intervals, available_count);
    if (!append_interval(exclusion_intervals, MAX_EXCLUSIONS,
                         &exclusion_count, 0u, LOW_MEMORY_END) ||
        !add_outward_aligned_exclusion(kernel_base, kernel_length,
                                       &exclusion_count) ||
        !add_outward_aligned_exclusion(boot_info_base, boot_info_length,
                                       &exclusion_count)) {
        serial_write("Memory map normalization: reservation capacity exceeded\n");
        return 0;
    }
    exclusion_count = sort_and_merge(exclusion_intervals, exclusion_count);

    for (index = 0u; index < available_count; index++) {
        const struct interval available = available_intervals[index];
        uint64_t cursor = available.start;
        uint32_t exclusion_index;

        for (exclusion_index = 0u; exclusion_index < exclusion_count;
             exclusion_index++) {
            const struct interval exclusion =
                exclusion_intervals[exclusion_index];

            if (exclusion.end <= cursor) {
                continue;
            }
            if (exclusion.start >= available.end) {
                break;
            }
            if (exclusion.start > cursor &&
                !append_normalized_range(cursor, exclusion.start)) {
                serial_write("Memory map normalization: usable range capacity exceeded\n");
                x64_usable_range_count = 0u;
                return 0;
            }
            if (exclusion.end > cursor) {
                cursor = exclusion.end;
            }
            if (cursor >= available.end) {
                break;
            }
        }

        if (cursor < available.end &&
            !append_normalized_range(cursor, available.end)) {
            serial_write("Memory map normalization: usable range capacity exceeded\n");
            x64_usable_range_count = 0u;
            return 0;
        }
    }

    if (x64_usable_range_count == 0u) {
        serial_write("Memory map normalization: no usable page ranges remain\n");
        return 0;
    }
    return 1;
}
