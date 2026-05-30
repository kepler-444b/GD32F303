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

static void app_load_scene_bind_cfg(void);
static void app_save_scene_bind_cfg(void);

static void app_load_group_bind_cfg(void);
static void app_save_group_bind_cfg(void);

static void app_scene_info_update(const uint8_t *cfg, uint16_t len);
static void app_bind_scene_info_update(const uint8_t *cfg, uint16_t len);
static void app_bind_group_info_update(const uint8_t *cfg, uint16_t len);

static scene_id_t my_scene_id[SCENE_ID_MAX]; // 场景ID列表
static uint8_t active_scene;                 // 激活的场景

static bind_group_t my_bind_group[GROUP_ID_MAX]; // 群组ID列表
static uint8_t active_group_bind;                // 激活的群组绑定信息

static bind_scene_t my_bind_scene[BIND_SCENE_MAX];
static uint8_t active_scene_bind; // 激活的绑定信息

void app_public_cfg_init(void)
{
    memset(my_scene_id, 0xFF, sizeof(my_scene_id));
    memset(my_bind_scene, 0xFF, sizeof(my_bind_scene));
    memset(my_bind_group, 0xFF, sizeof(my_bind_group));

    app_load_scene_cfg();
    delay_1ms(50);
    app_load_scene_bind_cfg();
    delay_1ms(50);
    app_load_group_bind_cfg();

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

// 更新到场景列表
static void app_scene_info_update(const uint8_t *cfg, uint16_t len)
{
    if (!cfg || len == 0) return;

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
    // scene
    my_scene_id[insert].id = cfg[offset];
    APP_PRINTF("scene_id: %02X\r\n", my_scene_id[insert].id);

    if (insert == active_scene) {
        active_scene++;
    }

    offset += 1 + 1; // data + FF

    // relay
    memcpy(my_scene_id[insert].relay, &cfg[offset], RELAY_NUM_MAX);
    APP_PRINTF_BUF("relay", my_scene_id[insert].relay, RELAY_NUM_MAX);
    offset += RELAY_NUM_MAX + 1;

    // led
    memcpy(my_scene_id[insert].led, &cfg[offset], LED_NUM_MAX);
    APP_PRINTF_BUF("led", my_scene_id[insert].led, LED_NUM_MAX);
    offset += (LED_NUM_MAX * 2) + 1; // 实际发下来的是64路led的状态,故而这里要乘2

    // key_ctrl
    memcpy(my_scene_id[insert].key_ctrl, &cfg[offset], PANEL_DEV_MAX);
    APP_PRINTF_BUF("key_ctrl", my_scene_id[insert].key_ctrl, PANEL_DEV_MAX);
    offset += PANEL_DEV_MAX + 1;

    // key_status
    memcpy(my_scene_id[insert].key_status, &cfg[offset], PANEL_DEV_MAX);
    APP_PRINTF_BUF("key_status", my_scene_id[insert].key_status, PANEL_DEV_MAX);
    offset += PANEL_DEV_MAX + 1;

    // key_reserve
    memcpy(my_scene_id[insert].key_reserve, &cfg[offset], PANEL_DEV_MAX);
    APP_PRINTF_BUF("key_reserve", my_scene_id[insert].key_reserve, PANEL_DEV_MAX);
}

// 更新到绑定列表
static void app_bind_scene_info_update(const uint8_t *cfg, uint16_t len)
{
    if (!cfg || len != 6) { // 绑定信息固定6个字节
        APP_ERROR("bind scene info len");
        return;
    }

    uint8_t addr     = cfg[0];
    uint8_t key_num  = cfg[1];
    uint8_t status   = cfg[2];
    uint8_t scene_id = cfg[3];

    for (uint8_t i = 0; i < active_scene_bind; i++) {

        bool same_addr   = (my_bind_scene[i].addr == addr);
        bool same_key    = (my_bind_scene[i].key_num == key_num);
        bool same_status = (my_bind_scene[i].status == status);
        bool same_scene  = (my_bind_scene[i].scene_id == scene_id);

        // 不是同一个按键事件
        if (!(same_addr && same_key && same_status)) {
            continue;
        }

        if (same_scene) { // 完全相同的绑定
            APP_PRINTF("same scene bind\n");
            return;
        } else { //  场景号不同,覆盖
            my_bind_scene[i].scene_id = scene_id;
            return;
        }
    }

    if (active_scene_bind >= BIND_SCENE_MAX) {
        APP_ERROR("bind scene full");
        return;
    }
    
    my_bind_scene[active_scene_bind].addr     = addr;
    my_bind_scene[active_scene_bind].key_num  = key_num;
    my_bind_scene[active_scene_bind].status   = status;
    my_bind_scene[active_scene_bind].scene_id = scene_id;

    active_scene_bind++;
    APP_PRINTF("addr:%02X key_num:%02X status:%02X scene_id:%02X\n",
               my_bind_scene[active_scene_bind].addr, my_bind_scene[active_scene_bind].key_num, my_bind_scene[active_scene_bind].status, my_bind_scene[active_scene_bind].scene_id);
}

// 更新到群组列表
static void app_bind_group_info_update(const uint8_t *cfg, uint16_t len)
{
    if (!cfg || len != 12) { // 绑定信息固定12个字节
        APP_PRINTF("len:%d\n", len);
        return;
    }

    uint8_t addr     = cfg[0];
    uint8_t ctrls[8] = {0};
    uint8_t close_id = cfg[10];
    uint8_t open_id  = cfg[11];

    memcpy(ctrls, &cfg[1], sizeof(ctrls));
    for (uint8_t i = 0; i < active_group_bind; i++) {
        if (my_bind_group->addr == addr &&
            (memcmp(ctrls, my_bind_group->ctrls, 0) == 0) &&
            open_id == my_bind_group->open_id &&
            close_id == my_bind_group->close_id) {
            APP_PRINTF("same group bind\n");
            return;
        }
    }
    my_bind_group[active_group_bind].addr = addr;
    memcpy(my_bind_group[active_group_bind].ctrls, ctrls, sizeof(ctrls));
    my_bind_group[active_group_bind].close_id = close_id;
    my_bind_group[active_group_bind].open_id  = open_id;
    APP_PRINTF("addr:%02X open_id:%02X close_id:%02X\n", my_bind_group[active_group_bind].addr, my_bind_group[active_group_bind].open_id, my_bind_group[active_group_bind].close_id);
    APP_PRINTF_BUF("ctrls", my_bind_group[active_group_bind].ctrls, sizeof(my_bind_group[active_group_bind].ctrls));
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
                APP_PRINTF("id:%02X\n", my_scene_id[i].id);
                APP_PRINTF_BUF("led", my_scene_id[i].led, sizeof(my_scene_id[i].led));
                APP_PRINTF_BUF("relay", my_scene_id[i].relay, sizeof(my_scene_id[i].relay));
                APP_PRINTF_BUF("key_ctrl", my_scene_id[i].key_ctrl, sizeof(my_scene_id[i].key_ctrl));
                APP_PRINTF_BUF("key_status", my_scene_id[i].key_status, sizeof(my_scene_id[i].key_status));
                APP_PRINTF_BUF("key_reserve", my_scene_id[i].key_reserve, sizeof(my_scene_id[i].key_reserve));
                active_scene++;
            }
        }
        APP_PRINTF("active_scene = %d\n", active_scene);
    } else {
        APP_ERROR("my_scene_id error\n");
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
        for (uint8_t i = 0; i < GROUP_ID_MAX; i++) {
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
    app_scene_info_update(cfg, len);
    app_save_scene_cfg();
    bsp_set_buuzzer(1);
}
// 删除场景信息
void app_del_scene_cfg(void)
{
    APP_PRINTF("app_del_scene_cfg\n");
    app_flash_erase_page(FLASH_SCENE_CFG);
    active_scene_bind = 0;
    bsp_set_buuzzer(3);
}

// 设置绑定场景信息
void app_set_bind_scene_cfg(const uint8_t *cfg, uint16_t len)
{
    app_bind_scene_info_update(cfg, len);
    app_save_scene_bind_cfg();
    bsp_set_buuzzer(1);
}

// 设置绑定的群组
void app_set_bind_group_cfg(const uint8_t *cfg, uint16_t len)
{
    app_bind_group_info_update(cfg, len);
    app_save_group_bind_cfg();
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
