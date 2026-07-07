#include "app_public_protocol.h"
#include "../Source/app/app_base/app_base.h"
#include "../Source/app/app_timer/app_timer.h"
#include "../Source/app/app_rtc/app_rtc.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"
#include "../Source/app/app_evenbus/app_eventbus.h"
#include "../Source/app/app_protocol/app_protocol.h"
#include "../Source/app/app_public_protocol/app_public.h"
#include "systick.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

// 函数声明
static void app_panel_protocol_check(usart2_rx_buf_t *buf);
static void app_build_panel_frame_by_sid(uint8_t id);
static void app_build_extend_frame_by_sid(uint8_t id);
static void app_build_extend_frame_by_gaddr(uint8_t addr, uint8_t lum);

static void app_public_exe_scene_by_sid(uint8_t scene_id);
static void app_build_extend_frame(void);
static void app_build_panel_frame(void);

static void app_public_event_handler(event_type_e event, void *params);
static void app_public_timer_task(void *arg);

static panel_full_status_t my_panel_full_status; //  存储设备状态信息
static panel_src_info_t my_panel_status;         //  面板上报数据

static panel_all_status_t my_panel_all_status;
static extend_all_status_t my_extend_all_status_t; // 所有扩展设备的状态

static extend_tx_buf_t my_extend_tx_buf; // 发送给扩展设备的数据帧
static panel_tx_buf_t my_panel_tx_buf;   // 发送给面板设备的数据帧

void app_public_protocol_init(void)
{
    app_uart_init_all();   // 初始化设备所用串口
    app_public_cfg_init(); // 加载设备信息
    app_timer_start(30000, app_public_timer_task, true, NULL, "timer_task");
    app_eventbus_subscribe(app_public_event_handler);
}

// 定时任务回调函数
static void app_public_timer_task(void *arg)
{
    rtc_ex_time_t now_time;
    rtc_get_struct_time(&now_time);
    const timer_task_t *temp_task = app_public_get_timer_task();

    for (uint8_t i = 0; i < TIMER_TASK_MAX; i++) {
        if (temp_task[i].hour == now_time.hour &&
            temp_task[i].min == now_time.min &&
            temp_task[i].enable == true) {
            app_public_exe_scene_by_sid(temp_task[i].scene_id);
            APP_PRINTF("timer task exe id:%d scene_id:%d", i, temp_task[i].scene_id);
            return;
        }
    }
}

static void app_public_event_handler(event_type_e event, void *params)
{
    switch (event) {
        case EVENT_USART2_RECV_MSG: {
            usart2_rx_buf_t *frame = (usart2_rx_buf_t *)params;
            app_panel_protocol_check(frame);
        } break;
        case EVENT_USART0_GET_TIMER: {
            const timer_task_t *temp = app_public_get_timer_task();
            if (temp != NULL) {

                uint8_t tx_buf[TIMER_TASK_MAX * 6]; // 1(index)+5字段
                uint32_t offset = 0;

                for (uint8_t i = 0; i < TIMER_TASK_MAX; i++) {

                    tx_buf[offset++] = i; // index
                    tx_buf[offset++] = temp[i].scene_id;
                    tx_buf[offset++] = temp[i].enable;
                    tx_buf[offset++] = temp[i].hour;
                    tx_buf[offset++] = temp[i].min;
                    tx_buf[offset++] = temp[i].reserve;
                }

                app_usart0_build(GET_TIMER, tx_buf, offset);
            }
        } break;
        case EVENT_USART0_SET_TIMER: {
            type_c_rx_t *temp = (type_c_rx_t *)params;
            app_set_timer_task_cfg(temp->buffer, temp->length);
        }
        case EVENT_USART0_CAL_TIME: {
            type_c_rx_t *temp  = (type_c_rx_t *)params;
            uint32_t unix_time = (uint32_t)strtoul((const char *)temp->buffer, NULL, 10);
            rct_set_unix_time(unix_time);
        } break;
        default:
            break;
    }
}

