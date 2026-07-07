#include "app_public_config.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"
#include "../Source/bsp/bsp_flash/bsp_flash.h"
#include "../Source/bsp/bsp_board/bsp_board.h"
#include "systick.h"
#include <string.h>
#include <stdbool.h>

// 函数声明

static void app_load_scene_cfg(void);
static void app_save_scene_cfg(void);

static void app_load_timer_task_cfg(void);
static void app_save_timer_task_cfg(void);

static void app_load_scene_bind_cfg(void);
static void app_save_scene_bind_cfg(void);

static void app_load_group_bind_cfg(void);
static void app_save_group_bind_cfg(void);

static bool app_scene_info_update(const uint8_t *cfg, uint16_t len);
static bool app_timer_task_info_update(const uint8_t *cfg, uint16_t len);

static bool app_bind_scene_info_update(const uint8_t *cfg, uint16_t len);
static bool app_bind_group_info_update(const uint8_t *cfg, uint16_t len);

static timer_task_t my_timer_task[TIMER_TASK_MAX];

static scene_id_t my_scene_id[SCENE_ID_MAX]; // 场景ID列表
static uint8_t active_scene;                 // 激活的场景

static bind_group_t my_bind_group[BIND_GROUP_MAX]; // 群组ID列表
static uint8_t active_group_bind;                  // 激活的群组绑定信息

static bind_scene_t my_bind_scene[BIND_SCENE_MAX];
static uint8_t active_scene_bind; // 激活的绑定信息

void app_public_cfg_init(void)
{
    memset(my_scene_id, 0xFF, sizeof(my_scene_id));
    memset(my_bind_scene, 0xFF, sizeof(my_bind_scene));
    memset(my_bind_group, 0xFF, sizeof(my_bind_group));
    memset(my_timer_task, 0xFF, sizeof(my_timer_task));

    app_load_scene_cfg();
    delay_1ms(50);
    app_load_scene_bind_cfg();
    delay_1ms(50);
    app_load_group_bind_cfg();
    delay_1ms(50);
    app_load_timer_task_cfg();

    // if (app_flash_erase_page(FLASH_SCENE_BIND_CFG) != FMC_READY) {
    //     APP_ERROR("app_flash_erase_page");
    // }
    // if (app_flash_erase_page(FLASH_SCENE_CFG) != FMC_READY) {
    //     APP_ERROR("app_flash_erase_page");
    // }
    // if (app_flash_erase_page(FLASH_GROUP_BIND_CFG) != FMC_READY) {
    //     APP_ERROR("app_flash_erase_page");
    // }
}

static bool app_timer_task_info_update(const uint8_t *cfg, uint16_t len)
{
    if (!cfg || len != TIMER_TASK_INFO_SIZE) {
        APP_ERROR("timer_task info len");
        return false;
    }
    for (uint16_t i = 0; i < TIMER_TASK_MAX; i++) {

        const uint8_t *p = cfg + i * 6;

        uint8_t id       = p[0];
        uint8_t scene_id = p[1];
        uint8_t enable   = p[2];
        uint8_t hour     = p[3];
        uint8_t min      = p[4];
        uint8_t reserve  = p[5];

        if (id >= TIMER_TASK_MAX) {
            APP_ERROR("timer id err:%d", id);
            continue;
        }

        if (hour >= 24 || min >= 60) {
            APP_ERROR("time err: id=%d h=%d m=%d", id, hour, min);
            continue;
        }
        my_timer_task[id].scene_id = scene_id;
        my_timer_task[id].enable   = (enable != 0);
        my_timer_task[id].hour     = hour;
        my_timer_task[id].min      = min;
        my_timer_task[id].reserve  = reserve;
        APP_PRINTF("id:%d scene_id:%d enable:%d hour:%d min:%d reserve:%d\n", id, scene_id, enable, hour, min, reserve);
    }
    return true;
}

