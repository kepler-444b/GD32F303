#include "dev_info.h"
#include "../Source/bsp/bsp_flash/bsp_flash.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"

// 函数声明
static void dev_load_device_info(void);

// 全局变量
static dev_info_t my_dev_info;

void dev_device_init(void)
{
    dev_load_device_info();
}

static void dev_load_device_info(void)
{
    fmc_state_enum status;
    status = app_flash_read_word(FLASH_121PAGE_ADDR, (uint32_t *)&my_dev_info, sizeof(my_dev_info));

    if (status == FMC_READY) {
        APP_PRINTF("dev_load_device_info success!\n");
    } else {
        APP_ERROR("dev_load_device_info error\n");
    }
}

const dev_info_t *dev_get_device_info(void)
{
    return &my_dev_info;
}

bool dev_set_device_info(dev_info_t *info)
{
    return false;
}
