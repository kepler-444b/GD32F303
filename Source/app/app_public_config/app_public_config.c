#include "app_public_config.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"
#include "../Source/bsp/bsp_flash/bsp_flash.h"
#include <string.h>

static uint8_t DEF_PANEL_CONFIG[PANEL_INFO_SIZE]        = {0xF2, 0x0E, 0x0E, 0x0E, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x21, 0x21, 0x21, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x44, 0x00, 0x21, 0x00, 0x0E, 0x00, 0x00, 0x21, 0x00, 0xB0, 0x21, 0x84, 0x00, 0x00, 0x00, 0x00};
static uint8_t BIND_INFO_DONE_DATA[BIND_INFO_DONE_SIZE] = {0x42, 0x69, 0x6e, 0x64, 0x49, 0x6e, 0x66, 0x6f, 0x53, 0x65, 0x6e, 0x64, 0x44, 0x6f, 0x6e, 0x65};

// 函数声明
static void app_load_panel_cfg(void);
static void app_load_bind_info(void);

static void app_save_bind_cfg(void);
static void app_save_panel_cfg(void);

static void app_load_panel_cfg_by_num(uint8_t *cfg, uint8_t panel_num);
static void app_panel_info_update(const uint8_t *cfg, uint16_t len);

static panel_info_t my_panel_info[PANEL_DEV_MAX]; // 面板的配置信息
static bind_info_t my_bind_info[PANEL_DEV_MAX];   // 面板的绑定信息

static uint8_t panel_count;

void app_public_cfg_init(void)
{
    memset(my_panel_info, 0xFF, sizeof(my_panel_info));
    memset(my_bind_info, 0xFF, sizeof(my_bind_info));
    panel_count = 0;

    app_load_panel_cfg();
    app_load_bind_info();
}

// 更新到静态数组
static void app_panel_info_update(const uint8_t *cfg, uint16_t len)
{
    uint8_t panel_num = cfg[PANEL_INFO_SIZE - 1];

    panel_info_t *P_cfg = &my_panel_info[panel_num];
    for (uint8_t key_num = 0; key_num < KEY_NUMBER; key_num++) {
        if (key_num < 4) {
            P_cfg->key_cfg[key_num].func        = cfg[key_num + 1];
            P_cfg->key_cfg[key_num].group       = cfg[key_num + 5];
            P_cfg->key_cfg[key_num].area        = cfg[key_num + 9];
            P_cfg->key_cfg[key_num].perm        = cfg[key_num + 13];
            P_cfg->key_cfg[key_num].scene_group = cfg[key_num + 17];
        } else if (key_num == 4) {
            P_cfg->key_cfg[key_num].func        = cfg[21];
            P_cfg->key_cfg[key_num].group       = cfg[22];
            P_cfg->key_cfg[key_num].area        = cfg[23];
            P_cfg->key_cfg[key_num].perm        = cfg[26];
            P_cfg->key_cfg[key_num].scene_group = cfg[27];
        } else if (key_num == 5) {
            P_cfg->key_cfg[key_num].func        = cfg[28];
            P_cfg->key_cfg[key_num].group       = cfg[29];
            P_cfg->key_cfg[key_num].area        = cfg[30];
            P_cfg->key_cfg[key_num].perm        = cfg[31];
            P_cfg->key_cfg[key_num].scene_group = cfg[32];
        }
    }
    P_cfg->addr = panel_num;
}

static void app_bind_info_update(const uint8_t *cfg, uint16_t len)
{
    uint8_t panel_num = cfg[0];
    uint8_t key_num   = cfg[1];

    if (panel_num >= 32 || key_num >= 6) { // 边界检查
        APP_ERROR("panel_num or key_num error");
        return;
    }
    memcpy(my_bind_info[panel_num].key_bind[key_num].led_bind, &cfg[11], LED_NUM_MAX);
    memcpy(my_bind_info[panel_num].key_bind[key_num].relay_bind, &cfg[2], RELAY_NUM_MAX);
}

