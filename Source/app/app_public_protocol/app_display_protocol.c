#include "app_display_protocol.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"
#include "../Source/app/app_base/app_base.h"
#include "../Source/app/app_rtc/app_rtc.h"
#include "../Source/app/app_public_protocol/app_public.h"
#include "../Source/app/app_public_config/app_public_config.h"
#include "../Source/dev/dev_info.h"
#include "../Source/bsp/bsp_board/bsp_board.h"

#include <string.h>

#define MAIN_MENU_COUNT 4 // 主菜单个数

static const uint16_t menu_scenes_pages[]   = {6, 7, 8, 9}; // 场景菜单
static const uint16_t menu_resource_pages[] = {10, 11};     // 资源状态
static const uint16_t menu_device_pages[]   = {13};         // 设备信息
static const uint16_t menu_version_pages[]  = {12};         // 软件版本

static const menu_item_t main_menu[MAIN_MENU_COUNT] = {
    {2, menu_scenes_pages, 4},   // 0: 场景
    {3, menu_resource_pages, 2}, // 1: 资源状态
    {4, menu_device_pages, 1},   // 2: 设备信息
    {5, menu_version_pages, 1}   // 3: 软件版本
};

static menu_state_t menu = {.level = MENU_LEVEL_HOME};

static bool over_temp_flag = false; // 超温报警状态

// 函数声明
static uint16_t app_display_build_frame(display_data_t *data, uint8_t *tx_buf);
static void app_display_send_page(uint16_t page);
static void app_display_update_page(void);
static void app_display_set_soft_ver(void);

static void app_display_forward(void);
static void app_display_backward(void);
static void app_display_enter(void);
static void app_display_back(void);
static void app_display_wake_up(void);

// ==================== 按键处理表 ====================
typedef void (*key_handler_t)(void);

static const key_handler_t key_handlers[] = {
    app_display_enter,    // 0 - 确认
    app_display_backward, // 1 - 后退/左
    app_display_forward,  // 2 - 前进/右
    app_display_back      // 3 - 返回
};

// ==================== 初始化 ====================
void app_display_init(void)
{
    menu.level      = MENU_LEVEL_GIF;
    menu.main_index = 0;
    menu.sub_index  = 0;
}

// ==================== 按键接收 ====================
void app_display_recv_key(uint8_t key_num)
{
    if (key_num < sizeof(key_handlers) / sizeof(key_handlers[0])) {
        key_handlers[key_num]();
    }
    app_display_wake_up();
}

// ==================== 页面切换核心 ====================
static void app_display_send_page(uint16_t page)
{
    display_data_t disp = {0};
    disp.cmd            = WRITE_CMD;
    disp.has_addr       = true;
    disp.addr           = PAGE_CMD;
    disp.data[0]        = page;
    disp.data_len       = 1;

    uint8_t tx_buf[64];
    uint16_t len = app_display_build_frame(&disp, tx_buf);
    bsp_usart_tx_buf(tx_buf, len, USART1);
}

static void app_display_update_page(void)
{
    app_display_send_page(app_display_get_current_page());
}

uint16_t app_display_get_current_page(void)
{
    if (menu.level == MENU_LEVEL_GIF) return 0;
    if (menu.level == MENU_LEVEL_HOME) return 1;

    const menu_item_t *item = &main_menu[menu.main_index];

    if (menu.level == MENU_LEVEL_MAIN) {
        return item->main_page;
    }
    if (menu.level == MENU_LEVEL_SUB && item->sub_pages && menu.sub_index < item->sub_count) {
        return item->sub_pages[menu.sub_index];
    }

    return 1; // 默认返回 Home 页
}

// 前进
static void app_display_forward(void)
{
    if (over_temp_flag) {
        return;
    }
    if (menu.level == MENU_LEVEL_MAIN) {
        menu.main_index = (menu.main_index + 1) % MAIN_MENU_COUNT;
    } else if (menu.level == MENU_LEVEL_SUB) {
        const menu_item_t *item = &main_menu[menu.main_index];
        if (item->sub_count > 0) {
            menu.sub_index = (menu.sub_index + 1) % item->sub_count;
        }
    }
    app_display_update_page();
}

