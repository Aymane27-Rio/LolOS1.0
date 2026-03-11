#include "../include/terminal.h"
#include "../include/graphics.h"

int cursor_x = 0;
int cursor_y = 0;
uint32_t term_color = 0x00FFFFFF; 
uint32_t bg_color   = 0x001E1E1E; 

void terminal_init(){
    terminal_clear();
}

void terminal_clear(){
    draw_rect(0, 0, 800, 600, bg_color);
    cursor_x = 0;
    cursor_y = 0;
}


void terminal_apply_theme(uint8_t color){
    if (color == 0x0A) term_color = 0x0000FF00;   
    else if (color == 0x09) term_color = 0x000088FF; 
    else if (color == 0x0C) term_color = 0x00FF0000; 
    else term_color = 0x00FFFFFF;                    
}


void print_char(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y += 8;
    } else if (c == '\b') {
        if (cursor_x >= 8) {
            cursor_x -= 8;
            draw_rect(cursor_x, cursor_y, 8, 8, bg_color);
        }
    } else {
        draw_char(c, cursor_x, cursor_y, term_color);
        cursor_x += 8;
    }

    if (cursor_x >= 800) {
        cursor_x = 0;
        cursor_y += 8;
    }
    if (cursor_y >= 600) {
        terminal_clear();
    }
}

void print_string(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        print_char(str[i]);
    }
}

void print_time_number(int num) {
    if (num < 10) {
        print_char('0'); 
    }
    char buf[10];
    int i = 0;
    if (num ==0) buf[i++] = '0';
    while (num > 0) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }
    while (i > 0){
        i--;
        print_char(buf[i]);
    }
}