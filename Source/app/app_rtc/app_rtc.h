#ifndef _APP_RTC_H_
#define _APP_RTC_H_

#include <stdint.h>

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
} rtc_ex_time_t;

void app_rtc_ex_init(void);

void rct_set_unix_time(const uint32_t time);

void rct_get_unix_time(uint32_t *time);     // 获取unix类型的时间
void rtc_get_struct_time(rtc_ex_time_t *t); // 获取结构体类型的时间

#endif