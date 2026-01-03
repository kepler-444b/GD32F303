#ifndef _BSP_FLASH_H_
#define _BSP_FLASH_H_

// #define FLASH_119PAGE_ADDR 0x08037800UL // 用于存放按键绑定信息
#define FLASH_120PAGE_ADDR 0x08038000UL
#define FLASH_121PAGE_ADDR 0x08038000UL // 用于存放按键绑定信息
#define FLASH_127PAGE_ADDR 0x0803F800UL // 用于存放面板配置信息

#include "gd32f30x_fmc.h"
__IO fmc_state_enum app_flash_write_word(uint32_t flash_start_addr, uint32_t *buffer, uint32_t byte_length);
__IO fmc_state_enum app_flash_read_word(uint32_t flash_start_addr, uint32_t *buffer, uint32_t byte_length);

#endif