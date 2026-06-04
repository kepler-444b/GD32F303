#ifndef _APP_PUBLIC_CONFIG_H_
#define _APP_PUBLIC_CONFIG_H_

#include <stdint.h>

#define SCENE_INFO_SIZE      185 // 设置场景信息字节数
#define BIND_SCENE_INFO_SIZE 6   // 绑定场景信息字节数
#define BIND_GROUP_INFO_SIZE 11  // 绑定群组信息字节数
#define TIMER_TASK_INFO_SIZE 60  // 定时任务信息字节数

#define BIND_SCENE_MAX       128 // 绑定场景最大个数
#define SCENE_ID_MAX         128 // 场景ID最大个数
#define BIND_GROUP_MAX       18  // 绑定群组最大个数
#define TIMER_TASK_MAX       10  // 定时任务最大个数

#define PANEL_DEV_MAX        32 // 面板最大个数
#define KEY_NUMBER           6  // 按键最大个数

#define RELAY_NUM_MAX        18
#define LED_NUM_MAX          32

// 定时任务结构体
typedef struct {
    uint8_t scene_id;
    uint8_t enable;
    uint8_t hour;
    uint8_t min;
    uint8_t reserve;
    uint8_t padding;

} timer_task_t;

// 场景结构体
typedef struct {
    uint8_t id;
    uint8_t led[LED_NUM_MAX];     // 64路led状态
    uint8_t relay[RELAY_NUM_MAX]; // 72路继电器状态

    uint8_t key_ctrl[PANEL_DEV_MAX];   // 32个面板控制状态
    uint8_t key_status[PANEL_DEV_MAX]; // 32个面板实际状态

    uint8_t key_reserve[PANEL_DEV_MAX]; // 32个面板保留
} scene_id_t;

// 绑定场景结构体
typedef struct {

    uint8_t addr;     // 设备地址
    uint8_t key_num;  // 按键号
    uint8_t scene_id; // 场景id
    uint8_t status;   // 安装见状态
} bind_scene_t;

// 绑定群组结构体
typedef struct {

    uint8_t addr;     // 设备地址
    uint8_t ctrls[8]; // 被控led使能
    uint8_t close_id; // 关闭场景
    uint8_t open_id;  // 开启场景
    uint8_t padding;  // 保证四字节对齐
} bind_group_t;

void app_public_cfg_init(void);

void app_set_scene_cfg(const uint8_t *cfg, uint16_t len);
void app_set_timer_task_cfg(const uint8_t *cfg, uint16_t len);

void app_set_bind_scene_cfg(const uint8_t *cfg, uint16_t len);
void app_set_bind_group_cfg(const uint8_t *cfg, uint16_t len);

void app_del_bind_cfg(void);
void app_del_scene_cfg(void);

const bind_scene_t *app_public_get_bind_scene(void);
const uint8_t app_ppublic_get_active_scene_bind(void);

const bind_group_t *app_public_get_bind_group(void);
const uint8_t app_public_get_active_group_bind(void);

const scene_id_t *app_public_get_scene(void);
const uint8_t app_public_get_active_scene(void);

const timer_task_t *app_public_get_timer_task(void);

#endif