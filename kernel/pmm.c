#include "../include/pmm.h"
#include "../include/terminal.h"

// 128MB maximum memory support for LolOS for now
#define MAX_BLOCKS (128 * 1024 * 1024) / PMM_BLOCK_SIZE 
uint8_t memory_bitmap[MAX_BLOCKS / PMM_BLOCKS_PER_BYTE];
uint32_t total_blocks = 0;
uint32_t used_blocks = 0;

inline void bitmap_set(uint32_t bit) {
    memory_bitmap[bit / 8] |= (1 << (bit % 8)); // 1 is used
}

inline void bitmap_clear(uint32_t bit) {
    memory_bitmap[bit / 8] &= ~(1 << (bit % 8)); // 0 is free
}

inline uint8_t bitmap_test(uint32_t bit) {
    return memory_bitmap[bit / 8] & (1 << (bit % 8)); //checking
}

void pmm_init(uint32_t mem_size_kb) {
    total_blocks = (mem_size_kb * 1024) / PMM_BLOCK_SIZE;
    if (total_blocks > MAX_BLOCKS) total_blocks = MAX_BLOCKS;
    used_blocks = 0;
    for (uint32_t i = 0; i < total_blocks / 8; i++) {
        memory_bitmap[i] = 0;
    }
    // CRITICAL:must reserve the first ~4MB of RAM
    // this protects the kernel code, the VGA framebuffer, and multiboot data from being overwritten
    for (uint32_t i = 0; i < 1000; i++) {
        bitmap_set(i);
        used_blocks++;
    }
}

void* pmm_alloc_block() {
    if (used_blocks >= total_blocks) return 0; 
    for (uint32_t i = 0; i < total_blocks; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_blocks++;
            return (void*)(i * PMM_BLOCK_SIZE);
        }
    }
    return 0; // unless mem is full, this should never be reached
}

void pmm_free_block(void* physical_addr) {
    uint32_t block = (uint32_t)physical_addr / PMM_BLOCK_SIZE;
    bitmap_clear(block);
    used_blocks--;
}

uint32_t pmm_get_free_memory() {
    return (total_blocks - used_blocks) * PMM_BLOCK_SIZE;
}