#ifndef _APP_NETWORK_MODEL_H_
#define _APP_NETWORK_MODEL_H_

#include "../Source/w5500/ioLibrary_Driver/Internet/MQTT/MQTTClient.h"
#include "../Source/w5500/ioLibrary_Driver/Ethernet/wizchip_conf.h"
#include "../Source/cjson/cJSON.h"
#include <stdio.h>

// MQTT 连接状态机
typedef enum {
    CONNECT_MQTT, // 连接服务器
    SUB_TOPIC,    // 订阅主题
    KEEP_ALIVE,   // 保活心跳
    ERROR_STATUS  // 错误处理
} mqtt_status_e;

typedef enum {
    HTTP_RECONNECT_RESET = 0,
    MQTT_DNS_INIT, // 初始化 DNS
    CLIENT_INIT,   // 创建 MQTT 客户端
    CLIENT_SUCC    // MQTT 客户端创建成功
} mqtt_task_e;

typedef enum {
    MQTT_PRO_SET,  // 设置属性
    MQTT_PRO_GET,  // 获取属性
    MQTT_PRO_POST, // 上报属性
    MQTT_SERVICE,  // 服务下发
} mqtt_type_e;

typedef enum {
    HTTP_DNS_INIT,  // 初始化 DNS
    HTTP_OTA_CHECK, // 检查OTA任务
} http_task_e;

typedef struct {

    char mqttHostUrl[64]; // MQTT 服务器域名
    uint8_t server_ip[4]; // MQTT 服务器 IP 地址
    int port;             // MQTT 服务器端口号 1883

    char clientid[64]; // MQTT 客户端 ID
    char username[64]; // MQTT 登录用户名
    char passwd[150];  // MQTT 登录密码

    char property_post[64];       // 直连设备上报属性
    char property_post_reply[64]; // 发布消息对应的回复主题

    char property_set[64];       // 设置直连设备属性
    char property_set_reply[64]; // 直连设备属性设置响应

    char property_get[64];       // 获取直连设备属性
    char property_get_reply[64]; // 直连设备回复平台获取设备属性

    char service_invoke[64];
    char service_invoke_reply[128];

    char ota_inform[64];       // 系统OTA通知
    char ota_inform_reply[64]; // 设备回复系统OTA通知

    enum QoS pubQoS; // 发布消息的 QoS 等级
    enum QoS subQoS; // 订阅消息的 QoS 等级

    char willtopic[64]; // 遗嘱消息主题
    enum QoS willQoS;   // 遗嘱消息 QoS 等级
    char willmsg[64];   // 遗嘱消息内容
} mqttconn;

typedef void (*mqtt_handler_t)(cJSON *item, const char *id, mqtt_type_e type);

typedef struct {
    const char *key;
    mqtt_handler_t handler;
} mqtt_dispatch_t;

void app_network_model_init(void);

#endif