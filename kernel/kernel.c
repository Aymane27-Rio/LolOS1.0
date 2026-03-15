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

extern void isr32();
extern void isr128();


typedef struct {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
} multiboot_module_t;
multiboot_info_t* global_mbd = 0;

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
        }
        if (handle_mouse_packet()) {
            if (mouse_x != old_mx || mouse_y != old_my) {
                restore_cursor_bg(old_mx, old_my);
                draw_cursor(mouse_x, mouse_y);
                old_mx = mouse_x;
                old_my = mouse_y;
            }
            // EDGE DETECTION: Only trigger right when the button is initially pressed down
            // (this is where old_left_click is finally USED, making the compiler happy)
            if (left_click == 1 && old_left_click == 0) {
                if (mouse_x >= 20 && mouse_x <= 36 && mouse_y >= 40 && mouse_y <= 56) {
                    // DOUBLE CLICK MATH: was the last click less than 50 ticks ago?
                    if (timer_ticks - last_click_time < 50) {
                        restore_cursor_bg(old_mx, old_my); 
                        terminal_clear(); 
                        if (global_mbd->mods_count > 0) {
                            multiboot_module_t* mod = (multiboot_module_t*)global_mbd->mods_addr;
                            uint8_t* elf_file_in_ram = (uint8_t*)mod->mod_start;
                            
                            print_string("Loading User Space Executable...\n");
                            load_elf_and_execute(elf_file_in_ram);
                        } else {
                            print_string("ERROR: No ELF module found by GRUB!\n");
                        }
                    }
                }
                last_click_time = timer_ticks;
            }
            old_left_click = left_click;
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
    start_desktop(mbd);

    
}