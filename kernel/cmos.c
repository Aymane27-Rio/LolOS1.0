#include "../include/cmos.h"
#include "../include/io.h"
#include <stdint.h>

// read a byte from the specified RTC register
uint8_t read_rtc(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

// convert BCD to binary, since the RTC stores time in BCD format
uint8_t bcd_to_binary(uint8_t bcd) {
    return ((bcd & 0xF0) >> 1) + ((bcd & 0xF0) >> 3) + (bcd & 0xf);
}

uint8_t get_rtc_seconds() { return bcd_to_binary(read_rtc(RTC_SECONDS)); }
uint8_t get_rtc_minutes() { return bcd_to_binary(read_rtc(RTC_MINUTES)); }
uint8_t get_rtc_hours()   { return bcd_to_binary(read_rtc(RTC_HOURS)); }
uint8_t get_rtc_day()     { return bcd_to_binary(read_rtc(RTC_DAY)); }
uint8_t get_rtc_month()   { return bcd_to_binary(read_rtc(RTC_MONTH)); }
uint8_t get_rtc_year()    { return bcd_to_binary(read_rtc(RTC_YEAR)); }


uint8_t get_rtc_register(int reg) {
    outb(0x70, reg);
    return inb(0x71);
}


void get_real_time(uint8_t* h, uint8_t* m, uint8_t* s) {

    while (get_rtc_register(0x0A) & 0x80);
    
    *s = get_rtc_register(0x00);
    *m = get_rtc_register(0x02);
    *h = get_rtc_register(0x04);
    uint8_t registerB = get_rtc_register(0x0B);
    if (!(registerB & 0x04)) {
        *s = (*s & 0x0F) + ((*s / 16) * 10);
        *m = (*m & 0x0F) + ((*m / 16) * 10);
        *h = ( (*h & 0x0F) + (((*h & 0x70) / 16) * 10) ) | (*h & 0x80);
    }
}