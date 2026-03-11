#include "../include/terminal.h"
#include "../include/shell.h"
#include "../include/multiboot.h"
#include "../include/graphics.h"

void kmain(uint32_t magic, multiboot_info_t* mbd) {
    if (magic != 0x2BADB002) {
        return;
    }
    init_graphics(mbd);
    terminal_init();
    draw_rect(0, 0, 800, 20, 0x00333333);
    draw_icon(770, 2, 0x0000FF00);
    extern uint32_t cursor_y;
    cursor_y = 25;
    print_string("LolOS Graphical Shell v1.0\n");
    print_string("> ");
    start_shell();
    // draw_rect(350, 250, 100, 100, 0x00FF0000);
    // draw_rect(100, 100, 150, 150, 0x0000FF00);
    __asm__ volatile("cli; hlt"); //just in case
}