static void app_load_bind_info(void)
{
    fmc_state_enum status;
    status = app_flash_read_word(FLASH_121PAGE_ADDR, (uint32_t *)my_bind_info, sizeof(my_bind_info));

    if (status == FMC_READY) {
        APP_PRINTF("load_bind_cfg success!\n");
    } else {
        APP_ERROR("load_bind_cfg error\n");
    }
}

static void app_save_bind_cfg(void)
{
    fmc_state_enum status;
    status = app_flash_write_word(FLASH_121PAGE_ADDR, (uint32_t *)my_bind_info, sizeof(my_bind_info));

    if (status == FMC_READY) {
        APP_PRINTF("save_bind_cfg success!\n");
    } else {
        APP_ERROR("save_bind_cfg error");
    }
}

static void app_load_panel_cfg(void)
{
    fmc_state_enum status;
    status = app_flash_read_word(FLASH_127PAGE_ADDR, (uint32_t *)my_panel_info, sizeof(my_panel_info));
    APP_PRINTF("my_panel_info  length:%d\n", sizeof(my_panel_info));

    if (status == FMC_READY) {
        APP_PRINTF("load_panel_cfg success!\n");
    } else {
        APP_ERROR("load_panel_cfg error\n");
    }

    for (uint8_t panel_num = 0; panel_num < PANEL_DEV_MAX; panel_num++) {
        if (my_panel_info[panel_num].addr == 0xFF) {
            continue;
        }

        APP_PRINTF("addr[%02X]\n", my_panel_info[panel_num].addr);
        for (uint8_t key_num = 0; key_num < KEY_NUMBER; key_num++) {
            key_panel_t *const k_cfg = &my_panel_info[panel_num].key_cfg[key_num];
            APP_PRINTF("%02X %02X %02X %02X %02X |", k_cfg->func, k_cfg->group, k_cfg->area, k_cfg->perm, k_cfg->scene_group);
        }
        panel_count++;
    }
    APP_PRINTF("panel_count:%d\n", panel_count);
}

static void app_save_panel_cfg(void)
{
    fmc_state_enum status;
    status = app_flash_write_word(FLASH_127PAGE_ADDR, (uint32_t *)my_panel_info, sizeof(my_panel_info));

    if (status == FMC_READY) {
        APP_PRINTF("save_panel_cfg success!\n");
    } else {
        APP_ERROR("save_panel_cfg error");
    }
}

static void app_load_panel_cfg_by_num(uint8_t *cfg, uint8_t panel_num)
{
    fmc_state_enum status;
    status = app_flash_read_word(FLASH_127PAGE_ADDR + (panel_num * PANEL_INFO_SIZE), (uint32_t *)cfg, sizeof(cfg));

    if (status == FMC_READY) {
        APP_PRINTF("my_panel_info success!\n");
    } else {
        APP_ERROR("my_panel_info error\n");
    }
}

// 设置绑定信息
void app_set_bind_cfg(const uint8_t *cfg, uint16_t len)
{
    if ((len == BIND_INFO_DONE_SIZE) && (memcmp(cfg, BIND_INFO_DONE_DATA, len) == 0)) {
        app_save_bind_cfg(); // 绑定完成,存储到flash
        return;
    }

    if (len != BIND_INFO_SIZE) {
        APP_ERROR("BIND_INFO_SIZE error");
    }

    app_bind_info_update(cfg, len); // 更新到静态数组
}

// 设置配置信息
void app_set_panel_cfg(const uint8_t *cfg, uint16_t len)
{

    uint8_t addr = cfg[PANEL_INFO_SIZE - 1];
    if (addr >= 32) {
        APP_ERROR("addr error");
        return;
    }
    app_panel_info_update(cfg, len); // 更行到静态状态表

    app_save_panel_cfg();
}

const panel_info_t *app_public_get_cfg(uint8_t panel_num)
{
    return &my_panel_info[panel_num];
}

const bind_info_t *app_public_get_bind(uint8_t panel_num)
{
    return &my_bind_info[panel_num];
}
const uint8_t app_public_get_panel_count(void)
{
    return panel_count;
}