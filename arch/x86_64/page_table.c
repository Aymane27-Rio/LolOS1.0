#include <stdint.h>

#include "memory_map.h"
#include "page_table.h"
#include "pmm.h"

#define X64_PAGE_PRESENT 0x001ULL
#define X64_PAGE_WRITABLE 0x002ULL
#define X64_PAGE_USER 0x004ULL
#define X64_PAGE_LARGE 0x080ULL
#define X64_PAGE_NO_EXECUTE (1ULL << 63)
#define X64_PAGE_ADDRESS_MASK 0x000FFFFFFFFFF000ULL
#define X64_PAGE_MASK ((uint64_t)X64_PMM_PAGE_SIZE - 1u)
#define X64_USER_CANONICAL_LIMIT 0x0000800000000000ULL

extern unsigned char __kernel_start[];
extern unsigned char __kernel_end[];
extern unsigned char __kernel_text_start[];
extern unsigned char __kernel_text_end[];
extern unsigned char __kernel_writable_start[];
extern unsigned char __x64_user_test_text_start[];
extern unsigned char __x64_user_test_text_end[];
extern unsigned char __x64_user_test_rodata_start[];
extern unsigned char __x64_user_test_rodata_end[];
extern unsigned char x64_boot_stack_guard[];
extern unsigned char __x64_double_fault_guard_page[];
extern unsigned char __x64_nmi_guard_page[];
extern unsigned char __x64_machine_check_guard_page[];
extern unsigned char __x64_syscall_stack_guard_page[];

static struct x64_page_table_stats page_table_stats;
static int page_table_initialized;

static uint64_t allocate_table_page(void)
{
    uint64_t physical = x64_pmm_allocate_page();

    if (physical == 0u || (physical & X64_PAGE_MASK) != 0u ||
        physical >= X64_PMM_IDENTITY_LIMIT) {
        return 0u;
    }
    page_table_stats.table_pages++;
    return physical;
}

static int get_or_create_child(uint64_t *parent, uint32_t index,
                               int user, uint64_t **child_out)
{
    uint64_t entry = parent[index];

    if ((entry & X64_PAGE_PRESENT) != 0u) {
        if ((entry & X64_PAGE_LARGE) != 0u) {
            return 0;
        }
        if (user) {
            parent[index] |= X64_PAGE_USER;
        }
        *child_out = (uint64_t *)(uintptr_t)
            (entry & X64_PAGE_ADDRESS_MASK);
        return 1;
    }

    {
        uint64_t physical = allocate_table_page();

        if (physical == 0u) {
            return 0;
        }
        parent[index] = physical | X64_PAGE_PRESENT | X64_PAGE_WRITABLE |
                        (user ? X64_PAGE_USER : 0u);
        *child_out = (uint64_t *)(uintptr_t)physical;
    }
    return 1;
}

static int is_canonical_address(uint64_t address)
{
    uint64_t upper = address >> 48;

    return ((address >> 47) & 1u) != 0u
        ? upper == 0xFFFFu
        : upper == 0u;
}

static int map_page(uint64_t *root, uint64_t virtual_address,
                    uint64_t physical_address, uint64_t leaf_flags)
{
    uint32_t pml4_index = (uint32_t)((virtual_address >> 39) & 0x1FFu);
    uint32_t pdpt_index = (uint32_t)((virtual_address >> 30) & 0x1FFu);
    uint32_t pd_index = (uint32_t)((virtual_address >> 21) & 0x1FFu);
    uint32_t pt_index = (uint32_t)((virtual_address >> 12) & 0x1FFu);
    uint64_t *pdpt;
    uint64_t *page_directory;
    uint64_t *page_table;
    int user = (leaf_flags & X64_PAGE_USER) != 0u;

    if ((virtual_address & X64_PAGE_MASK) != 0u ||
        (physical_address & X64_PAGE_MASK) != 0u ||
        physical_address >= X64_PMM_IDENTITY_LIMIT ||
        !is_canonical_address(virtual_address) ||
        (user && virtual_address >= X64_USER_CANONICAL_LIMIT) ||
        !get_or_create_child(root, pml4_index, user, &pdpt) ||
        !get_or_create_child(pdpt, pdpt_index, user, &page_directory) ||
        !get_or_create_child(page_directory, pd_index, user, &page_table) ||
        (page_table[pt_index] & X64_PAGE_PRESENT) != 0u) {
        return 0;
    }

    page_table[pt_index] = physical_address | leaf_flags | X64_PAGE_PRESENT;
    return 1;
}

