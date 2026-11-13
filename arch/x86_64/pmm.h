#ifndef LOLOS_X86_64_PMM_H
#define LOLOS_X86_64_PMM_H

#include <stdint.h>

#define X64_PMM_PAGE_SIZE 4096u
#define X64_PMM_IDENTITY_LIMIT 0x40000000ULL
#define X64_PMM_PAGE_COUNT \
    ((uint32_t)(X64_PMM_IDENTITY_LIMIT / X64_PMM_PAGE_SIZE))

struct x64_pmm_stats {
    uint32_t managed_pages;
    uint32_t free_pages;
    uint32_t allocated_pages;
};

int x64_pmm_init(void);
uint64_t x64_pmm_allocate_page(void);
int x64_pmm_free_page(uint64_t physical_address);
void x64_pmm_get_stats(struct x64_pmm_stats *stats);
int x64_pmm_self_test(void);

#endif
