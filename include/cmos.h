#ifndef CMOS_H
#define CMOS_H

#include <stdint.h>

uint8_t get_rtc_register(int reg);
void get_real_time(uint8_t* h, uint8_t* m, uint8_t* s);

#define RTC_SECONDS 0x00
#define RTC_MINUTES 0x02
#define RTC_HOURS   0x04
#define RTC_DAY     0x07
#define RTC_MONTH   0x08
#define RTC_YEAR    0x09

uint8_t read_rtc(uint8_t reg);
uint8_t get_rtc_seconds();
uint8_t get_rtc_minutes();
uint8_t get_rtc_hours();
uint8_t get_rtc_day();
uint8_t get_rtc_month();
uint8_t get_rtc_year();

#endif