static int is_guard_page(uint64_t page)
{
    return page == ((uint64_t)(uintptr_t)x64_boot_stack_guard & ~X64_PAGE_MASK) ||
           page == ((uint64_t)(uintptr_t)__x64_double_fault_guard_page &
                    ~X64_PAGE_MASK) ||
           page == ((uint64_t)(uintptr_t)__x64_nmi_guard_page &
                    ~X64_PAGE_MASK) ||
           page == ((uint64_t)(uintptr_t)__x64_machine_check_guard_page &
                    ~X64_PAGE_MASK) ||
           page == ((uint64_t)(uintptr_t)__x64_syscall_stack_guard_page &
                    ~X64_PAGE_MASK);
}

static uint64_t kernel_page_flags(uint64_t page)
{
    uint64_t text_start = (uint64_t)(uintptr_t)__kernel_text_start;
    uint64_t text_end = (uint64_t)(uintptr_t)__kernel_text_end;
    uint64_t writable_start = (uint64_t)(uintptr_t)__kernel_writable_start;
    uint64_t user_text_start =
        (uint64_t)(uintptr_t)__x64_user_test_text_start;
    uint64_t user_text_end =
        (uint64_t)(uintptr_t)__x64_user_test_text_end;
    uint64_t user_rodata_start =
        (uint64_t)(uintptr_t)__x64_user_test_rodata_start;
    uint64_t user_rodata_end =
        (uint64_t)(uintptr_t)__x64_user_test_rodata_end;

    if (page >= user_text_start && page < user_text_end) {
        return X64_PAGE_USER; /* User-readable, read-only executable text. */
    }
    if (page >= user_rodata_start && page < user_rodata_end) {
        return X64_PAGE_USER | X64_PAGE_NO_EXECUTE;
    }
    if (page >= text_start && page < text_end) {
        return 0u; /* Read-only executable text. */
    }
    if (page >= writable_start) {
        return X64_PAGE_WRITABLE | X64_PAGE_NO_EXECUTE;
    }
    return X64_PAGE_NO_EXECUTE; /* Header, padding, and read-only data. */
}

static int map_kernel(uint64_t *root)
{
    uint64_t start = (uint64_t)(uintptr_t)__kernel_start;
    uint64_t end = (uint64_t)(uintptr_t)__kernel_end;
    uint64_t page;

    if ((start & X64_PAGE_MASK) != 0u || (end & X64_PAGE_MASK) != 0u ||
        start >= end || end > X64_PMM_IDENTITY_LIMIT) {
        return 0;
    }

    for (page = start; page < end; page += X64_PMM_PAGE_SIZE) {
        if (is_guard_page(page)) {
            continue;
        }
        if (!map_page(root, page, page, kernel_page_flags(page))) {
            return 0;
        }
        page_table_stats.kernel_pages++;
    }
    return 1;
}

static int map_usable_memory(uint64_t *root)
{
    uint32_t range_index;

    for (range_index = 0u; range_index < x64_usable_range_count;
         range_index++) {
        const struct x64_usable_range *range =
            &x64_usable_ranges[range_index];
        uint64_t end;
        uint64_t page;

        if (range->length == 0u ||
            range->length > UINT64_MAX - range->base ||
            (range->base & X64_PAGE_MASK) != 0u ||
            (range->length & X64_PAGE_MASK) != 0u) {
            return 0;
        }
        if (range->base >= X64_PMM_IDENTITY_LIMIT) {
            continue;
        }
        end = range->base + range->length;
        if (end > X64_PMM_IDENTITY_LIMIT) {
            end = X64_PMM_IDENTITY_LIMIT;
        }

        for (page = range->base; page < end; page += X64_PMM_PAGE_SIZE) {
            if (!map_page(root, page, page,
                          X64_PAGE_WRITABLE | X64_PAGE_NO_EXECUTE)) {
                return 0;
            }
            page_table_stats.usable_pages++;
        }
    }
    return 1;
}

