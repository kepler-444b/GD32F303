#include "app_rtc.h"
#include "gd32f30x.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"
#include "../Source/app/app_evenbus/app_eventbus.h"
#include "../Source/app/app_public_protocol/app_display_protocol.h"
#include "../Source/bsp/bsp_pcb/bsp_pcb.h"
#include "../Source/bsp/bsp_i2c/bsp_i2c.h"
#include "../Source/app/app_base/app_base.h"
#include <stdlib.h>
#include <string.h>

// 宏定义
#define DEC_TO_BCD(val) (((val) / 10) << 4 | ((val) % 10))
#define BCD_TO_DEC(val) (((val) >> 4) * 10 + ((val) & 0x0F))

#define RTC_ADDR        (0x32 << 1) // RTC 模块i2c地址 预移位，简化后续调用

#define STATE_REG       0x1D // 状态寄存器
#define CTRL_REG        0x1E // 控制寄存器
#define INIEN_REG       0x1F // 控制寄存器(电池自动切换)

#define SEC_REG         0x10 // 秒寄存器
#define MIN_REG         0x11 // 分寄存器
#define HOUR_REG        0x12 // 时寄存器
#define DAY_REG         0x13 // 天寄存器
#define MONTH_REG       0x15 // 月寄存器
#define YEAR_REG        0x16 // 年寄存器

// 函数声明
static void app_rtc_unix_to_date(uint32_t timestamp, rtc_ex_time_t *t);
static void app_rtc_date_to_unix(rtc_ex_time_t *t, uint32_t *timestamp);
static void rtc_ex_set_time(rtc_ex_time_t *t);
static void rtc_ex_get_time(rtc_ex_time_t *t);
static void app_rtc_display_set_time(void);

// 初始化 RTC
void app_rtc_ex_init(void)
{
    APP_PRINTF("[app_rtc_ex_init] ================================\n");

    bsp_i2c_init(I2C0, RTC_ADDR); // 初始化 i2c 接口
    uint8_t inien_status = 0;
    if (!bsp_i2c_read(I2C0, RTC_ADDR, INIEN_REG, &inien_status)) {
        APP_ERROR("RTC not found (I2C NACK)");
        return;
    }

    inien_status |= (1 << 4); // INIEN 启用电源切换检测
    inien_status |= (1 << 5); // CHGEN 启用电池充电
    if (!bsp_i2c_write(I2C0, RTC_ADDR, INIEN_REG, inien_status)) {
        APP_ERROR("Failed to write CTRL_1");
        return;
    }

    uint8_t status = 0;
    if (!bsp_i2c_read(I2C0, RTC_ADDR, STATE_REG, &status)) {
        APP_ERROR("Failed to read STATE_REG");
        return;
    }
    if (BIT0(status)) { // 振荡器发生过停摆,此时时间已经不准
        APP_PRINTF("VLF = 1, oscillator stop / low voltage detected\n");
    }
    // 清除寄存器标志位
    if (!bsp_i2c_write(I2C0, RTC_ADDR, STATE_REG, 0x00)) {
        APP_ERROR("Failed to clear STATE_REG");
    }
    // 回读寄存器标志位
    uint8_t new_status = 0;
    bsp_i2c_read(I2C0, RTC_ADDR, STATE_REG, &new_status);
    if (BIT0(new_status) || BIT1(new_status)) {
        APP_ERROR("STATE flags not cleared, status=0x%02X", new_status);
    } else {
        APP_PRINTF("STATE flags cleared successfully\n");
    }

    app_eventbus_publish(EVENT_STANDARD_TIME, NULL);
    app_rtc_display_set_time();
}

// 获取 nuix 类型时间
void rct_get_unix_time(uint32_t *time)
{
    rtc_ex_time_t t;
    rtc_ex_get_time(&t);
    app_rtc_date_to_unix(&t, time);
}

// 获取结构体类型的时间
void rtc_get_struct_time(rtc_ex_time_t *t)
{
    if (t == NULL) {
        return;
    }
    rtc_ex_get_time(t);
}

// 通过 nuix 设置时间
void rct_set_unix_time(const uint32_t time)
{
    APP_PRINTF("rct_set_unix_time\n");
    rtc_ex_time_t t;
    app_rtc_unix_to_date(time, &t);
    rtc_ex_set_time(&t);

    app_rtc_display_set_time();
}