// 更新到场景列表
static bool app_scene_info_update(const uint8_t *cfg, uint16_t len)
{
    if (!cfg || len != SCENE_INFO_SIZE) {
        APP_ERROR("scene info len");
        return false;
    }

    uint16_t offset  = 0;
    uint8_t insert   = active_scene;
    uint8_t scene_id = cfg[0];

    // 查找是否存在
    for (uint8_t i = 0; i < SCENE_ID_MAX; i++) {
        if (my_scene_id[i].id == scene_id) {
            insert = i;
            break;
        }
    }

    if (insert == active_scene) {
        if (active_scene >= SCENE_ID_MAX) {
            APP_ERROR("scene list full");
            return false;
        }
        active_scene++;
    }

    // scene
    my_scene_id[insert].id = cfg[offset];
    APP_PRINTF("scene_id: %d\r\n", my_scene_id[insert].id);
    offset += 1 + 1; // data + FF

    // relay
    memcpy(my_scene_id[insert].relay, &cfg[offset], RELAY_NUM_MAX);
    // APP_PRINTF_BUF("relay", my_scene_id[insert].relay, RELAY_NUM_MAX);
    offset += RELAY_NUM_MAX + 1;

    // led
    memcpy(my_scene_id[insert].led, &cfg[offset], LED_NUM_MAX);
    // APP_PRINTF_BUF("led", my_scene_id[insert].led, LED_NUM_MAX);
    offset += (LED_NUM_MAX * 2) + 1; // 实际发下来的是64路led的状态,故而这里要乘2

    // key_ctrl
    memcpy(my_scene_id[insert].key_ctrl, &cfg[offset], PANEL_DEV_MAX);
    // APP_PRINTF_BUF("key_ctrl", my_scene_id[insert].key_ctrl, PANEL_DEV_MAX);
    offset += PANEL_DEV_MAX + 1;

    // key_status
    memcpy(my_scene_id[insert].key_status, &cfg[offset], PANEL_DEV_MAX);
    // APP_PRINTF_BUF("key_status", my_scene_id[insert].key_status, PANEL_DEV_MAX);
    offset += PANEL_DEV_MAX + 1;

    // key_reserve
    memcpy(my_scene_id[insert].key_reserve, &cfg[offset], PANEL_DEV_MAX);
    // APP_PRINTF_BUF("key_reserve", my_scene_id[insert].key_reserve, PANEL_DEV_MAX);
    return true;
}

// 更新到绑定列表
static bool app_bind_scene_info_update(const uint8_t *cfg, uint16_t len)
{
    if (!cfg || len != BIND_SCENE_INFO_SIZE) { // 绑定信息固定12个字节
        APP_PRINTF("bind scene info len err:%d\n", len);
        return false;
    }

    uint8_t addr     = cfg[0];
    uint8_t key_num  = cfg[1];
    uint8_t status   = cfg[2];
    uint8_t scene_id = cfg[3];
    uint8_t insert   = active_scene_bind;

    // 查找是否存在相同的按键事件
    for (uint8_t i = 0; i < active_scene_bind; i++) {

        if ((my_bind_scene[i].addr == addr) &&
            (my_bind_scene[i].key_num == key_num) &&
            (my_bind_scene[i].status == status)) {
            insert = i; // 找到了相同的按键和状态,记录下标并准备更新/覆盖
            break;
        }
    }
    // 如果是全新绑定,检查容量是否已满
    if (insert == active_scene_bind) {
        if (active_scene_bind >= SCENE_ID_MAX) {
            APP_ERROR("bind scene full");
            return false;
        } else {
            active_scene_bind++; // 安全过关,计数+1
        }
    }

    // 不管是新纪录还是老覆盖，直接无条件写入
    my_bind_scene[insert].addr     = addr;
    my_bind_scene[insert].key_num  = key_num;
    my_bind_scene[insert].scene_id = scene_id;
    my_bind_scene[insert].status   = status;
    return true;
}

// 更新到群组列表
static bool app_bind_group_info_update(const uint8_t *cfg, uint16_t len)
{
    if (!cfg || len != BIND_GROUP_INFO_SIZE) { // 绑定信息固定12个字节
        APP_PRINTF("len:%d\n", len);
        return false;
    }

    uint8_t addr     = cfg[0];
    uint8_t ctrls[8] = {0};
    uint8_t close_id = cfg[10];
    uint8_t open_id  = cfg[11];
    memcpy(ctrls, &cfg[1], sizeof(ctrls));

    uint8_t insert = active_group_bind;

    for (uint8_t i = 0; i < active_group_bind; i++) {
        if ((my_bind_group[i].addr == addr) &&
            (my_bind_group[i].close_id == close_id) &&
            (my_bind_group[i].open_id == open_id) &&
            (memcmp(my_bind_group[i].ctrls, ctrls, sizeof(ctrls)) == 0)) {
            insert = i;
            break;
        }
    }

    if (insert == active_group_bind) { // 是新绑定
        if (active_group_bind >= BIND_GROUP_MAX) {
            APP_ERROR("bind group full");
            return false;
        } else {
            active_group_bind++;
        }
    }

    my_bind_group[insert].addr     = addr;
    my_bind_group[insert].close_id = close_id;
    my_bind_group[insert].open_id  = open_id;
    memcpy(my_bind_group[insert].ctrls, ctrls, sizeof(ctrls));

    APP_PRINTF("addr:%02X open_id:%02X close_id:%02X\n",
               my_bind_group[insert].addr, my_bind_group[insert].open_id, my_bind_group[insert].close_id);
    APP_PRINTF_BUF("ctrls", my_bind_group[insert].ctrls, sizeof(my_bind_group[insert].ctrls));
    return true;
}

