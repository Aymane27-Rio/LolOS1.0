#ifndef PMM_H
#define PMM_H
#include <stdint.h>

#define PMM_BLOCK_SIZE 4096
#define PMM_BLOCKS_PER_BYTE 8

void pmm_init(uint32_t mem_size_kb);
void* pmm_alloc_block();
void pmm_free_block(void* physical_addr);
uint32_t pmm_get_free_memory();

#endif