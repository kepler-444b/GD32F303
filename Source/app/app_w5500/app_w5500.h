#if 0
#ifndef _APP_W5500_H_
#define _APP_W5500_H_

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

typedef void (*dispatch_handler_t)(cJSON *item);

typedef struct {
    const char *key;
    dispatch_handler_t handler;
} dispatch_map_t;

void app_mqtt_model_init(void);

#endif
#endif