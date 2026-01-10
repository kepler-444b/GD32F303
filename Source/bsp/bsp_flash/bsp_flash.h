#ifndef _BSP_FLASH_H_
#define _BSP_FLASH_H_
#include "gd32f30x_fmc.h"

#define FLASH_SADDR      0x08000000 // flash 起始地址
#define FLASH_PAGE_SIZE  0x800      // flash 扇区大小

#define FLASH_BOOT_SADDR 0x08000000 // boot 程序起始地址    (16kb)
#define FLASH_APP_SADDR  0x08004000 // app  程序起始地址    (100kb)
#define FLASH_OTA_SADDR  0x0801D000 // ota  临时起始地址    (100kb)
#define FLASH_CFG_SADDR  0x08036000 // cfg  配置信息地址    (40kb)

#define FLASH_CFG_1      FLASH_CFG_SADDR
#define FLASH_CFG_2      FLASH_CFG_1 + FLASH_PAGE_SIZE

// #define FLASH_119PAGE_ADDR 0x08037800UL // 用于存放按键绑定信息
#define FLASH_120PAGE_ADDR 0x08038000UL
#define FLASH_121PAGE_ADDR 0x08038000UL // 用于存放按键绑定信息
#define FLASH_127PAGE_ADDR 0x0803F800UL // 用于存放面板配置信息

__IO fmc_state_enum app_flash_write_word(uint32_t flash_start_addr, uint32_t *buffer, uint32_t byte_length);
__IO fmc_state_enum app_flash_write_page(uint32_t page_addr, uint32_t *buffer, uint32_t byte_length);
__IO fmc_state_enum app_flash_read_word(uint32_t flash_start_addr, uint32_t *buffer, uint32_t byte_length);

#endif