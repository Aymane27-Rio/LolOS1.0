#include "../include/graphics.h"
#include "../include/font.h"

uint32_t* framebuffer;
uint32_t pitch;
uint32_t width;
uint32_t height;

void init_graphics(multiboot_info_t* mbd) {
    // cast the 64-bit physical address to a 32-bit pointer
    framebuffer = (uint32_t*) (uint32_t) mbd->framebuffer_addr;
    pitch = mbd->framebuffer_pitch;
    width = mbd->framebuffer_width;
    height = mbd->framebuffer_height;
}

// color format is Hex: 0x00RRGGBB
void put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= width || y >= height) return;
    // pitch is in bytes and since we use 32-bit (4-byte) pixels, we divide pitch by 4.
    framebuffer[y * (pitch / 4) + x] = color;
}

void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t i = 0; i < h; i++) {
        for (uint32_t j = 0; j < w; j++) {
            put_pixel(x + j, y + i, color);
        }
    }
}

void draw_char(char c, uint32_t x, uint32_t y, uint32_t color) {
    // casting to unsigned char to safely check the bounds without fucking compiler warnings
    unsigned char uc = (unsigned char)c;
    // only draw printable ASCII characters for now
    if (uc < 32 || uc > 127) return; 
    uint8_t* glyph = font8x8[uc];
    for (int cy = 0; cy < 8; cy++) {
        for (int cx = 0; cx < 8; cx++) {
            // checking if the specific bit is set to 1
            if (glyph[cy] & (0x80 >> cx)) {
                put_pixel(x + cx, y + cy, color);
            }
        }
    }
}

void draw_string(const char* str, uint32_t x, uint32_t y, uint32_t color) {
    int current_x = x;
    for (int i = 0; str[i] != '\0'; i++) {
        draw_char(str[i], current_x, y, color);
        current_x += 8; // move 8 pixels to the right for the next letter
    }
}

// a 16x16 pixel retro gamepad since i love games
uint16_t sys_icon[16] = {
    0b0000000000000000,
    0b0000000000000000,
    0b0000111111110000,
    0b0011111111111100,
    0b0111111111111110,
    0b0110101111001010, // D-pad left/right, action buttons
    0b0101011111010110,
    0b0110101111001010,
    0b0111111111111110,
    0b0011100000011100,
    0b0001100000011000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000
};
void draw_icon(uint32_t x, uint32_t y, uint32_t color) {
    for (int cy = 0; cy < 16; cy++) {
        for (int cx = 0; cx < 16; cx++) {
            if (sys_icon[cy] & (0x8000 >> cx)) {
                put_pixel(x + cx, y + cy, color);
            }
        }
    }
}