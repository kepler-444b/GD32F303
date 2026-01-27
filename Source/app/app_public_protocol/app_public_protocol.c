#include "app_public_protocol.h"
#include "../Source/app/app_base/app_base.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"
#include "../Source/app/app_evenbus/app_eventbus.h"
#include "../Source/app/app_protocol/app_protocol.h"
#include "systick.h"
#include <stdbool.h>
#include <string.h>

#define PANEL_FH1 0xFF
#define PANEL_FH2 0xAA

// 函数声明
static void app_panel_protocol_check(usart2_rx_buf_t *buf);
static void app_bulid_panel_frame(void);
static void app_build_extend_frame(void);
static void app_public_event_handler(event_type_e event, void *params);

static void app_panel_protocol_parse(const key_panel_t *src_cfg, const panel_src_info_t *p_status);
static void app_public_bind_cfg_parse(const char *str);
static void app_public_panel_cfg_parse(const char *str);

static void app_set_extend_table(uint8_t panel_num, uint8_t key_num, bool key_status);
static void app_set_panel_table(uint8_t sub_idx, uint8_t addr_idx, uint8_t key_num, bool key_status);

static void panel_light_mode(const key_panel_t *p_cfg, const panel_src_info_t *p_status);

static panel_full_status_t my_panel_full_status; //  存储面板状态信息
static panel_src_info_t my_panel_status;         //  面板上报数据

static extend_status_t my_extend_status_t;

static uint8_t panel_count;

void app_public_protocol_init(void)
{
    app_protocol_init(); // 初始化串口

    app_public_cfg_init();

    panel_count = app_public_get_panel_count();
    app_eventbus_subscribe(app_public_event_handler);
}

static void app_public_event_handler(event_type_e event, void *params)
{
    switch (event) {
        case EVENT_USART_RECV_MSG: {
            usart2_rx_buf_t *frame = (usart2_rx_buf_t *)params;
            app_panel_protocol_check(frame);
        } break;
        case MQTT_BIND_CFG_MSG: { // 处理绑定信息
            app_public_bind_cfg_parse((const char *)params);

        } break;
        case MQTT_PANEL_CFG_MSG: { // 处理配置信息
            app_public_panel_cfg_parse((const char *)params);
        } break;
        default:
            break;
    }
}

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

    my_panel_status.src_addr = buf->buffer[3]; // src_addr
    my_panel_status.status   = buf->buffer[5]; // status
    my_panel_status.key_num  = buf->buffer[6]; // key_num

    // 根据上报的 status 和 key_num 计算出上报按键的状态
    my_panel_status.key_status = (my_panel_status.status >> my_panel_status.key_num) & 0x01;

    const key_panel_t *src_cfg = NULL; // 获取键位配置信息
    for (uint8_t panel_num = 0; panel_num < panel_count; panel_num++) {
        const panel_info_t *all_cfg = app_public_get_cfg(panel_num);
        if (all_cfg == NULL) {
            continue;
        }

        if (all_cfg->addr == my_panel_status.src_addr) { // 匹配到面板配置
            APP_PRINTF("cfg->addr:%02X panel_addr:%02X\n", all_cfg->addr, my_panel_status.src_addr);
            src_cfg = &all_cfg->key_cfg[my_panel_status.key_num];
            break;
        }
    }

    if (src_cfg == NULL) {
        APP_ERROR("panel addr not found: %02X", my_panel_status.src_addr);
        return;
    }

    for (uint8_t panel_num = 0; panel_num < panel_count; panel_num++) {
        const key_panel_t *cfg = app_public_get_cfg(panel_num)->key_cfg;
        for (uint8_t key_num; key_num < KEY_NUMBER; key_num++) {
            if (src_cfg->func == cfg->func) { // 匹配功能
            }
        }
    }

    app_panel_protocol_parse(src_cfg, &my_panel_status);
}

static void app_panel_protocol_parse(const key_panel_t *src_cfg, const panel_src_info_t *p_status)
{
    switch (src_cfg->func) {
        // case ALL_CLOSE: // 总关
        //     panel_all_close(frame, cfg);
        //     break;
        // case ALL_ON_OFF: // 总开关
        //     panel_all_on_off(frame, cfg);
        //     break;
        // case CLEAN_ROOM: // 清理
        //     panel_clean_room(frame, cfg);
        //     break;
        // case DND_MODE: // 勿扰
        //     panel_dnd_mode(frame, cfg);
        //     break;
        // case LATER_MODE: // 请稍后
        //     panel_later_mode(frame, cfg);
        //     break;
        // case CHECK_OUT: // 退房
        //     panel_chect_out(frame, cfg);
        //     break;
        // case SOS_MODE: // 紧急呼叫
        //     panel_sos_mode(frame, cfg);
        //     break;
        // case SERVICE: // 请求服务
        //     panel_service(frame, cfg);
        //     break;
        // case CURTAIN_OPEN: // 窗帘开
        //     panel_curtain_open(frame, cfg);
        //     break;
        // case CURTAIN_CLOSE: // 窗帘关
        //     panel_curtain_close(frame, cfg);
        //     break;
        // case SCENE_MODE: // 场景模式
        //     panel_scene_mode(frame, cfg);
        //     break;
        case LIGHT_MODE: // 灯控模式
            panel_light_mode(src_cfg, p_status);
            break;
            // case NIGHT_LIGHT: // 夜灯模式
            //     panel_night_light(frame, cfg);
    }
    app_bulid_panel_frame();
    app_build_extend_frame();
}

