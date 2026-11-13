#include <stdint.h>

#include "memory_map.h"
#include "pmm.h"

#define X64_PMM_BITMAP_BYTES ((X64_PMM_PAGE_COUNT + 7u) / 8u)

static uint8_t managed_bitmap[X64_PMM_BITMAP_BYTES];
static uint8_t allocated_bitmap[X64_PMM_BITMAP_BYTES];
static uint32_t managed_page_count;
static uint32_t free_page_count;
static uint32_t allocated_page_count;
static uint32_t next_search_page;
static int allocator_initialized;

static int bitmap_test(const uint8_t *bitmap, uint32_t bit)
{
    return (bitmap[bit / 8u] & (uint8_t)(1u << (bit % 8u))) != 0u;
}

static void bitmap_set(uint8_t *bitmap, uint32_t bit)
{
    bitmap[bit / 8u] |= (uint8_t)(1u << (bit % 8u));
}

static void bitmap_clear(uint8_t *bitmap, uint32_t bit)
{
    bitmap[bit / 8u] &= (uint8_t)~(uint8_t)(1u << (bit % 8u));
}

static void bitmap_set_range(uint8_t *bitmap, uint32_t first_bit,
                             uint32_t end_bit)
{
    uint32_t bit = first_bit;

    while (bit < end_bit && (bit & 7u) != 0u) {
        bitmap_set(bitmap, bit);
        bit++;
    }
    while (end_bit - bit >= 8u) {
        bitmap[bit / 8u] = 0xFFu;
        bit += 8u;
    }
    while (bit < end_bit) {
        bitmap_set(bitmap, bit);
        bit++;
    }
}

static void zero_physical_page(uint64_t physical_address)
{
    volatile uint8_t *page =
        (volatile uint8_t *)(uintptr_t)physical_address;
    uint32_t offset;

    for (offset = 0u; offset < X64_PMM_PAGE_SIZE; offset++) {
        page[offset] = 0u;
    }
}

int x64_pmm_init(void)
{
    uint32_t index;
    uint64_t previous_end = 0u;

    allocator_initialized = 0;
    managed_page_count = 0u;
    free_page_count = 0u;
    allocated_page_count = 0u;
    next_search_page = 0u;
    for (index = 0u; index < X64_PMM_BITMAP_BYTES; index++) {
        managed_bitmap[index] = 0u;
        allocated_bitmap[index] = 0u;
    }

    if (x64_usable_range_count == 0u) {
        return 0;
    }

    for (index = 0u; index < x64_usable_range_count; index++) {
        const struct x64_usable_range *range = &x64_usable_ranges[index];
        uint64_t end;
        uint64_t usable_end;
        uint32_t first_page;
        uint32_t end_page;
        uint32_t page_count;

        if (range->length == 0u ||
            (range->base & (X64_PMM_PAGE_SIZE - 1u)) != 0u ||
            (range->length & (X64_PMM_PAGE_SIZE - 1u)) != 0u ||
            range->length > UINT64_MAX - range->base) {
            return 0;
        }
        end = range->base + range->length;
        if (index != 0u && range->base < previous_end) {
            return 0;
        }
        previous_end = end;

        if (range->base >= X64_PMM_IDENTITY_LIMIT) {
            continue;
        }
        usable_end = end < X64_PMM_IDENTITY_LIMIT
            ? end : X64_PMM_IDENTITY_LIMIT;
        first_page = (uint32_t)(range->base / X64_PMM_PAGE_SIZE);
        end_page = (uint32_t)(usable_end / X64_PMM_PAGE_SIZE);
        page_count = end_page - first_page;
        bitmap_set_range(managed_bitmap, first_page, end_page);
        managed_page_count += page_count;
        free_page_count += page_count;
    }

    if (managed_page_count == 0u) {
        return 0;
    }
    allocator_initialized = 1;
    return 1;
}

