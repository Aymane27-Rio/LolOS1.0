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

#endif
