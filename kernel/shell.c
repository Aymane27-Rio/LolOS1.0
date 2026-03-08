#include <stdint.h>
#include "../include/io.h"
#include "../include/terminal.h"
#include "../include/string.h"
#include "../include/cmos.h"
#include "../include/shell.h"
#include "../include/cpu.h"
#include "../include/ata.h"
#include "../include/fs.h"

// keyboard driver
char get_keypress();
// we need these to manage our command buffer and history
char command_buffer[256];
int buffer_index = 0;

char history[5][256]; 
int history_index = 0;
int history_count = 0;

// sleep func
void sleep(int seconds) {
    int elapsed = 0;
    uint8_t last_sec = get_rtc_seconds();
    
    while (elapsed < seconds) {
        uint8_t current_sec = get_rtc_seconds();
        if (current_sec != last_sec) {
            elapsed++;
            last_sec = current_sec;
        }
    }
}
// the main shell loop
void start_shell() {
    fs_init(); // booting the FS
    while (1) {
        char key = get_keypress();
        if (key != 0){
            
            // enter key
            if (key == '\n') {
                print_char('\n');
                command_buffer[buffer_index] = '\0'; 
            
                if (buffer_index > 0){
                    strcpy(history[history_count % 5], command_buffer);
                    history_count++;
                    history_index = history_count;

                    char* args = 0;
                    for (int i = 0; i < buffer_index; i++){
                        if (command_buffer[i] == ' '){
                            command_buffer[i] = '\0';
                            args = &command_buffer[i + 1];
                            break;
                        }
                    }
                    // commands
                    if (strcmp(command_buffer, "help") == 0) {
                        print_string("\n--- LolOS Command Reference ---\n");
                        print_string(" SYSTEM : help, clear, sysinfo, reboot, shutdown\n");
                        print_string(" UTILS  : echo, theme, time, date, sleep\n");
                        print_string(" DISK   : disktest, ls, touch, write, cat, rm\n");
                        print_string("-------------------------------\n");
                    }
                    else if (strcmp(command_buffer, "clear") == 0) {
                        terminal_clear();
                    } 
                    else if (strcmp(command_buffer, "echo") == 0) {
                        if (args != 0) print_string(args);
                        print_char('\n');
                    }
                    else if (strcmp(command_buffer, "time") == 0) {
                        print_string("Current Time: ");
                        print_time_number(get_rtc_hours());
                        print_char(':');
                        print_time_number(get_rtc_minutes());
                        print_char(':');
                        print_time_number(get_rtc_seconds());
                        print_string(" (UTC)\n");
                    }
                    else if (strcmp(command_buffer, "date") == 0) {
                        print_string("Current Date: ");
                        print_time_number(get_rtc_day());
                        print_char('/');
                        print_time_number(get_rtc_month());
                        print_string("/20"); 
                        print_time_number(get_rtc_year());
                        print_char('\n');
                    }
                    else if (strcmp(command_buffer, "sleep") == 0) {
                        if (args != 0) {
                            int sec = atoi(args);
                            if (sec > 0) sleep(sec);
                        } else {
                            print_string("Usage: sleep [seconds]\n");
                        }
                    }
                    else if (strcmp(command_buffer, "reboot") == 0) {
                        print_string("Rebooting system...\n");
                        outb(0x64, 0xFE); 
                    }
                    else if (strcmp(command_buffer, "shutdown") == 0) {
                        print_string("System stopped. You can close the emulator now.\n");
                        __asm__ volatile("cli; hlt"); 
                    }
                    else if (strcmp(command_buffer, "theme") == 0) {
                        if (args == 0) print_string("Usage: theme [green|blue|red|default]\n");
                        else if (strcmp(args, "green") == 0) terminal_apply_theme(0x0A);
                        else if (strcmp(args, "blue") == 0) terminal_apply_theme(0x09);
                        else if (strcmp(args, "red") == 0) terminal_apply_theme(0x0C);
                        else if (strcmp(args, "default") == 0) terminal_apply_theme(0x0F);
                        else print_string("Unknown theme. Try: green, blue, red, default\n");
                    }
                    else if (strcmp(command_buffer, "sysinfo") == 0) {
                        char vendor[13];
                        get_cpu_vendor(vendor);
                        print_string("OS: LolOs v0.3 (32-bit x86)\n");
                        print_string("CPU Vendor: ");
                        print_string(vendor);
                        print_char('\n');
                    }
                    else if (strcmp(command_buffer, "disktest") == 0){
                        uint8_t write_buf[512];
                        uint8_t read_buf[512];
                        //clear the buffers first
                        memset(write_buf, 0, 512);
                        memset(read_buf, 0, 512);
                        //test data to write buffer
                        strcpy((char*)write_buf, "LolOs hard drive is working");
                        print_string("Writing to disk\n");
                        ata_write_sector(1, write_buf); // write to LBA 1
                        print_string("Reading from disk\n");
                        ata_read_sector(1, read_buf); //read from LBA 1
                        print_string((char*)read_buf);
                        print_char('\n');
                    }
                    else if (strcmp(command_buffer, "ls") == 0) {
                        fs_list_files();
                    }
                    else if (strcmp(command_buffer, "touch") == 0) {
                        if (args != 0) {
                            fs_create_file(args);
                        } else {
                            print_string("Usage: touch [filename]\n");
                        }
                    }
                    else if (strcmp(command_buffer, "write") == 0) {
                        if (args != 0) {
                            char* content = 0;
                            for (int i = 0; args[i] != '\0'; i++) {
                                if (args[i] == ' ') {
                                    args[i] = '\0'; // splitting the string to separate filename and content
                                    content = &args[i + 1];
                                    break;
                                }
                            }
                            if (content != 0) {
                                fs_write_file(args, content);
                            } else {
                                print_string("Usage: write [filename] [text]\n");
                            }
                        } else {
                            print_string("Usage: write [filename] [text]\n");
                        }
                    }
                    else if (strcmp(command_buffer, "cat") == 0) {
                        if (args != 0) {
                            fs_read_file(args);
                        } else {
                            print_string("Usage: cat [filename]\n");
                        }
                    }
                    else if (strcmp(command_buffer, "rm") == 0) {
                        if (args != 0) {
                            fs_delete_file(args);
                        } else {
                            print_string("Usage: rm [filename]\n");
                        }
                    }
                    else {
                        print_string("Unknown command: ");
                        print_string(command_buffer);
                        print_char('\n');
                    }
                }
                buffer_index = 0;
                print_string("> ");
            } 
            // up arrow
            else if (key == 17){
                if (history_count > 0 && (history_count - history_index) < 5 && history_index > 0) {
                    for (int i = 0; i < buffer_index; i++) print_char('\b');
                    history_index--;
                    strcpy(command_buffer, history[history_index % 5]);
                    buffer_index = strlen(command_buffer);
                    print_string(command_buffer);
                }
            } 
            // down arrow
            else if (key == 18){
                if (history_index < history_count) {
                    for (int i = 0; i < buffer_index; i++) print_char('\b');
                    history_index++;
                    if (history_index == history_count) {
                        command_buffer[0] = '\0';
                        buffer_index = 0;
                    } else {
                        strcpy(command_buffer, history[history_index % 5]);
                        buffer_index = strlen(command_buffer);
                        print_string(command_buffer);
                    }
                }
            } 
            // mss7
            else if (key == '\b') {
                if (buffer_index > 0) {
                    buffer_index--;     
                    print_char('\b'); 
                }
            } 
            // normal characters
            else {
                if (buffer_index < 254) {
                    command_buffer[buffer_index] = key;
                    buffer_index++;
                    print_char(key);
                }
            }
        }      
    }
}