static void rtc_ex_set_time(rtc_ex_time_t *t)
{
    bsp_i2c_write(I2C0, RTC_ADDR, CTRL_REG, 0x80); // 停止RTC
    bsp_i2c_write(I2C0, RTC_ADDR, SEC_REG, DEC_TO_BCD(t->sec));
    bsp_i2c_write(I2C0, RTC_ADDR, MIN_REG, DEC_TO_BCD(t->min));
    bsp_i2c_write(I2C0, RTC_ADDR, HOUR_REG, DEC_TO_BCD(t->hour));
    bsp_i2c_write(I2C0, RTC_ADDR, DAY_REG, DEC_TO_BCD(t->day));
    bsp_i2c_write(I2C0, RTC_ADDR, MONTH_REG, DEC_TO_BCD(t->month));
    bsp_i2c_write(I2C0, RTC_ADDR, YEAR_REG, DEC_TO_BCD((uint8_t)(t->year % 100)));

    bsp_i2c_write(I2C0, RTC_ADDR, CTRL_REG, 0x00); // 启动RTC
}

static void rtc_ex_get_time(rtc_ex_time_t *t)
{
    uint8_t sec, min, hour, day, month, year;

    // 读取寄存器
    bsp_i2c_read(I2C0, RTC_ADDR, SEC_REG, &sec);
    bsp_i2c_read(I2C0, RTC_ADDR, MIN_REG, &min);
    bsp_i2c_read(I2C0, RTC_ADDR, HOUR_REG, &hour);
    bsp_i2c_read(I2C0, RTC_ADDR, DAY_REG, &day);
    bsp_i2c_read(I2C0, RTC_ADDR, MONTH_REG, &month);
    bsp_i2c_read(I2C0, RTC_ADDR, YEAR_REG, &year);

    t->sec   = BCD_TO_DEC(sec);
    t->min   = BCD_TO_DEC(min);
    t->hour  = BCD_TO_DEC(hour);
    t->day   = BCD_TO_DEC(day);
    t->month = BCD_TO_DEC(month);
    t->year  = BCD_TO_DEC(year) + 2000;
}

// 设置显示屏时间
static void app_rtc_display_set_time(void)
{
    rtc_ex_time_t t;
    rtc_ex_get_time(&t);
    APP_PRINTF("hour:%d min:%d sec:%d\n", t.hour, t.min, t.sec);

    uint16_t time_arr[6];
    time_arr[0] = t.year % 100;
    time_arr[1] = t.month;
    time_arr[2] = t.day;
    time_arr[3] = t.hour;
    time_arr[4] = t.min;
    time_arr[5] = t.sec;

    app_display_set_time(time_arr, sizeof(time_arr) / sizeof(time_arr[0])); // 同步到显示屏
}

// 将 unix 时间转换为 rtc_ex_time_t 结构体
static void app_rtc_unix_to_date(uint32_t timestamp, rtc_ex_time_t *t)
{
    const uint16_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    timestamp += 8 * 3600; // 转换为北京时间
    uint32_t days = timestamp / 86400;
    uint32_t secs = timestamp % 86400;

    t->hour = secs / 3600;
    secs %= 3600;
    t->min        = secs / 60;
    t->sec        = secs % 60;
    uint16_t year = 1970;
    while (1) {
        uint16_t days_in_year = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) ? 366 : 365;
        if (days >= days_in_year) {
            days -= days_in_year;
            year++;
        } else {
            break;
        }
    }
    t->year       = year;
    uint8_t month = 0;
    while (1) {
        uint8_t dim = days_in_month[month];

        if (month == 1) {
            if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
                dim = 29;
        }
        if (days >= dim) {
            days -= dim;
            month++;
        } else {
            break;
        }
    }
    t->month = month + 1;
    t->day   = days + 1;
}

// 将 rtc_ex_time_t 结构体转换为 unix 时间戳
static void app_rtc_date_to_unix(rtc_ex_time_t *t, uint32_t *timestamp)
{
    const uint16_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint32_t days                  = 0;

    for (uint16_t year = 1970; year < t->year; year++) {
        days += ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) ? 366 : 365;
    }

    for (uint8_t month = 0; month < t->month - 1; month++) {
        uint8_t dim = days_in_month[month];
        // 闰年2月
        if (month == 1 && ((t->year % 4 == 0 && t->year % 100 != 0) || (t->year % 400 == 0))) {
            dim = 29;
        }
        days += dim;
    }

    days += (t->day - 1);

    uint32_t ts = days * 86400 + t->hour * 3600 + t->min * 60 + t->sec;

    ts -= 8 * 3600;

    *timestamp = ts;
}