uint64_t x64_pmm_allocate_page(void)
{
    uint32_t scanned;

    if (!allocator_initialized || free_page_count == 0u) {
        return 0u;
    }
    for (scanned = 0u; scanned < X64_PMM_PAGE_COUNT; scanned++) {
        uint32_t page = next_search_page + scanned;
        uint64_t physical_address;

        if (page >= X64_PMM_PAGE_COUNT) {
            page -= X64_PMM_PAGE_COUNT;
        }
        if (!bitmap_test(managed_bitmap, page) ||
            bitmap_test(allocated_bitmap, page)) {
            continue;
        }

        bitmap_set(allocated_bitmap, page);
        free_page_count--;
        allocated_page_count++;
        next_search_page = page + 1u;
        if (next_search_page == X64_PMM_PAGE_COUNT) {
            next_search_page = 0u;
        }

        physical_address = (uint64_t)page * X64_PMM_PAGE_SIZE;
        zero_physical_page(physical_address);
        return physical_address;
    }
    return 0u;
}

int x64_pmm_free_page(uint64_t physical_address)
{
    uint32_t page;

    if (!allocator_initialized ||
        (physical_address & (X64_PMM_PAGE_SIZE - 1u)) != 0u ||
        physical_address >= X64_PMM_IDENTITY_LIMIT) {
        return 0;
    }
    page = (uint32_t)(physical_address / X64_PMM_PAGE_SIZE);
    if (!bitmap_test(managed_bitmap, page) ||
        !bitmap_test(allocated_bitmap, page)) {
        return 0;
    }

    bitmap_clear(allocated_bitmap, page);
    allocated_page_count--;
    free_page_count++;
    if (page < next_search_page) {
        next_search_page = page;
    }
    return 1;
}

void x64_pmm_get_stats(struct x64_pmm_stats *stats)
{
    if (stats == 0) {
        return;
    }
    stats->managed_pages = managed_page_count;
    stats->free_pages = free_page_count;
    stats->allocated_pages = allocated_page_count;
}

static int page_is_zero(uint64_t physical_address)
{
    const volatile uint8_t *page =
        (const volatile uint8_t *)(uintptr_t)physical_address;
    uint32_t offset;

    for (offset = 0u; offset < X64_PMM_PAGE_SIZE; offset++) {
        if (page[offset] != 0u) {
            return 0;
        }
    }
    return 1;
}

int x64_pmm_self_test(void)
{
    struct x64_pmm_stats before;
    struct x64_pmm_stats after;
    uint64_t first_page;
    uint64_t second_page;
    uint64_t reused_page;
    volatile uint8_t *first;
    volatile uint8_t *second;
    int first_freed;
    int second_freed;
    int passed = 1;

    if (!allocator_initialized) {
        return 0;
    }
    x64_pmm_get_stats(&before);
    first_page = x64_pmm_allocate_page();
    if (first_page == 0u) {
        return 0;
    }
    second_page = x64_pmm_allocate_page();
    if (second_page == 0u) {
        (void)x64_pmm_free_page(first_page);
        return 0;
    }

    if (first_page == second_page || !page_is_zero(first_page) ||
        !page_is_zero(second_page)) {
        passed = 0;
    }
    first = (volatile uint8_t *)(uintptr_t)first_page;
    second = (volatile uint8_t *)(uintptr_t)second_page;
    first[0] = 0xA5u;
    second[X64_PMM_PAGE_SIZE - 1u] = 0x5Au;

    first_freed = x64_pmm_free_page(first_page);
    second_freed = x64_pmm_free_page(second_page);
    if (!first_freed || !second_freed) {
        passed = 0;
    }

    reused_page = x64_pmm_allocate_page();
    if (reused_page == 0u) {
        passed = 0;
    } else {
        if (!page_is_zero(reused_page)) {
            passed = 0;
        }
        if (!x64_pmm_free_page(reused_page)) {
            passed = 0;
        }
    }
    if (x64_pmm_free_page(first_page)) {
        passed = 0;
    }
    x64_pmm_get_stats(&after);
    if (after.free_pages != before.free_pages ||
        after.allocated_pages != before.allocated_pages) {
        passed = 0;
    }
    return passed;
}
