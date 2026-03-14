#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>

void init_paging(uint32_t framebuffer_addr);
void map_page(uint32_t physical_addr, uint32_t virtual_addr);

#endif