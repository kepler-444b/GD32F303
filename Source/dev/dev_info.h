#ifndef _DEV_INFO_H_
#define _DEV_INFO_H_

#include <stdint.h>
#include <stdbool.h>
#include "../Source/w5500/ioLibrary_Driver/Ethernet/wizchip_conf.h"

#define OTA_SET_FLAG 0xAABB1122 // 升级标志位
#define CUR_VER      "V0.1"     // 初始软件版本

// 固件信息
typedef struct
{
    char products[32]; // 产品ID
    char devices[32];  // 设备ID
    char key[128];     // key
    char cur_ver[16];  // 当前固件版本
    char md5[34];      // md5
    char version[32];
    char passwd[150];
    uint32_t et;
    uint32_t ota_flag;  // 升级标志位
    uint32_t file_size; // 文件大小

} dev_save_info_t;

typedef struct
{
    wiz_NetInfo net;
    char devices[32];
    char key[128];
    char cur_ver[16];

} dev_packet_t;

void dev_device_info_init(void);

const dev_save_info_t *dev_get_save_device_info(void);

const wiz_NetInfo *dev_get_nw_cfg_info(void);    // 获取网络信息
bool dev_set_nw_cfg_info(wiz_NetInfo *dev_info); // 设置网络信息

bool dev_set_save_device_info(dev_save_info_t *dev_info);
#endif