// 处理面板发来的信息
static void app_panel_protocol_check(usart2_rx_buf_t *buf)
{
    APP_PRINTF_BUF("buf", buf->buffer, buf->length);

    if (buf->buffer[0] != PANEL_FRAME_RX_HEAD) {
        APP_ERROR("panel frame");
        return;
    }
    if (buf->length > USART2_RECV_SIZE) {
        APP_ERROR("panel frame too long");
        return;
    }
    if (app_panel_frame_crc(&buf->buffer[3], buf->buffer[2]) != buf->buffer[9]) {
        APP_ERROR("panel frame crc");
        return;
    }
    uint8_t data_type = buf->buffer[1]; // 数据类型(按键:0x01,旋钮:0x02)

    my_panel_status.src_addr  = buf->buffer[3]; // 面板地址
    my_panel_status.status    = buf->buffer[5]; // 面板类型
    my_panel_status.key_num   = buf->buffer[6]; // 按键号
    my_panel_status.reserve_1 = buf->buffer[7]; // 保留/旋钮数据

    switch (data_type) {
        case KNOB: {
            const bind_group_t *binds       = app_public_get_bind_group();
            const uint8_t active_group_bind = app_public_get_active_group_bind();

            static uint8_t last_lum[BIND_GROUP_MAX] = {0};

            for (uint8_t i = 0; i < active_group_bind; i++) {
                if (binds->addr == binds[i].addr) { // 匹配上绑定的群组
                    app_build_extend_frame_by_gaddr(binds[i].addr, my_panel_status.reserve_1);
                    app_display_resource_icon();
                }
            }
        } break;
        case KEY: {
            // 根据上报的 面板状态 和 按键号 计算出上报按键的状态
            bool key_status                 = (my_panel_status.status >> my_panel_status.key_num) & 0x01;
            const bind_scene_t *binds       = app_public_get_bind_scene();         // 获取绑定信息
            const uint8_t active_scene_bind = app_ppublic_get_active_scene_bind(); // 获取激活的绑定信息条目

            for (uint8_t i = 0; i < active_scene_bind; i++) {
                if (binds[i].addr == my_panel_status.src_addr &&
                    binds[i].key_num == my_panel_status.key_num &&
                    binds[i].status == key_status &&
                    binds[i].scene_id != 0xFF) {
                    app_public_exe_scene_by_sid(binds[i].scene_id);
                }
            }
        } break;
        default:
            return;
    }
}

// 根据场景id构造面板数据帧
static void app_build_panel_frame_by_sid(uint8_t id)
{
    const scene_id_t *scenes = app_public_get_scene();
    uint8_t active_scene     = app_public_get_active_scene();

    // 遍历所有激活场景
    for (uint8_t i = 0; i < active_scene; i++) {
        if (scenes[i].id != id) continue; // 如果 id 不匹配, 跳过

        // 遍历每个面板
        for (uint8_t j = 0; j < PANEL_DEV_MAX; j++) {
            // 遍历 0~5 位
            uint8_t sub_idx  = j / 8; // 在哪个 sub_frame 中
            // uint8_t addr_idx = j % 8; // 在该 sub_frame 中的第几个地址

            for (uint8_t bit = 0; bit <= 5; bit++) {
                // 如果 ctrl 的该位被勾选,则赋值 status 对应位
                if (scenes[i].key_ctrl[j] & (1U << bit)) {

                    my_panel_full_status.sub_frame[sub_idx].idx[j].status &= ~(1U << bit);
                    my_panel_full_status.sub_frame[sub_idx].idx[j].status |= (scenes[i].key_status[j] & (1U << bit));
                    APP_PRINTF_BUF("panel_status", &my_panel_full_status, sizeof(my_panel_full_status));
                }
            }
            if (BIT7(scenes[i].key_reserve[j])) { // 是否控制该旋钮

                my_panel_full_status.sub_frame[sub_idx].idx[j].status |= (1U << 6); // 将 status 的 bit6 置1,表示key_reserve中的数据为旋钮值
                my_panel_full_status.sub_frame[sub_idx].idx[j].reserve = scenes[i].key_reserve[j] & 0x7F;
            }
        }
        app_build_panel_frame();
        break;
    }
}

