#ifndef _DEV_INFO_H_
#define _DEV_INFO_H_

#include <stdint.h>
#include <stdbool.h>

// 固件信息
typedef struct
{
    uint8_t dev_type[24]; // 设备名称
    uint8_t old_ver[16];  // 原始固件版本
    uint8_t new_ver[16];  // 新固件版本
    uint8_t md5[16];

    bool bin_ota_flag;

} dev_info_t;

void dev_device_init(void);

#endif