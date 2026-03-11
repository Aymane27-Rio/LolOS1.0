#ifndef GRAPHICS_H
#define GRAPHICS_H
#include "multiboot.h"
#include <stdint.h>

void init_graphics(multiboot_info_t* mbd);
void put_pixel(uint32_t x, uint32_t y, uint32_t color);
void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void draw_char(char c, uint32_t x, uint32_t y, uint32_t color);
void draw_string(const char* str, uint32_t x, uint32_t y, uint32_t color);
void draw_icon(uint32_t x, uint32_t y, uint32_t color);

#endif