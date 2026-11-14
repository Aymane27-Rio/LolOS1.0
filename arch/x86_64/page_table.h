#ifndef LOLOS_X86_64_PAGE_TABLE_H
#define LOLOS_X86_64_PAGE_TABLE_H

#include <stdint.h>

struct x64_page_table_stats {
    uint64_t root_physical;
    uint32_t kernel_pages;
    uint32_t usable_pages;
    uint32_t table_pages;
};

/* Build an identity map from the normalized memory map and activate it. */
int x64_page_table_init(void);
void x64_page_table_get_stats(struct x64_page_table_stats *stats);

/* Map a single explicit user page. Executable and writable are mutually exclusive. */
int x64_page_table_map_user_page(uint64_t virtual_address,
                                uint64_t physical_address,
                                int writable, int executable);

/* Validate every page in a user range before the kernel dereferences it. */
int x64_page_table_validate_user_range(uint64_t address, uint64_t length,
                                       int writable);

#endif
