#ifndef _APP_MQTT_MODEL_H_
#define _APP_MQTT_MODEL_H_

#include "../Source/w5500/ioLibrary_Driver/Internet/MQTT/MQTTClient.h"
#include "../Source/cjson/cJSON.h"
#include <stdio.h>

typedef struct MQTTCONNECTION {
    char mqttHostUrl[128]; /*Server URL*/
    uint8_t server_ip[4];  /*Server IP*/
    int port;              /*Server port number*/
    char clientid[128];    /*client ID*/
    char username[128];    /*user name*/
    char passwd[128];      /*user passwords*/
    char pubtopic[255];    /*publication*/
    char pubtopic_reply[255];
    char subtopic[255]; /*subscription*/
    char subtopic_reply[255];
    enum QoS pubQoS;     /* publishing messages*/
    enum QoS subQoS;     /* subscription messages*/
    char willtopic[255]; /*Will topic  */
    enum QoS willQoS;    /*Will message  */
    char willmsg[255];   /*Will */
} mqttconn;

// 绑定按键信息结构体
typedef struct {
    char bind_info[160];
} bind_cfg_msg_t;

typedef struct {
    char panel_info[82]; // 要多分配2个字节来存放"\0"
} panel_cfg_msg_t;

typedef void (*dispatch_handler_t)(cJSON *item);

typedef struct {
    const char *key;
    dispatch_handler_t handler;
} dispatch_map_t;

void app_mqtt_model_init(void);

#endif