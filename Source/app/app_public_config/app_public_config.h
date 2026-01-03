#ifndef _APP_PUBLIC_CONFIG_H_
#define _APP_PUBLIC_CONFIG_H_

#include <stdint.h>

#define PANEL_INFO_SIZE     40 // 面板配置信息字节数
#define BIND_INFO_SIZE      75 // 绑定信息的字节数

#define BIND_INFO_DONE_SIZE 16 // 绑定信息完成字节数

#define PANEL_DEV_MAX       32 // 面板最大个数
#define KEY_NUMBER          6  // 按键最大个数

#define RELAY_NUM_MAX       9
#define LED_NUM_MAX         64

// 用于存放面板单个按键的配置信息
typedef struct
{
    uint8_t func;        // 按键功能
    uint8_t group;       // 双控分组
    uint8_t area;        // 按键区域(高4位:总开关分区,低4位:场景分区)
    uint8_t perm;        // 按键权限
    uint8_t scene_group; // 场景分组
} key_panel_t;

// 用于存放单个面板的配置信息
typedef struct
{
    uint8_t addr;
    key_panel_t key_cfg[KEY_NUMBER];
} panel_info_t;

// 用于存放单个按键的绑定信息
typedef struct
{
    uint8_t led_bind[LED_NUM_MAX];     // 绑定的led
    uint8_t relay_bind[RELAY_NUM_MAX]; // 绑定的relay
} key_bind_t;

typedef struct
{
    key_bind_t key_bind[KEY_NUMBER];
} bind_info_t;

const bind_info_t *app_public_get_bind(uint8_t panel_num);
const panel_info_t *app_public_get_cfg(uint8_t panel_num);

void app_public_cfg_init(void);
void app_set_bind_cfg(const uint8_t *cfg, uint16_t len);
void app_set_panel_cfg(const uint8_t *cfg, uint16_t len);
const uint8_t app_public_get_panel_count(void);

#endif