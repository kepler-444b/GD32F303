#ifndef _APP_HTTP_OTA_H_
#define _APP_HTTP_OTA_H_

#include <stdint.h>

typedef struct
{
    char target[24]; // 目标版本
    uint32_t tid;    // 升级任务ID
    uint32_t size;   // 文件总大小
    uint8_t status;  // 升级状态(1:待升级2:下载中:升级中)
    uint8_t type;    // 1:完整包,2:差分包
    char md5[64];

    uint32_t total_packets;  // 总包数
    uint32_t current_packet; // 当前下载包索引 (从0开始)

} ota_info_t;

#define OTA_BLOCK_SIZE 2048 // 每次下载 2KB

int app_ota_check(uint8_t sn, uint8_t *server_name, uint8_t *server_ip, uint8_t server_port, uint8_t *shared_buf);
#endif