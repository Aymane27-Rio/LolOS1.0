#include "../include/terminal.h"
#include "../include/shell.h"
#include "../include/multiboot.h"
#include "../include/graphics.h"
#include "../include/mouse.h"
#include "../include/pmm.h"
#include "../include/paging.h"
#include "../include/idt.h"
#include "../include/pic.h"
#include "../include/timer.h"
#include "../include/gdt.h"
#include "../include/cmos.h"
#include "../include/syscall.h"
#include "../include/elf.h"
#include "../include/fs.h"
#include "../include/vfs.h"

extern void isr32();
extern void isr128();
uint32_t terminal_row;
uint32_t terminal_column;

typedef struct {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
} multiboot_module_t;
multiboot_info_t* global_mbd = 0;
uint8_t window_open = 0; // 0 for desktop, 1 for teminal window

void start_desktop() {
    draw_rect(0, 0, 800, 20, 0x00333333);          
    draw_icon(770, 2, 0x0000FF00);                 
    draw_terminal_icon(20, 40, 0x00FFFFFF);
    draw_string("CLI", 14, 60, 0x00FFFFFF);        

    int32_t old_mx = mouse_x;
    int32_t old_my = mouse_y;
    draw_cursor(mouse_x, mouse_y);

    uint8_t old_left_click = 0;
    uint32_t last_click_time = 0;
    uint32_t last_clock_update = 0;
    swap_buffers();
    uint32_t win_x = 150;
    uint32_t win_y = 100;
    uint8_t is_dragging = 0;
    int32_t drag_offset_x = 0;
    int32_t drag_offset_y = 0;
    while (1) {
        // --- MULTITASKING: UPDATE THE CLOCK EVERY SECOND ---
        // 100 ticks = 1 second of hardware interrupts
        if (timer_ticks - last_clock_update >= 100) {
            last_clock_update = timer_ticks;
            uint8_t h, m, s;
            get_real_time(&h, &m, &s);
            char clock_buf[] = "00:00:00 UTC";
            
            clock_buf[0] = (h / 10) + '0'; clock_buf[1] = (h % 10) + '0';
            clock_buf[3] = (m / 10) + '0'; clock_buf[4] = (m % 10) + '0';
            clock_buf[6] = (s / 10) + '0'; clock_buf[7] = (s % 10) + '0';
            draw_rect(350, 2, 160, 16, 0x00333333); 
            draw_string(clock_buf, 350, 2, 0x00FFFFFF);
            swap_buffers();
        }
        if (handle_mouse_packet()) {
            if (mouse_x != old_mx || mouse_y != old_my) {
                if (is_dragging) {
                    win_x = mouse_x - drag_offset_x;
                    win_y = mouse_y - drag_offset_y;
                    draw_rect(0, 20, 800, 580, 0x001E1E1E); 
                    draw_terminal_icon(20, 40, 0x00FFFFFF);
                    draw_string("CLI", 14, 60, 0x00FFFFFF);
                    draw_window(win_x, win_y, 500, 350, "Terminal - /bin/app.elf");
                }
                else{
                    restore_cursor_bg(old_mx, old_my);
                }
                draw_cursor(mouse_x, mouse_y);
                old_mx = mouse_x;
                old_my = mouse_y;
            }
            // EDGE DETECTION: Only trigger right when the button is initially pressed down
            // (this is where old_left_click is finally USED, making the compiler happy)
            if (left_click == 1 && old_left_click == 0) {
                int32_t mx = (int32_t)mouse_x;
                int32_t my = (int32_t)mouse_y;
                int32_t wx = (int32_t)win_x;
                int32_t wy = (int32_t)win_y;
                // SCENARIO A: window is open
                if (window_open == 1) {
                    // 1. close button
                    if (mx >= wx + 500 - 22 && mx <= wx + 500 - 6 && 
                        my >= wy + 4 && my <= wy + 20) {
                        window_open = 0;
                        is_dragging = 0;
                        draw_rect(0, 20, 800, 580, 0x001E1E1E); 
                        draw_terminal_icon(20, 40, 0x00FFFFFF);
                        draw_string("CLI", 14, 60, 0x00FFFFFF);
                    } 
                    // 2. title bar dragging
                    else if (mx >= wx && mx <= wx + 500 - 25 && 
                             my >= wy && my <= wy + 24) {
                        is_dragging = 1;
                        drag_offset_x = mx - wx;
                        drag_offset_y = my - wy;
                    }
                }
                // SCENARIO B: no window is open
                else if (window_open == 0) {
                    if (mx >= 20 && mx <= 36 && my >= 40 && my <= 56) {
                        if (timer_ticks - last_click_time < 50) {
                            window_open = 1;
                            win_x = 150;
                            win_y = 100;
                            draw_rect(0, 20, 800, 580, 0x001E1E1E); 
                            draw_terminal_icon(20, 40, 0x00FFFFFF);
                            draw_string("CLI", 14, 60, 0x00FFFFFF);
                            draw_window(win_x, win_y, 500, 350, "Terminal - /bin/app.elf");
                            terminal_row = (win_y + 24) / 8;
                            terminal_column = (win_x + 8) / 8;
                            if (global_mbd->mods_count > 0) {
                                multiboot_module_t* mod = (multiboot_module_t*)global_mbd->mods_addr;
                                uint8_t* elf_file_in_ram = (uint8_t*)mod->mod_start;
                                load_elf_and_execute(elf_file_in_ram);
                            }
                        }
                    }
                }
                last_click_time = timer_ticks;
            }
            if (left_click == 0 && old_left_click == 1) {
                is_dragging = 0;
            }
            old_left_click = left_click;
            swap_buffers();
        }
    }
}



void kmain(uint32_t magic, multiboot_info_t* mbd) {
    if (magic != 0x2BADB002) {
        return;
    }
    global_mbd = mbd;
    init_graphics(mbd);
    uint32_t total_mem_kb = mbd->mem_lower + mbd->mem_upper;
    pmm_init(total_mem_kb);
    init_paging((uint32_t)mbd->framebuffer_addr);
    terminal_init();
    mouse_init();
    init_gdt();
    init_idt();                
    pic_remap();                                   
    idt_set_gate(32, (uint32_t)isr32, 0x08, 0x8E); 
    idt_set_gate(128, (uint32_t)isr128, 0x08, 0xEE);
    __asm__ volatile ("sti");
    vfs_lba_init();
    print_string("\n-- VFS mount test --\n");
    if (fs_root != 0) {
        struct dirent* dir_node = 0;
        uint32_t file_index = 0;
        while ((dir_node = vfs_readdir(fs_root, file_index)) != 0) {
            print_string("VFS Found File: ");
            print_string(dir_node->name);
            print_string(" (Inode: ");
            print_time_number(dir_node->ino); 
            print_string(")\n");
            
            file_index++;
        }
        
        if (file_index == 0) {
            print_string("VFS is mounted, but the root directory is empty\n");
        }
    } else {
        print_string("PANIC: VFS root is null\n");
    }
    print_string("----------------------\n\n");
    start_desktop(mbd);

    
}