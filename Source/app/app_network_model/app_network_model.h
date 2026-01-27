#ifndef _APP_NETWORK_MODEL_H_
#define _APP_NETWORK_MODEL_H_

#include "../Source/w5500/ioLibrary_Driver/Internet/MQTT/MQTTClient.h"
#include "../Source/w5500/ioLibrary_Driver/Ethernet/wizchip_conf.h"
#include "../Source/cjson/cJSON.h"
#include <stdio.h>

// MQTT 连接状态
typedef enum {
    CONNECT_MQTT,
    SUB_TOPIC,
    KEEP_ALIVE,
    ERROR_STATUS
} mqtt_status_e;

typedef struct
{
    wiz_NetInfo netinfo;
    uint8_t *domain_name;
    uint8_t *domain_ip;
    uint8_t *recv_buf;
    uint8_t *send_buf;
    uint8_t sn;
} mqtt_dns_info_t;

typedef struct {
    wiz_NetInfo netinfo;
    uint8_t *buf;
    uint8_t *domain_name;
    uint8_t *domain_ip;
    uint8_t sn;
} http_dns_info_t;

typedef struct {

    char mqttHostUrl[128]; // MQTT 服务器域名
    uint8_t server_ip[4];  // MQTT 服务器 IP 地址
    int port;              // MQTT 服务器端口号 1883

    char clientid[128]; // MQTT 客户端 ID
    char username[128]; // MQTT 登录用户名
    char passwd[128];   // MQTT 登录密码

    char property_post[255];       // 直连设备上报属性
    char property_post_reply[255]; // 发布消息对应的回复主题

    char property_set[255];       // 设置直连设备属性
    char property_set_reply[255]; // 直连设备属性设置响应

    char property_get[255];       // 获取直连设备属性
    char property_get_reply[255]; // 直连设备回复平台获取设备属性

    char ota_inform[128];       // 系统OTA通知
    char ota_inform_reply[128]; // 设备回复系统OTA通知

    enum QoS pubQoS; // 发布消息的 QoS 等级
    enum QoS subQoS; // 订阅消息的 QoS 等级

    char willtopic[255]; // 遗嘱消息主题
    enum QoS willQoS;    // 遗嘱消息 QoS 等级
    char willmsg[255];   // 遗嘱消息内容
} mqttconn;

// 绑定按键信息结构体
typedef struct {
    char bind_info[160];
} bind_cfg_msg_t;

typedef struct {
    char panel_info[82]; // 要多分配2个字节来存放"\0"
} panel_cfg_msg_t;

typedef void (*dispatch_handler_t)(cJSON *params, const char *id);

typedef struct {
    const char *key;
    dispatch_handler_t handler;
} dispatch_map_t;

void app_network_model_init(void);

#endif