// 根据场景id构造扩展数据帧
static void app_build_extend_frame_by_sid(uint8_t id)
{
    const scene_id_t *scenes = app_public_get_scene();
    uint8_t active_scene     = app_public_get_active_scene(); // 获取活跃的场景数量

    for (uint8_t i = 0; i < active_scene; i++) {
        if (scenes[i].id != id) continue;

        // 处理 LED 部分
        for (uint8_t j = 0; j < LED_NUM_MAX; j++) {
            if (!(scenes[i].led[j] & 0x80)) continue;

            uint8_t val    = scenes[i].led[j] & 0x7F; // 读取实际的亮度值
            uint8_t *p_led = app_get_led_by_num(&my_extend_all_status_t, j);
            if (p_led) *p_led = val;
        }

        // 将 relay_sel_1 的首地址作为 9 字节缓冲区的起点
        uint8_t *p_relay_base = my_extend_all_status_t.relay_sel_1;

        for (uint8_t g = 0; g < 9; g++) {
            uint8_t ctrl   = scenes[i].relay[g * 2];     // 控制位字节
            uint8_t status = scenes[i].relay[g * 2 + 1]; // 状态位字节

            if (ctrl == 0) continue; // 该组无变化，跳过

            // 使用掩码位运算：保留(非控制位) + 应用(控制位 & 状态值)
            p_relay_base[g] = (p_relay_base[g] & ~ctrl) | (status & ctrl);
        }

        app_build_extend_frame();
        break;
    }
}

// 根据群组地址构造扩展数据帧
static void app_build_extend_frame_by_gaddr(uint8_t addr, uint8_t lum)
{
    const bind_group_t *groups = app_public_get_bind_group();
    uint8_t active_group       = app_public_get_active_group_bind();

    for (uint8_t i = 0; i < active_group; i++) {

        if (groups[i].addr != addr) continue;

        for (uint8_t j = 0; j < 8; j++) {
            for (uint8_t bit = 0; bit < 8; bit++) {

                if (groups[i].ctrls[j] & (1U << bit)) {

                    uint8_t led = j * 8 + bit;
                    if (led <= 3) {
                        my_extend_all_status_t.led_sel_1[led] = lum;
                    }
                }
            }
        }

        app_build_extend_frame();
        break;
    }
}

// 根据场景id执行场景
static void app_public_exe_scene_by_sid(uint8_t scene_id)
{
    app_build_panel_frame_by_sid(scene_id);  // 根据场景id构造面板数据帧
    app_build_extend_frame_by_sid(scene_id); // 根据场景id构造扩展数据帧

    app_display_scene_icon(scene_id); // 控制显示屏的场景页面
    app_display_resource_icon();      // 控制显示屏的资源页面
}

// 执行场景
void app_public_exe_scene(const char *str)
{
    uint8_t scene_id = 0xFF;
    uint16_t buf_len = app_string_to_bytes(str, &scene_id, sizeof(scene_id));
    if (scene_id > SCENE_ID_MAX) {
        APP_ERROR("scene id is too large");
        return;
    }
    app_public_exe_scene_by_sid(scene_id);
}

// 显示屏控制执行场景
void app_display_exe_scene(uint8_t scene_id)
{
    if (scene_id > SCENE_ID_MAX) {
        APP_ERROR("scene id is too large");
        return;
    }
    app_public_exe_scene_by_sid(scene_id);
}

// 删除配置信息
void app_public_del_cfg(const char *str)
{
    if (strcmp(str, "DelScene") == 0) { // 删除场景信息
        app_del_scene_cfg();
    } else if (strcmp(str, "DelBind") == 0) { // 删除绑定信息
        app_del_bind_cfg();
    }
}

// 绑定场景
void app_public_bind_scene_cfg_parse(const char *str)
{
    static uint8_t bind_info[BIND_SCENE_INFO_SIZE];
    uint16_t buf_len = app_string_to_bytes(str, bind_info, BIND_SCENE_INFO_SIZE);
    app_set_bind_scene_cfg(bind_info, buf_len);
}