static void panel_light_mode(const key_panel_t *src_cfg, const panel_src_info_t *src_info)
{
    for (uint8_t panel_num = 0; panel_num < panel_count; panel_num++) {
        const panel_info_t *p_cfg = app_public_get_cfg(panel_num); // 匹配面板配置

        for (uint8_t key_num = 0; key_num < KEY_NUMBER; key_num++) {

            const key_panel_t *k_cfg = &p_cfg->key_cfg[key_num]; // 匹配到按键配置

            bool func_match  = (src_cfg->func == LIGHT_MODE);
            bool group_match = (src_cfg->group == k_cfg->group || src_cfg->group == 0xFF);
            bool skip_group  = (src_cfg->group == 0x00); // 若双控分组为0,则不双控

            bool special = ((BIT2(src_cfg->perm) && BIT4(src_cfg->perm)) && BIT4(k_cfg->perm));

            if (!func_match || !group_match) {
                continue;
            }

            uint8_t sub_idx  = p_cfg->addr / 8; // 在哪个 sub_frame 中
            uint8_t addr_idx = p_cfg->addr % 8; // 在该 sub_frame 中的第几个地址

            if (skip_group) { // 跳过(只控制自己)

                if ((src_info->src_addr == p_cfg->addr) && (src_info->key_num == key_num)) {
                    my_panel_full_status.sub_frame[sub_idx].addr[addr_idx] = src_info->status;
                    app_set_extend_table(panel_num, key_num, src_info->key_status);
                }
            } else { // 双控

                app_set_panel_table(sub_idx, addr_idx, src_info->key_num, src_info->key_status);
                app_set_extend_table(panel_num, key_num, src_info->status);
            }
        }
    }
}

// 设置 led 和 relay 状态表
static void app_set_extend_table(uint8_t panel_num, uint8_t key_num, bool key_status)
{

    const key_bind_t *k_bind = &app_public_get_bind(panel_num)->key_bind[key_num];

    for (uint8_t i = 0; i < LED_NUM_MAX; i++) {

        uint8_t led_bind = k_bind->led_bind[i];

        if (!BIT7(led_bind) || (k_bind->led_bind[0] == 0xFF)) { // 未勾选该 LED,跳过
            continue;
        }

        uint8_t brightness = led_bind & 0x7F;             // 亮度值只保留低7位
        uint8_t out_value  = key_status ? brightness : 0; // 开:输出亮度值;关:输出0

        if (BIT7(k_bind->led_bind[i])) { // 查看该按键勾选了哪些 LED
            uint8_t brightness = k_bind->led_bind[i] & 0x7F;
            if (i < 4) {
                my_extend_status_t.led_sel_1[i] = out_value;
            } else if (i < 12) {
                my_extend_status_t.led_sel_2[i - 4] = out_value;
            } else if (i < 24) {
                my_extend_status_t.led_sel_3[i - 12] = out_value;
            } else if (i < 32) {
                my_extend_status_t.led_sel_4[i - 24] = out_value;
            }
        }
    }

    for (uint8_t i = 0; i < RELAY_NUM_MAX; i++) {
        uint8_t relay_sel = k_bind->relay_bind[i]; // 需要处理的继电器掩码

        if ((relay_sel == 0) || (relay_sel == 0xFF)) { // 未勾选该 relay,跳过
            continue;
        }
        if (i < 5) {
            if (key_status) {
                my_extend_status_t.relay_sel_1[i] |= relay_sel; // 对应位置1
            } else {
                my_extend_status_t.relay_sel_1[i] &= ~relay_sel; // 对应位清0
            }
        } else if (i < 8) {
            if (key_status) {
                my_extend_status_t.relay_sel_2[i - 5] |= relay_sel;
            } else {
                my_extend_status_t.relay_sel_2[i - 5] &= ~relay_sel;
            }
        }
    }
    APP_PRINTF_BUF("relay_sel_1", &my_extend_status_t.relay_sel_1, sizeof(my_extend_status_t.relay_sel_1));
    APP_PRINTF_BUF("led_sel_1", &my_extend_status_t.led_sel_1, sizeof(my_extend_status_t));
}

