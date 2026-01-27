#ifndef _APP_HTTP_OTA_H_
#define _APP_HTTP_OTA_H_

#include <stdint.h>
#include <stdbool.h>
#include "app_network_info.h"

// OTA 任务信息
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

// OTA 状态机
typedef enum {

    OTA_IDLE = 0,          // 空闲状态
    OTA_REPORT_VER,        // 上报固件版本
    OTA_GET_TASK,          // 获取升级任务
    OTA_REPORT_OK,         // 上报下载成功
    OTA_REPORT_CHECK_FAIL, // 上报DM5校验失败

} ota_flag_e;

// HTTP 连接信息结构体
typedef struct {

    uint8_t sn;
    char httpHostUrl[128]; // HTTP服务器域名
    uint8_t server_ip[4];  // DNS解析后的IP
    uint16_t port;         // 端口号
} httpconn;

#define OTA_BLOCK_SIZE 2048 // 每次下载 2KB

bool app_ota_check(httpconn *http_params, device_info_t *dev, uint8_t *shared_buf);

#endif