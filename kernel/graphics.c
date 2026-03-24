#include "../include/graphics.h"
#include "../include/font.h"

uint32_t* framebuffer;
uint32_t pitch;
uint32_t width;
uint32_t height;

static uint32_t back_buffer[800 * 600]; // for double buffering, feel free to change the resolution if you want
static uint32_t* real_screen;

void init_graphics(multiboot_info_t* mbd) {
    // cast the 64-bit physical address to a 32-bit pointer
    real_screen = (uint32_t*)(uint32_t)mbd->framebuffer_addr;
    framebuffer = back_buffer;
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
    int scale = 2; //better visibility on 800x600, feel free to change it back to 1 for a sharper look
    for (int cy = 0; cy < 8; cy++) {
        for (int cx = 0; cx < 8; cx++) {
            // checking if the specific bit is set to 1
            if (glyph[cy] & (0x80 >> cx)) {
                draw_rect(x + (cx * scale), y + (cy * scale), scale, scale, color);            
            }
        }
    }
}

void draw_string(const char* str, uint32_t x, uint32_t y, uint32_t color) {
    int current_x = x;
    for (int i = 0; str[i] != '\0'; i++) {
        draw_char(str[i], current_x, y, color);
        current_x += 16; // move 16 pixels to the right for the next letter
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

uint32_t cursor_bg[256];

uint16_t mouse_arrow[16] = {
    0b1000000000000000,
    0b1100000000000000,
    0b1110000000000000,
    0b1111000000000000,
    0b1111100000000000,
    0b1111110000000000,
    0b1111111000000000,
    0b1111111100000000,
    0b1111111110000000,
    0b1111111111000000,
    0b1111111000000000,
    0b1110111100000000,
    0b1100011100000000,
    0b1000001110000000,
    0b0000000110000000,
    0b0000000000000000
};


void save_cursor_bg(uint32_t x, uint32_t y) {
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            if (x + j < width && y + i < height) {
                // copying raw data
                cursor_bg[i * 16 + j] = framebuffer[(y + i) * (pitch / 4) + (x + j)];
            }
        }
    }
}

void restore_cursor_bg(uint32_t x, uint32_t y) {
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            if (x + j < width && y + i < height) {
                // originally i wanted to use put_pixel here but since we already have the raw data, might as well copy it directly for better performance
                put_pixel(x + j, y + i, cursor_bg[i * 16 + j]);
            }
        }
    }
}

void draw_cursor(uint32_t x, uint32_t y) {
    save_cursor_bg(x, y); 
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            if (x + j < width && y + i < height) {
                if (mouse_arrow[i] & (0x8000 >> j)) { 
                    put_pixel(x + j, y + i, 0x00FFFFFF);
                }
            }
        }
    }
}

// 16x16 terminal window sprite
uint16_t term_sprite[16] = {
    0b1111111111111111,
    0b1000000000000001,
    0b1010000000000001,
    0b1001000000000001,
    0b1010000000000001,
    0b1000001110000001,
    0b1000000000000001,
    0b1000000000000001,
    0b1000000000000001,
    0b1000000000000001,
    0b1000000000000001,
    0b1000000000000001,
    0b1000000000000001,
    0b1000000000000001,
    0b1000000000000001,
    0b1111111111111111
};

void draw_terminal_icon(uint32_t x, uint32_t y, uint32_t color) {
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            if (term_sprite[i] & (0x8000 >> j)) {
                put_pixel(x + j, y + i, color);
            } else {
                put_pixel(x + j, y + i, 0x001E1E1E); 
            }
        }
    }
}

void draw_window(uint32_t x, uint32_t y, uint32_t w, uint32_t h, char* title) {
    draw_rect(x, y, w, h, 0x001E1E1E);

    draw_rect(x, y, w, 1, 0x00AAAAAA);           
    draw_rect(x, y + h, w, 1, 0x00AAAAAA);       
    draw_rect(x, y, 1, h, 0x00AAAAAA);           
    draw_rect(x + w, y, 1, h + 1, 0x00AAAAAA);  
    draw_rect(x + 1, y + 1, w - 2, 22, 0x00333333);
    draw_string(title, x + 8, y + 8, 0x00FFFFFF);
    draw_rect(x + w - 22, y + 4, 16, 16, 0x00FF4444);
    draw_string("X", x + w - 18, y + 8, 0x00FFFFFF);
}

void swap_buffers() {
    for (uint32_t i = 0; i < 800 * 600; i++) {
        real_screen[i] = back_buffer[i];
    }
}