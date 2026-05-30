#include "dev_public_host.h"
#include "systick.h"
#include "../Source/app/app_public_protocol/app_public_protocol.h"
#include "../Source/app/app_public_protocol/app_display_protocol.h"
#include "../Source/app/app_network_model/app_network_model.h"
#include "../Source/app/app_rtc/app_rtc.h"
#include "../Source/app/app_evenbus/app_eventbus.h"
#include "../Source/app/app_timer/app_timer.h"
#include "../Source/bsp/bsp_pcb/bsp_pcb.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"
#include "../Source/dev/dev_info.h"
#include "../Source/bsp/bsp_adc/bsp_adc.h"
#include "../Source/bsp/bsp_board/bsp_board.h"

// 宏定义
#define BUTTON_NUMBER 4
#define ADC_BUF_SIZE  10
#define NO_KEY_VOL    300 // 大于这个认为没有按键

// 函数声明
static void dev_led_blink(void);
static void dev_temp_update(void);
static void dev_standard_time(void);
static void dev_key_check(void);
static void dev_public_timer_cb(void *arg);
static void dev_display_key_check(void);
static void app_sub_device_reset(const char *dev_neme);
static void dev_board_event_handler(event_type_e event, void *params);

// 全局变量
typedef struct {
    uint16_t min;
    uint16_t max;
} vol_range_t;

typedef struct {
    bool is_pressed;
} key_status_t;

static uint16_t adc_buffer[ADC_BUF_SIZE] = {0};
static uint8_t adc_buf_idx               = 0;

static const vol_range_t key_voltage_table[BUTTON_NUMBER] = {
    {0, 33},    // K1
    {34, 111},  // K2
    {112, 206}, // K3
    {207, 280}  // K4
};
static key_status_t keys[BUTTON_NUMBER];

static bool led_blink      = false;
static bool network_status = 0; // 网络连接状态
static bool standard_time  = false;

void dev_public_host(void)
{
    dev_device_info_init();     // 初始化设备信息
    app_public_protocol_init(); // 初始化面板,可扩展协议层
    app_rtc_ex_init();          // RTC模块初始化
    app_network_model_init();   // 初始化网络协议层
    app_sub_device_reset("panel");

    bsp_board_init(); // 板级初始化
    app_timer_start(10, dev_public_timer_cb, true, NULL, "timer_cb");
    app_eventbus_subscribe(dev_board_event_handler);
}

static void dev_board_event_handler(event_type_e event, void *params)
{
    switch (event) {
        case EVENT_USER_LINK_ON:
        case EVENT_PHY_LINK_ON:
            led_blink = false;
            bsp_set_led_status(true);
            bsp_set_buuzzer(1); // 蜂鸣器响一声
            app_display_connect_changed(false);
            break;
        case EVENT_PHY_LINK_OFF:
            BSP_SET_GPIO(PD10, true);
            led_blink = false;
            app_display_connect_changed(false);
            break;
        case EVENT_NETWOR_ON:
            network_status = true;
            led_blink      = false;
            bsp_set_buuzzer(2);
            led_blink = true; // LED 闪烁
            app_display_connect_changed(true);
            break;
        case EVENT_NETWOR_OFF:
            app_network_model_init();
            network_status = false;
            led_blink      = false;
            bsp_set_led_status(false);
            app_display_connect_changed(false);
            break;
        case EVENT_STANDARD_TIME: // 时间校准
            standard_time = true;
            break;
        default:
            break;
    }
}

static void dev_public_timer_cb(void *arg)
{
    dev_led_blink();         // LED闪烁
    dev_key_check();         // 按键检测
    dev_temp_update();       // 主板温度检测
    dev_standard_time();     // 校准时间
    dev_display_key_check(); // 显示屏按键检测
}

// 显示屏按键检测
static void dev_display_key_check(void)
{
    static uint16_t adc_buffer[ADC_BUF_SIZE];
    static uint8_t buf_idx   = 0;
    static uint16_t last_avg = 0xFFFF;

    uint16_t raw_vol = bsp_adc_read_key();
    // APP_PRINTF("raw_vol:%d\n", raw_vol);
    // 无按键
    if (raw_vol > NO_KEY_VOL) {
        if (last_avg != 0xFFFF) {
            for (uint8_t i = 0; i < BUTTON_NUMBER; i++) {
                keys[i].is_pressed = false;
            }
            last_avg = 0xFFFF;
        }
        buf_idx = 0;
        return;
    }

    adc_buffer[buf_idx++] = raw_vol;

    if (buf_idx >= ADC_BUF_SIZE) {
        // 计算平均值
        uint32_t sum = 0;
        for (uint8_t i = 0; i < ADC_BUF_SIZE; i++) {
            sum += adc_buffer[i];
        }
        uint16_t avg_vol = (uint16_t)(sum / ADC_BUF_SIZE);
        if (avg_vol != last_avg) {
            for (uint8_t i = 0; i < BUTTON_NUMBER; i++) {
                if (avg_vol >= key_voltage_table[i].min && avg_vol <= key_voltage_table[i].max) {
                    if (!keys[i].is_pressed) {
                        keys[i].is_pressed = true;
                        app_display_recv_key(i);
                    }
                    break;
                }
            }
            last_avg = avg_vol;
        }
        buf_idx = 0;
    }
}

static void dev_led_blink(void)
{
    static uint16_t led_count = 0;
    static bool led_status;

    if (led_blink) {
        led_count++;
        if (led_count == 50) {
            led_status = !led_status;
            bsp_set_led_status(led_status);
            led_count = 0;
        }
    }
}

// 按键检测
static void dev_key_check(void)
{
    static bool last_key = false;
    bool cur_key         = !BSP_GET_GPIO(PC0);
    if (cur_key != last_key) {
        if (cur_key) {
            app_eventbus_publish(EVENT_USER_LINK_ON, NULL); // 手动联网
        }
        last_key = cur_key;
    }
}

// 更新主板温度
static void dev_temp_update(void)
{
    static uint16_t temp_count = 0;
    temp_count++;
    if (temp_count >= 600) {

        static float cur_temp;  // 当前温度
        static float last_temp; // 上次温度
        cur_temp = bsp_adc_read_temp();
        app_display_set_temp(cur_temp); // 更新到串口屏

        if (((cur_temp - last_temp) >= 0.5f) || ((last_temp - cur_temp) >= 0.5f)) { // 当温度变化超过0.5度,才会上报到云端

            if (network_status == true)
                app_eventbus_publish(EVENT_REPORT_TEMP, (void *)&cur_temp); // 上报到云端
            last_temp = cur_temp;
        }
        temp_count = 0;
    }
}

// 校准时间
static void dev_standard_time(void)
{
    if (standard_time == true && network_status == true) {
        app_eventbus_publish(EVENT_REPORT_GET_TIME, NULL);
        standard_time = false;
    }
}

// 子设备重启
static void app_sub_device_reset(const char *dev_neme)
{
    static uint8_t reset_cmd[7] = {0xFF, 0xAA, 0x01, 0x04, 0x00, 0x0D, 0x0A};
    if (strcmp(dev_neme, "panel") == 0) {
        APP_PRINTF("panel_reset\n");
        reset_cmd[4] = 0x00;
    }
    bsp_usart_tx_buf(reset_cmd, sizeof(reset_cmd), USART2);
}