static void app_save_scene_cfg(void)
{
    fmc_state_enum status;
    status = app_flash_write_word(FLASH_SCENE_CFG, (uint32_t *)my_scene_id, sizeof(my_scene_id));

    if (status == FMC_READY) {
        APP_PRINTF("save_scene_id success!\n");
    } else {
        APP_ERROR("save_scene_id error");
    }
}

static void app_save_timer_task_cfg(void)
{
    fmc_state_enum status;
    status = app_flash_write_word(FLASH_TIMER_TASK_CFG, (uint32_t *)my_timer_task, sizeof(my_timer_task));

    if (status == FMC_READY) {
        APP_PRINTF("save my_timer_task success!\n");
    } else {
        APP_ERROR("save my_timer_task error");
    }
}

static void app_save_scene_bind_cfg(void)
{
    fmc_state_enum status;
    status = app_flash_write_word(FLASH_SCENE_BIND_CFG, (uint32_t *)my_bind_scene, sizeof(my_bind_scene));

    if (status == FMC_READY) {
        APP_PRINTF("save_bind_scene success!\n");
    } else {
        APP_ERROR("save_bind_scene error");
    }
}

static void app_save_group_bind_cfg(void)
{
    fmc_state_enum status;
    status = app_flash_write_word(FLASH_GROUP_BIND_CFG, (uint32_t *)my_bind_group, sizeof(my_bind_group));

    if (status == FMC_READY) {
        APP_PRINTF("save_bind_group success!\n");
    } else {
        APP_ERROR("save_bind_group error");
    }
}

// 加载场景信息
static void app_load_scene_cfg(void)
{
    APP_PRINTF("[load scene_id] ==================================\n");
    fmc_state_enum status;
    active_scene = 0;

    status = app_flash_read_word(FLASH_SCENE_CFG, (uint32_t *)my_scene_id, sizeof(my_scene_id));

    if (status == FMC_READY) {
        APP_PRINTF("my_scene_id success!\n");
        for (uint8_t i = 0; i < SCENE_ID_MAX; i++) {
            if (my_scene_id[i].id != 0xFF) {
                // APP_PRINTF("id:%02X\n", my_scene_id[i].id);
                // APP_PRINTF_BUF("led", my_scene_id[i].led, sizeof(my_scene_id[i].led));
                // APP_PRINTF_BUF("relay", my_scene_id[i].relay, sizeof(my_scene_id[i].relay));
                // APP_PRINTF_BUF("key_ctrl", my_scene_id[i].key_ctrl, sizeof(my_scene_id[i].key_ctrl));
                // APP_PRINTF_BUF("key_status", my_scene_id[i].key_status, sizeof(my_scene_id[i].key_status));
                // APP_PRINTF_BUF("key_reserve", my_scene_id[i].key_reserve, sizeof(my_scene_id[i].key_reserve));
                active_scene++;
            }
        }
        APP_PRINTF("active_scene = %d\n", active_scene);
    } else {
        APP_ERROR("my_scene_id error\n");
    }
}

// 加载定时任务信息
static void app_load_timer_task_cfg(void)
{
    APP_PRINTF("[load timer_task] ================================\n");
    fmc_state_enum status;

    status = app_flash_read_word(FLASH_TIMER_TASK_CFG, (uint32_t *)my_timer_task, sizeof(my_timer_task));
    if (status == FMC_READY) {
        APP_PRINTF("my_timer_task success!\n");
        for (uint8_t i = 0; i < TIMER_TASK_MAX; i++) {

            APP_PRINTF("id=%d enable=%d hour=%d min=%d\n", my_timer_task[i].scene_id, my_timer_task[i].enable, my_timer_task[i].hour, my_timer_task[i].min);
        }
    } else {
        APP_ERROR("my_timer_task error\n");
    }
}

