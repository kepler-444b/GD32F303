#ifndef _BSP_FLASH_H_
#define _BSP_FLASH_H_
#include "gd32f30x_fmc.h"

#define FLASH_SADDR          0x08000000 // flash 起始地址
#define FLASH_PAGE_SIZE      0x800      // flash 扇区大小

#define FLASH_BOOT_SADDR     0x08000000 // BOOT 16 KB
#define FLASH_APP_SADDR      0x08004000 // APP 200 KB

#define FLASH_OTA_SADDR      0x08036000 // OTA 200 KB
#define FLASH_CFG_SADDR      0x08068000 // CFG 96 KB

#define FLASH_OTA_INFO       FLASH_CFG_SADDR                          // 2 KB OTA 标志位 配置信息
#define FLASH_DEV_CFG        (FLASH_OTA_INFO + FLASH_PAGE_SIZE)       // 2 KB
#define FLASH_SCENE_BIND_CFG (FLASH_DEV_CFG + FLASH_PAGE_SIZE)        // 2 KB
#define FLASH_GROUP_BIND_CFG (FLASH_SCENE_BIND_CFG + FLASH_PAGE_SIZE) // 2 KB
#define FLASH_SCENE_CFG      (FLASH_GROUP_BIND_CFG + FLASH_PAGE_SIZE) // 20 KB
#define FLASH_TIMER_TASK_CFG (FLASH_SCENE_CFG + FLASH_PAGE_SIZE * 10) // 2 KB

__IO fmc_state_enum app_flash_write_word(uint32_t flash_start_addr, uint32_t *buffer, uint32_t byte_length);
__IO fmc_state_enum app_flash_write_page(uint32_t page_addr, uint32_t *buffer, uint32_t byte_length);
__IO fmc_state_enum app_flash_read_word(uint32_t flash_start_addr, uint32_t *buffer, uint32_t byte_length);
__IO fmc_state_enum app_flash_erase_page(uint32_t page_addr);

#endif