int x64_page_table_init(void)
{
    uint64_t root_physical;
    uint64_t *root;

    if (page_table_initialized) {
        return 0;
    }
    page_table_stats.root_physical = 0u;
    page_table_stats.kernel_pages = 0u;
    page_table_stats.usable_pages = 0u;
    page_table_stats.table_pages = 0u;

    root_physical = allocate_table_page();
    if (root_physical == 0u) {
        return 0;
    }
    root = (uint64_t *)(uintptr_t)root_physical;
    page_table_stats.root_physical = root_physical;

    if (!map_kernel(root) || !map_usable_memory(root)) {
        return 0;
    }

    /* CR0.WP and EFER.NXE are enabled by the long-mode bootstrap. */
    __asm__ volatile ("mov %0, %%cr3" : : "r"(root_physical) : "memory");
    page_table_initialized = 1;
    return 1;
}

void x64_page_table_get_stats(struct x64_page_table_stats *stats)
{
    if (stats == 0) {
        return;
    }
    *stats = page_table_stats;
}

int x64_page_table_map_user_page(uint64_t virtual_address,
                                uint64_t physical_address,
                                int writable, int executable)
{
    uint64_t *root;
    uint64_t flags = X64_PAGE_USER;

    if (!page_table_initialized || writable < 0 || writable > 1 ||
        executable < 0 || executable > 1 || (writable && executable) ||
        virtual_address >= X64_USER_CANONICAL_LIMIT ||
        (virtual_address & X64_PAGE_MASK) != 0u ||
        (physical_address & X64_PAGE_MASK) != 0u ||
        physical_address == 0u ||
        physical_address >= X64_PMM_IDENTITY_LIMIT) {
        return 0;
    }
    if (writable) {
        flags |= X64_PAGE_WRITABLE;
    }
    if (!executable) {
        flags |= X64_PAGE_NO_EXECUTE;
    }

    root = (uint64_t *)(uintptr_t)page_table_stats.root_physical;
    if (!map_page(root, virtual_address, physical_address, flags)) {
        return 0;
    }
    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");
    return 1;
}

static int user_page_accessible(uint64_t *root, uint64_t virtual_address,
                                int writable)
{
    uint32_t pml4_index = (uint32_t)((virtual_address >> 39) & 0x1FFu);
    uint32_t pdpt_index = (uint32_t)((virtual_address >> 30) & 0x1FFu);
    uint32_t pd_index = (uint32_t)((virtual_address >> 21) & 0x1FFu);
    uint32_t pt_index = (uint32_t)((virtual_address >> 12) & 0x1FFu);
    uint64_t entry = root[pml4_index];
    uint64_t *table;
    uint64_t required = X64_PAGE_PRESENT | X64_PAGE_USER |
                        (writable ? X64_PAGE_WRITABLE : 0u);

    if ((entry & required) != required) {
        return 0;
    }
    table = (uint64_t *)(uintptr_t)(entry & X64_PAGE_ADDRESS_MASK);
    entry = table[pdpt_index];
    if ((entry & required) != required || (entry & X64_PAGE_LARGE) != 0u) {
        return 0;
    }
    table = (uint64_t *)(uintptr_t)(entry & X64_PAGE_ADDRESS_MASK);
    entry = table[pd_index];
    if ((entry & required) != required || (entry & X64_PAGE_LARGE) != 0u) {
        return 0;
    }
    table = (uint64_t *)(uintptr_t)(entry & X64_PAGE_ADDRESS_MASK);
    entry = table[pt_index];
    if ((entry & required) != required) {
        return 0;
    }
    return 1;
}

int x64_page_table_validate_user_range(uint64_t address, uint64_t length,
                                       int writable)
{
    uint64_t end;
    uint64_t page;
    uint64_t last_page;
    uint64_t *root;

    if (!page_table_initialized || writable < 0 || writable > 1 ||
        address >= X64_USER_CANONICAL_LIMIT ||
        length > X64_USER_CANONICAL_LIMIT - address) {
        return 0;
    }
    if (length == 0u) {
        return 1;
    }
    end = address + length;
    last_page = (end - 1u) & ~X64_PAGE_MASK;
    root = (uint64_t *)(uintptr_t)page_table_stats.root_physical;

    for (page = address & ~X64_PAGE_MASK;; page += X64_PMM_PAGE_SIZE) {
        if (!user_page_accessible(root, page, writable)) {
            return 0;
        }
        if (page == last_page) {
            break;
        }
    }
    return 1;
}