// 绑定群组
void app_public_bind_group_cfg_parse(const char *str)
{
    static uint8_t bind_info[BIND_GROUP_INFO_SIZE];
    uint16_t buf_len = app_string_to_bytes(str, bind_info, BIND_GROUP_INFO_SIZE);
    app_set_bind_group_cfg(bind_info, buf_len);
}

// 设置场景
void app_public_scene_cfg_parse(const char *str)
{
    static uint8_t scene_info[SCENE_INFO_SIZE];
    uint16_t buf_len = app_string_to_bytes(str, scene_info, SCENE_INFO_SIZE);
    app_set_scene_cfg(scene_info, SCENE_INFO_SIZE);
}

// 设置定时任务
void app_public_timer_task_cfg_parse(const char *str)
{
    static uint8_t timer_task_info[TIMER_TASK_INFO_SIZE];
    uint16_t buf_len = app_string_to_bytes(str, timer_task_info, TIMER_TASK_INFO_SIZE);
    app_set_timer_task_cfg(timer_task_info, TIMER_TASK_INFO_SIZE);
}

// 设置扩展状态
void app_puublic_set_extend(const char *str)
{
    uint16_t str_len = strlen(str);
    if (str_len != SET_EXTEND_MAX) {
        APP_ERROR("app_puublic_set_extend data");
    }

    char relay_str[72 + 1] = {0};
    char led_str[64 + 1]   = {0};

    snprintf(relay_str, sizeof(relay_str), "%.*s", 72, str);
    snprintf(led_str, sizeof(led_str), "%.*s", 64, str + 72);

    // 测试打印
    printf("relay_str: %s\n", relay_str);
    printf("led_str: %s\n", led_str);

    uint8_t relay_arr[9] = {0};
    uint8_t led_arr[32]  = {0};

    app_pack_bits(relay_str, sizeof(relay_arr), relay_arr);
    app_string_to_bytes(led_str, led_arr, sizeof(led_arr));
    APP_PRINTF_BUF("led_arr", led_arr, sizeof(led_arr));

    memcpy(my_extend_all_status_t.relay_sel_1, relay_arr, 6);
    memcpy(my_extend_all_status_t.relay_sel_2, relay_arr + 6, 3);

    memcpy(my_extend_all_status_t.led_sel_1, led_arr, 4);
    memcpy(my_extend_all_status_t.led_sel_2, led_arr + 4, 12);
    memcpy(my_extend_all_status_t.led_sel_3, led_arr + 16, 8);
    memcpy(my_extend_all_status_t.led_sel_4, led_arr + 24, 8);

    // APP_PRINTF_BUF("relay_sel_1", my_extend_all_status_t.relay_sel_1, sizeof(my_extend_all_status_t.relay_sel_1));
    // APP_PRINTF_BUF("relay_sel_2", my_extend_all_status_t.relay_sel_2, sizeof(my_extend_all_status_t.relay_sel_2));

    // APP_PRINTF_BUF("led_sel_1", my_extend_all_status_t.led_sel_1, sizeof(my_extend_all_status_t.led_sel_1));
    // APP_PRINTF_BUF("led_sel_2", my_extend_all_status_t.led_sel_2, sizeof(my_extend_all_status_t.led_sel_2));
    // APP_PRINTF_BUF("led_sel_3", my_extend_all_status_t.led_sel_3, sizeof(my_extend_all_status_t.led_sel_3));
    // APP_PRINTF_BUF("led_sel_4", my_extend_all_status_t.led_sel_4, sizeof(my_extend_all_status_t.led_sel_4));
    app_build_extend_frame();
}

// 获取扩展状态
extend_all_status_t *app_public_get_extend(void)
{
    return &my_extend_all_status_t;
}

