#ifndef _BSP_FLASH_H_
#define _BSP_FLASH_H_
#include "gd32f30x_fmc.h"

#define FLASH_SADDR        0x08000000 // flash 起始地址
#define FLASH_PAGE_SIZE    2048       // flash 扇区大小

#define FLASH_PAGE_NUM     128 // flash 总扇区个数

#define FLASH_B_PAGE_NUM   20                                // B 区扇区个数
#define FLASH_A_PAGE_NUM   FLASH_PAGE_NUM - FLASH_B_PAGE_NUM // A 区扇区个数

#define FLASH_A_SPAG       FLASH_PAGE_NUM                              // A 区起始扇区编号
#define FLASH_A_SADDR      FLASH_SADDR + FLASH_A_SPAG *FLASH_PAGE_SIZE // A区起始地址

#define FLASH_119PAGE_ADDR 0x08037000UL // 用于存放 ota_info

// #define FLASH_119PAGE_ADDR 0x08037800UL // 用于存放按键绑定信息
#define FLASH_120PAGE_ADDR 0x08038000UL
#define FLASH_121PAGE_ADDR 0x08038000UL // 用于存放按键绑定信息
#define FLASH_127PAGE_ADDR 0x0803F800UL // 用于存放面板配置信息

__IO fmc_state_enum app_flash_write_word(uint32_t flash_start_addr, uint32_t *buffer, uint32_t byte_length);
__IO fmc_state_enum app_flash_read_word(uint32_t flash_start_addr, uint32_t *buffer, uint32_t byte_length);

#endif