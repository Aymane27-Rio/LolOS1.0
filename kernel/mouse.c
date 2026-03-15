#include "../include/mouse.h"
#include "../include/io.h"

int32_t mouse_x = 400; // start in the middle of the screen
int32_t mouse_y = 300;
uint8_t left_click = 0;

uint8_t mouse_cycle = 0;
int8_t mouse_byte[3];

void mouse_wait(uint8_t a_type) {
    uint32_t timeout = 100000;
    if (a_type == 0) {
        while (timeout--) { if ((inb(0x64) & 1) == 1) return; } // signal data readu
    } else {
        while (timeout--) { if ((inb(0x64) & 2) == 0) return; } // ready for writing data
    }
}

void mouse_write(uint8_t a_write) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, a_write);
}

void mouse_init() {
    mouse_wait(1);
    outb(0x64, 0xA8);  
    mouse_write(0xF4); 
    mouse_wait(0);
    inb(0x60);
}

int handle_mouse_packet() {
    uint8_t status = inb(0x64);
    
    // check if data is ready AND if the 6th bit is set (which means it's mouse data, not keyboard)
    if ((status & 1) && (status & 0x20)) { 
        uint8_t mouse_in = inb(0x60);
        if (mouse_cycle == 0 && !(mouse_in & 0x08)) {
            return 0; 
        }
        mouse_byte[mouse_cycle++] = mouse_in;
        if (mouse_cycle == 3) {
            mouse_cycle = 0;
            left_click = mouse_byte[0] & 0x01;
            int sensitivity = 1; // adjust this value to make the mouse faster or slower
            mouse_x += (mouse_byte[1] * sensitivity);
            mouse_y -= (mouse_byte[2] * sensitivity); 
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_x > 784) mouse_x = 784;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_y > 584) mouse_y = 584;
            return 1;  
        }
    }
    return 0;
}