#include "bsp_flash.h"

#define FLASH_PAGE_SIZE ((uint16_t)0x800) // 单页大小 2KB

__IO fmc_state_enum app_flash_write_word(uint32_t flash_start_addr, uint32_t *buffer, uint32_t byte_length)
{
    fmc_state_enum flash_status = FMC_READY;
    uint32_t total_pages;
    uint32_t page_index;
    uint32_t current_addr;

    __IO uint32_t verify_passed = 1; // 1 = 成功, 0 = 失败

    // 参数检查：指针不能为空，地址和长度必须 4 字节对齐
    if (!buffer || (flash_start_addr % 4) || (byte_length % 4)) {
        return FMC_PGERR;
    }

    fmc_unlock(); // 解锁 Flash

    // 清除可能存在的状态标志
    fmc_flag_clear(FMC_FLAG_BANK0_END);
    fmc_flag_clear(FMC_FLAG_BANK0_WPERR);
    fmc_flag_clear(FMC_FLAG_BANK0_PGERR);

    // 计算需要擦除的页数
    total_pages = (byte_length + (FLASH_PAGE_SIZE - 1)) / FLASH_PAGE_SIZE;

    // 擦除 Flash 页
    for (page_index = 0; page_index < total_pages && flash_status == FMC_READY; page_index++) {
        flash_status = fmc_page_erase(flash_start_addr + (FLASH_PAGE_SIZE * page_index));
    }

    // 按 32 位写入数据
    current_addr = flash_start_addr;
    for (uint32_t i = 0; i < byte_length / 4 && flash_status == FMC_READY; i++) {
        flash_status = fmc_word_program(current_addr, buffer[i]);
        current_addr += 4;
    }

    // 校验写入
    current_addr = flash_start_addr;
    for (uint32_t i = 0; i < byte_length / 4 && verify_passed; i++) {
        if ((*(__IO uint32_t *)current_addr) != buffer[i]) {
            verify_passed = 0;
            flash_status  = FMC_PGERR;
        }
        current_addr += 4;
    }

    fmc_lock(); // 锁定 Flash

    return flash_status;
}

__IO fmc_state_enum app_flash_read_word(uint32_t flash_start_addr, uint32_t *buffer, uint32_t byte_length)
{
    // 参数检查
    if (!buffer || (flash_start_addr % 4) || (byte_length % 4)) {
        return FMC_PGERR; // 参数错误
    }

    uint32_t current_addr = flash_start_addr;

    // 按 32 位逐字读取
    for (uint32_t i = 0; i < byte_length / 4; i++) {
        buffer[i] = *(__IO uint32_t *)current_addr; // 从 Flash 读取数据到缓冲区
        current_addr += 4;                          // 移动到下一个字
    }

    return FMC_READY; // 读取成功
}