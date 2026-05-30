#ifndef _APP_DISPLAY_PROTOCOL_H_
#define _APP_DISPLAY_PROTOCOL_H_

#include <stdint.h>
#include <stdbool.h>

#define MAX_DATA_LEN 64

#define PAGE_CMD     0x7000
#define LUM_CMD      0x7001
#define TIME_CMD     0x7002
#define WRITE_CMD    0x10

#define OVER_TEMP    60 // 超温温度

typedef struct {
    uint8_t cmd;
    bool has_addr;
    uint16_t addr;
    uint16_t data[MAX_DATA_LEN];
    uint8_t data_len;
} display_data_t;

// ==================== 菜单层级定义 ====================
typedef enum {
    MENU_LEVEL_GIF = 0, // gif 启动页面
    MENU_LEVEL_HOME,    // 主页面
    MENU_LEVEL_MAIN,    // 主菜单
    MENU_LEVEL_SUB      // 子菜单
} menu_level_t;

typedef struct {
    uint16_t main_page;        // 主菜单页面号
    const uint16_t *sub_pages; // 子页面数组
    uint8_t sub_count;         // 子页面数量
} menu_item_t;

// 菜单状态
typedef struct {
    menu_level_t level;
    uint8_t main_index;
    uint8_t sub_index;
} menu_state_t;

// ==================== API ====================
void app_display_init(void);
void app_display_recv_key(uint8_t key_num); // 0:确认  1:后退  2:前进  3:返回

void app_display_connect_changed(bool connect);
void app_display_set_time(uint16_t *time, uint8_t len);
void app_display_set_temp(float temp);
void app_display_scene_icon(uint8_t scene);
void app_display_resource_icon(void);

uint16_t app_display_get_current_page(void);

#endif