// 加载绑定信息
static void app_load_scene_bind_cfg(void)
{
    APP_PRINTF("[load bind_scene] ================================\n");
    fmc_state_enum status;
    active_scene_bind = 0;

    status = app_flash_read_word(FLASH_SCENE_BIND_CFG, (uint32_t *)my_bind_scene, sizeof(my_bind_scene));

    if (status == FMC_READY) {
        APP_PRINTF("my_bind_scene success!\n");
        // 遍历数组,统计有效绑定
        for (uint8_t i = 0; i < BIND_SCENE_MAX; i++) {
            if (my_bind_scene[i].scene_id != 0xFF) {
                APP_PRINTF("addr=%d key=%d scene=%d status=%d\n", my_bind_scene[i].addr, my_bind_scene[i].key_num, my_bind_scene[i].scene_id, my_bind_scene[i].status);

                active_scene_bind++; // 统计激活绑定数量
            }
        }
    } else {
        APP_ERROR("my_bind_scene error\n");
    }
}

// 加载绑定的群组信息
static void app_load_group_bind_cfg(void)
{
    APP_PRINTF("[load bind_group] ================================\n");
    fmc_state_enum status;
    active_group_bind = 0;

    status = app_flash_read_word(FLASH_GROUP_BIND_CFG, (uint32_t *)my_bind_group, sizeof(my_bind_group));

    if (status == FMC_READY) {
        APP_PRINTF("my_bind_group success!\n");
        // 遍历数组,统计有效绑定
        for (uint8_t i = 0; i < BIND_GROUP_MAX; i++) {
            if (my_bind_group[i].addr != 0xFF) {
                APP_PRINTF("addr:%02X open_id:%02X close_id:%02X\n", my_bind_group[active_group_bind].addr, my_bind_group[active_group_bind].open_id, my_bind_group[active_group_bind].close_id);
                APP_PRINTF_BUF("ctrls", my_bind_group[active_group_bind].ctrls, sizeof(my_bind_group[active_group_bind].ctrls));
                active_group_bind++; // 统计激活绑定数量
            }
        }
    } else {
        APP_ERROR("my_bind_group error\n");
    }
}

// 设置场景信息
void app_set_scene_cfg(const uint8_t *cfg, uint16_t len)
{
    if (app_scene_info_update(cfg, len)) {
        app_save_scene_cfg();
    }
}

// 设置定时任务
void app_set_timer_task_cfg(const uint8_t *cfg, uint16_t len)
{
    if (app_timer_task_info_update(cfg, len)) {
        bsp_set_buuzzer(1);
        app_save_timer_task_cfg();
    }
}

// 删除场景信息
void app_del_scene_cfg(void)
{
    APP_PRINTF("app_del_scene_cfg\n");
    for (uint8_t i = 0; i < 10; i++) {
        app_flash_erase_page(FLASH_SCENE_CFG + (FLASH_PAGE_SIZE * i));
    }
    active_scene_bind = 0;
    bsp_set_buuzzer(3);
}

// 设置绑定场景信息
void app_set_bind_scene_cfg(const uint8_t *cfg, uint16_t len)
{
    if (app_bind_scene_info_update(cfg, len)) {
        app_save_scene_bind_cfg();
    }
    bsp_set_buuzzer(1);
}

// 设置绑定的群组
void app_set_bind_group_cfg(const uint8_t *cfg, uint16_t len)
{
    if (app_bind_group_info_update(cfg, len)) {
        app_save_group_bind_cfg();
    }
}

// 删除绑定信息
void app_del_bind_cfg(void)
{
    APP_PRINTF("app_del_bind_cfg\n");
    app_flash_erase_page(FLASH_SCENE_BIND_CFG);
    active_scene_bind = 0;
    bsp_set_buuzzer(3);
}

// 获取激活的场景绑定信息
const uint8_t app_ppublic_get_active_scene_bind(void)
{
    return active_scene_bind;
}

// 获取激活的群组绑定信息
const uint8_t app_public_get_active_group_bind(void)
{
    return active_group_bind;
}

// 获取绑定的群组信息
const bind_group_t *app_public_get_bind_group(void)
{
    return my_bind_group;
}

// 获取绑定的场景信息
const bind_scene_t *app_public_get_bind_scene(void)
{
    return my_bind_scene;
}

// 获取激活的场景信息
const uint8_t app_public_get_active_scene(void)
{
    return active_scene;
}

// 获取场景列表
const scene_id_t *app_public_get_scene(void)
{
    return my_scene_id;
}

// 获取定时任务
const timer_task_t *app_public_get_timer_task(void)
{
    return my_timer_task;
}
