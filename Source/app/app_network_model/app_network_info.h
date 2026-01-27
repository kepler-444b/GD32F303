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

// #define PASSWD               "version=2018-10-31&res=products%2F0GNFSdkNeZ%2Fdevices%2Flight_1&et=1924992000&method=md5&sign=TS9AJ8IVVrwGZ4S7u4Kk0w%3D%3D"

// #define PRODUCTS             "0GNFSdkNeZ"
// #define DEVICES              "light_1"

// #define MQTT_PUB_TOPIC       "$sys/" PRODUCTS "/" DEVICES "/thing/property_set/post"
// #define MQTT_PUB_REPLY_TOPIC "$sys/" PRODUCTS "/" DEVICES "/thing/property_set/post/reply"
// #define MQTT_SUB_TOPIC       "$sys/" PRODUCTS "/" DEVICES "/thing/property_set/set"
// #define MQTT_SUB_REPLY_TOPIC "$sys/" PRODUCTS "/" DEVICES "/thing/property_set/property_set_reply"

typedef struct
{
    char products[32];
    char devices[32];
    char key[128];
    char version[32];
    char passwd[128];
    uint32_t et;
} device_info_t;

typedef enum {
    not_exist = 12012, // not exist 任务不存在
} code_error_e;

#define ETHERNET_BUF_SIZE      4096

#define MQTT_ETHERNET_BUF_SIZE 1024

#define OTA_PACKET_SIZE        2048 // 一包 ota 数据的大小

#endif