// 后退
static void app_display_backward(void)
{
    if (over_temp_flag) {
        return;
    }
    if (menu.level == MENU_LEVEL_MAIN) {
        menu.main_index = (menu.main_index + MAIN_MENU_COUNT - 1) % MAIN_MENU_COUNT;
    } else if (menu.level == MENU_LEVEL_SUB) {
        const menu_item_t *item = &main_menu[menu.main_index];
        if (item->sub_count > 0) {
            menu.sub_index = (menu.sub_index + item->sub_count - 1) % item->sub_count;
        }
    }
    app_display_update_page();
}

// 确认
static void app_display_enter(void)
{
    if (over_temp_flag) {
        return;
    }

    switch (menu.level) {
        case MENU_LEVEL_GIF:
            menu.level = MENU_LEVEL_HOME;
            break;

        case MENU_LEVEL_HOME:
            menu.level      = MENU_LEVEL_MAIN;
            menu.main_index = 0;
            break;

        case MENU_LEVEL_MAIN: {
            const menu_item_t *item = &main_menu[menu.main_index];
            if (item->sub_count > 0) {
                menu.level     = MENU_LEVEL_SUB;
                menu.sub_index = 0;
            }
            // 软件版本特殊处理
            if (menu.main_index == 3) {
                app_display_set_soft_ver();
            }
        } break;

        case MENU_LEVEL_SUB:
            app_display_exe_scene(menu.sub_index); // 执行场景
            break;
    }
    app_display_update_page();
}

// 返回
static void app_display_back(void)
{
    if (over_temp_flag) {
        bsp_set_buuzzer(0xFE);
        over_temp_flag = false;
    }

    switch (menu.level) {
        case MENU_LEVEL_SUB:
            menu.level = MENU_LEVEL_MAIN;
            break;

        case MENU_LEVEL_MAIN:
            menu.level = MENU_LEVEL_HOME;
            break;

        case MENU_LEVEL_HOME:
            // menu.level = MENU_LEVEL_GIF; // 不返回 GIF
            break;

        default:
            break;
    }
    app_display_update_page();
}

// 软件版本
static void app_display_set_soft_ver(void)
{
    const dev_save_info_t *info = dev_get_save_device_info();
    const char *ver_str         = info->cur_ver;

    if (*ver_str == 'V' || *ver_str == 'v') ver_str++;

    uint16_t ver_num = 0;
    const char *dot  = strchr(ver_str, '.');
    if (dot) {
        ver_num = (uint16_t)((ver_str[0] - '0') * 10 + (dot[1] - '0'));
    }

    display_data_t disp = {0};
    disp.cmd            = WRITE_CMD;
    disp.has_addr       = true;
    disp.addr           = 0x1201;
    disp.data[0]        = ver_num;
    disp.data_len       = 1;

    uint8_t tx_buf[64];
    uint16_t len = app_display_build_frame(&disp, tx_buf);
    bsp_usart_tx_buf(tx_buf, len, USART1);
}

// 设置设备信息
void app_display_connect_changed(bool connect)
{
    const wiz_NetInfo *nw = dev_get_nw_cfg_info();
    uint8_t tx_buf[64];
    display_data_t disp = {0};

    // 连接状态
    disp.cmd      = WRITE_CMD;
    disp.has_addr = true;
    disp.addr     = 0x1101;
    disp.data[0]  = connect ? 0x0001 : 0x0000;
    disp.data_len = 1;
    bsp_usart_tx_buf(tx_buf, app_display_build_frame(&disp, tx_buf), USART1);

    // IP 地址
    disp.addr    = 0x1102;
    disp.data[0] = 0x1102;
    memcpy(&disp.data[1], nw->ip, 4);
    disp.data_len = 5;
    bsp_usart_tx_buf(tx_buf, app_display_build_frame(&disp, tx_buf), USART1);

    // DHCP 状态
    disp.addr    = 0x1106;
    disp.data[0] = (nw->dhcp == 1) ? 0x0001 : 0x0000;
    bsp_usart_tx_buf(tx_buf, app_display_build_frame(&disp, tx_buf), USART1);
}

