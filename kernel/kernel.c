#include "../include/terminal.h"
#include "../include/shell.h"
#include "../include/multiboot.h"
#include "../include/graphics.h"
#include "../include/mouse.h"
#include "../include/pmm.h"
#include "../include/paging.h"

void kmain(uint32_t magic, multiboot_info_t* mbd) {
    if (magic != 0x2BADB002) {
        return;
    }
    init_graphics(mbd);
    uint32_t total_mem_kb = mbd->mem_lower + mbd->mem_upper;
    pmm_init(total_mem_kb);
    init_paging((uint32_t)mbd->framebuffer_addr);
    terminal_init();
    mouse_init();
    draw_rect(0, 0, 800, 20, 0x00333333);
    draw_icon(770, 2, 0x0000FF00);
    int32_t old_mx = mouse_x;
    int32_t old_my = mouse_y;
    draw_cursor(mouse_x, mouse_y);
    while (1) {
        if (handle_mouse_packet()) {
            if (mouse_x != old_mx || mouse_y != old_my) {
                restore_cursor_bg(old_mx, old_my);
                draw_cursor(mouse_x, mouse_y);
                old_mx = mouse_x;
                old_my = mouse_y;
            }
            // did the user click the Gamepad icon?
            if (left_click && mouse_x >= 770 && mouse_x <= 786 && mouse_y >= 2 && mouse_y <= 18) {
                restore_cursor_bg(old_mx, old_my); 
                extern uint32_t cursor_y; 
                cursor_y = 25;
                print_string("  _          _  ___  ___\n");
                print_string(" | |    ___ | |/ _ \\/ __|\n");
                print_string(" | |__ / _ \\| | (_) \\__ \\\n");
                print_string(" |____|\\___/|_|\\___/|___/\n\n");
                print_string("LolOS Graphical Shell v1.0\n> ");
                start_shell(); 
            }
        }
    }
}