// 构造扩展数据帧
static void app_build_extend_frame(void)
{
    memset(&my_extend_tx_buf, 0, sizeof(my_extend_tx_buf));

    my_extend_tx_buf.fh   = EXTEND_FRAME_TX_HEAD;
    my_extend_tx_buf.type = EXTEND_FRAME_TX_TYPE;

    memcpy(my_extend_tx_buf.relay_sel_1, my_extend_all_status_t.relay_sel_1, sizeof(my_extend_all_status_t.relay_sel_1)); // 0~6路继电器
    memcmp(my_extend_tx_buf.tg_value, my_extend_all_status_t.tg_value, sizeof(my_extend_tx_buf.tg_value));                // 0~4路可控硅调光
    my_extend_tx_buf.crc_1 = app_panel_frame_sum(&my_extend_tx_buf.fh, 12);

    memcpy(my_extend_tx_buf.led_sel_1, my_extend_all_status_t.led_sel_1, sizeof(my_extend_tx_buf.led_sel_1)); // 0~4路 LED
    my_extend_tx_buf.reserve = 0x00;
    my_extend_tx_buf.crc_2   = app_panel_frame_crc(my_extend_tx_buf.led_sel_1, sizeof(my_extend_tx_buf.led_sel_1));

    memcpy(my_extend_tx_buf.led_sel_2, my_extend_all_status_t.led_sel_2, sizeof(my_extend_all_status_t.led_sel_2)); // 5~16路 LED
    my_extend_tx_buf.crc_3 = app_panel_frame_crc(my_extend_tx_buf.led_sel_2, sizeof(my_extend_tx_buf.led_sel_2) - 1);

    memcpy(my_extend_tx_buf.led_sel_3, my_extend_all_status_t.led_sel_3, sizeof(my_extend_tx_buf.led_sel_3));       // 17 ~24路 LED
    memcmp(my_extend_tx_buf.air_dev, my_extend_all_status_t.air_dev, sizeof(my_extend_tx_buf.air_dev));             // 空调模块
    memcmp(my_extend_tx_buf.relay_sel_2, my_extend_all_status_t.relay_sel_2, sizeof(my_extend_tx_buf.relay_sel_2)); // 7~9路继电器
    memcmp(my_extend_tx_buf.led_sel_4, my_extend_all_status_t.led_sel_4, sizeof(my_extend_tx_buf.led_sel_4));       // 25~32路 LED

    APP_PRINTF_BUF("my_extend_tx_buf", (uint8_t *)&my_extend_tx_buf, sizeof(my_extend_tx_buf));

    bsp_usart_tx_buf((uint8_t *)&my_extend_tx_buf, sizeof(my_extend_tx_buf), UART3);
}

// 构造面板数据帧
static void app_build_panel_frame(void)
{
    memset(&my_panel_tx_buf, 0, sizeof(my_panel_tx_buf));

    my_panel_tx_buf.fh_1       = PANEL_FRAME_TX_HEAD_1;
    my_panel_tx_buf.fh_2       = PANEL_FRAME_TX_HEAD_2;
    my_panel_tx_buf.type       = PANEL_FRAME_TX_TYPE;
    my_panel_tx_buf.length     = sizeof(my_panel_full_status);
    my_panel_tx_buf.full_frame = my_panel_full_status;
    my_panel_tx_buf.crc        = app_panel_frame_crc((uint8_t *)my_panel_full_status.sub_frame, sizeof(my_panel_full_status.sub_frame));
    my_panel_tx_buf.ft_1       = 0x0D;
    my_panel_tx_buf.ft_2       = 0x0A;

    APP_PRINTF_BUF("panel_tx_buf", &my_panel_tx_buf, sizeof(my_panel_tx_buf));
    bsp_usart_tx_buf((uint8_t *)&my_panel_tx_buf, sizeof(my_panel_tx_buf), USART2);
}

uint8_t *app_get_led_by_num(extend_all_status_t *status, uint8_t index)
{
    if (status == NULL) return NULL;

    if (index < 4) {
        return &status->led_sel_1[index];
    } else if (index < 16) {
        return &status->led_sel_2[index - 4];
    } else if (index < 24) {
        return &status->led_sel_3[index - 16];
    } else if (index < 32) {
        return &status->led_sel_4[index - 24];
    }
    return NULL;
}