// 设置时间
void app_display_set_time(uint16_t *time, uint8_t t_len)
{
    display_data_t disp = {0};
    disp.cmd            = WRITE_CMD;
    disp.has_addr       = true;
    disp.addr           = TIME_CMD;
    memcpy(disp.data, time, t_len * 2);
    disp.data_len = t_len;

    uint8_t tx_buf[64];
    uint16_t len = app_display_build_frame(&disp, tx_buf);
    bsp_usart_tx_buf(tx_buf, len, USART1);
}

// 设置温度
void app_display_set_temp(float temp)
{
    static float last_temp; // 上次温度

    uint16_t new_temp   = (uint16_t)(temp * 100);
    display_data_t disp = {0};
    disp.cmd            = WRITE_CMD;
    disp.has_addr       = true;
    disp.addr           = 0x0923;
    disp.data[0]        = new_temp;
    disp.data_len       = 1;

    uint8_t tx_buf[64];

    uint16_t len = app_display_build_frame(&disp, tx_buf);
    bsp_usart_tx_buf(tx_buf, len, USART1);

    if ((temp >= OVER_TEMP) && (last_temp <= OVER_TEMP)) { // 超温报警

        app_display_send_page(14); // 切换到报警页面
        disp.addr    = 0x0924;
        uint16_t len = app_display_build_frame(&disp, tx_buf);
        bsp_usart_tx_buf(tx_buf, len, USART1);
        bsp_set_buuzzer(0xFF);
        over_temp_flag = true;
    }

    last_temp = temp;
}

// 设置场景
void app_display_scene_icon(uint8_t scene)
{
    display_data_t disp = {0};
    disp.cmd            = WRITE_CMD;
    disp.has_addr       = false;
    disp.data[0]        = 0x092B;
    disp.data_len       = 5;
    if (scene < 4) {
        disp.data[scene + 1] = 0x0001;
    }

    uint8_t tx_buf[64];
    uint16_t len = app_display_build_frame(&disp, tx_buf);
    bsp_usart_tx_buf(tx_buf, len, USART1);
}

// 设置资源界面
void app_display_resource_icon(void)
{
    extend_all_status_t *temp = app_public_get_extend();
    uint8_t tx_buf[128];
    display_data_t disp = {0};

    disp.cmd      = WRITE_CMD;
    disp.has_addr = false;
    disp.data[0]  = 0x0900;

    disp.data_len = 33;
    // 状态位
    for (uint8_t i = 0; i < LED_NUM_MAX; i++) {

        uint8_t *led = app_get_led_by_num(temp, i);
        if (led) {
            disp.data[i + 1] = (*led) ? 0x0001 : 0x0000;
        }
    }
    bsp_usart_tx_buf(tx_buf, app_display_build_frame(&disp, tx_buf), USART1);

    // 百分比
    disp.data[0] = 0x0800;
    for (uint8_t i = 0; i < LED_NUM_MAX; i++) {
        uint8_t *led = app_get_led_by_num(temp, i);
        if (led) {
            disp.data[i + 1] = (*led);
        }
    }
    bsp_usart_tx_buf(tx_buf, app_display_build_frame(&disp, tx_buf), USART1);
}

// 组帧函数
static uint16_t app_display_build_frame(display_data_t *data, uint8_t *tx_buf)
{
    uint16_t index = 0;
    uint8_t length = 1 + (data->has_addr ? 2 : 0) + data->data_len * 2 + 2;

    tx_buf[index++] = 0x5A;
    tx_buf[index++] = 0xA5;
    tx_buf[index++] = length;
    tx_buf[index++] = data->cmd;

    if (data->has_addr) {
        tx_buf[index++] = data->addr >> 8;
        tx_buf[index++] = data->addr & 0xFF;
    }

    for (uint8_t i = 0; i < data->data_len; i++) {
        tx_buf[index++] = data->data[i] >> 8;
        tx_buf[index++] = data->data[i] & 0xFF;
    }

    uint16_t crc    = app_crc_16(&tx_buf[3], index - 3);
    tx_buf[index++] = crc & 0xFF;
    tx_buf[index++] = crc >> 8;

    return index;
}

static void app_display_wake_up(void)
{
    uint8_t tx_buf[10] = {0x5A, 0xA5, 0x07, 0x10, 0x70, 0x01, 0x00, 0x63, 0x6E, 0xDE};
    bsp_usart_tx_buf(tx_buf, 10, USART1);
}