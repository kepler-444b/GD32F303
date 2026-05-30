#ifndef _APP_NETWORK_INFO_H_
#define _APP_NETWORK_INFO_H_

#include <stdint.h>

#define MQTT_SOCKET_ID 0
#define HTTP_SOCKET_ID 1

#define MQTT_HOSTURL   "mqtts.heclouds.com"
#define MQTT_SERVER_IP ((uint8_t[4]){0, 0, 0, 0})
#define MQTT_PORT      1883
#define MQTT_WILLTOPIC "/wizchip/will"
#define MQTT_WILLMSG   "wizchip offline!"

#define HTTP_HOSTURL   "iot-api.heclouds.com"
#define HTTP_SERVER_IP ((uint8_t[4]){0, 0, 0, 0})

typedef enum {
    not_exist = 12012, // not exist 任务不存在
} code_error_e;

#define ETHERNET_BUF_SIZE      4096

#define MQTT_ETHERNET_BUF_SIZE 1024

#define OTA_PACKET_SIZE        2048 // 一包 ota 数据的大小

#endif