// 设置面板状态表
static void app_set_panel_table(uint8_t sub_idx, uint8_t addr_idx, uint8_t key_num, bool key_status)
{
    if (key_status) {
        my_panel_full_status.sub_frame[sub_idx].addr[addr_idx] |= (1 << key_num);

    } else {
        my_panel_full_status.sub_frame[sub_idx].addr[addr_idx] &= ~(1 << key_num);
    }
}

static void app_public_bind_cfg_parse(const char *str)
{
    static uint8_t bind_info[BIND_INFO_SIZE];
    memset(bind_info, 0, BIND_INFO_SIZE);
    uint16_t buf_len = app_string_to_bytes(str, bind_info, BIND_INFO_SIZE);

    app_set_bind_cfg(bind_info, buf_len);
}

static void app_public_panel_cfg_parse(const char *str)
{
    static uint8_t panel_info[PANEL_INFO_SIZE];
    memset(panel_info, 0, PANEL_INFO_SIZE);
    uint16_t buf_len = app_string_to_bytes(str, panel_info, PANEL_INFO_SIZE);

    app_set_panel_cfg(panel_info, PANEL_INFO_SIZE);
}

// 构造发送到面板的数据帧
static void app_bulid_panel_frame(void)
{
    static panel_tx_buf_t my_panel_tx_buf;
    memset(&my_panel_tx_buf, 0, sizeof(my_panel_tx_buf));

    my_panel_tx_buf.fh_1       = PANEL_FRAME_TX_HEAD_1;
    my_panel_tx_buf.fh_2       = PANEL_FRAME_TX_HEAD_2;
    my_panel_tx_buf.type       = PANEL_FRAME_TX_TYPE;
    my_panel_tx_buf.length     = sizeof(my_panel_full_status);
    my_panel_tx_buf.full_frame = my_panel_full_status;
    my_panel_tx_buf.crc        = app_panel_frame_crc((uint8_t *)my_panel_full_status.sub_frame, sizeof(my_panel_full_status.sub_frame));
    my_panel_tx_buf.ft_1       = 0x0D;
    my_panel_tx_buf.ft_2       = 0x0A;

    // APP_PRINTF_BUF("panel_tx_buf", &my_panel_tx_buf, sizeof(my_panel_tx_buf));
    bsp_usart_tx_buf((uint8_t *)&my_panel_tx_buf, sizeof(my_panel_tx_buf), USART2);
}

static void app_build_extend_frame(void)
{
    static extend_tx_buf_t extend_tx_buf;
    memset(&extend_tx_buf, 0, sizeof(extend_tx_buf));

    extend_tx_buf.fh   = EXTEND_FRAME_TX_HEAD;
    extend_tx_buf.type = EXTEND_FRAME_TX_TYPE;

    memcpy(extend_tx_buf.relay_sel_1, my_extend_status_t.relay_sel_1, sizeof(my_extend_status_t.relay_sel_1)); // 0~6路继电器
    memcmp(extend_tx_buf.tg_value, my_extend_status_t.tg_value, sizeof(extend_tx_buf.tg_value));               // 0~4路可控硅调光
    extend_tx_buf.crc_1 = app_panel_frame_sum(&extend_tx_buf.fh, 12);

    memcpy(extend_tx_buf.led_sel_1, my_extend_status_t.led_sel_1, sizeof(extend_tx_buf.led_sel_1)); // 0~4路 LED
    extend_tx_buf.reserve = 0x00;
    extend_tx_buf.crc_2   = app_panel_frame_crc(extend_tx_buf.led_sel_1, sizeof(extend_tx_buf.led_sel_1));

    memcpy(extend_tx_buf.led_sel_2, my_extend_status_t.led_sel_2, sizeof(my_extend_status_t.led_sel_2)); // 5~16路 LED
    extend_tx_buf.crc_3 = app_panel_frame_crc(extend_tx_buf.led_sel_2, sizeof(extend_tx_buf.led_sel_2) - 1);

    memcpy(extend_tx_buf.led_sel_3, my_extend_status_t.led_sel_3, sizeof(extend_tx_buf.led_sel_3));       // 17 ~24路 LED
    memcmp(extend_tx_buf.air_dev, my_extend_status_t.air_dev, sizeof(extend_tx_buf.air_dev));             // 空调模块
    memcmp(extend_tx_buf.relay_sel_2, my_extend_status_t.relay_sel_2, sizeof(extend_tx_buf.relay_sel_2)); // 7~9路继电器
    memcmp(extend_tx_buf.led_sel_4, my_extend_status_t.led_sel_4, sizeof(extend_tx_buf.led_sel_4));       // 25~32路 LED

    APP_PRINTF_BUF("extend_tx_buf", (uint8_t *)&extend_tx_buf, sizeof(extend_tx_buf));
    bsp_usart_tx_buf((uint8_t *)&extend_tx_buf, sizeof(extend_tx_buf), UART4);
}