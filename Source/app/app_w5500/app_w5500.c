#if 0
#include "app_w5500.h"
#include "../Source/app/app_evenbus/app_eventbus.h"
#include "../Source/app/app_timer/app_timer.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"
#include "../Source/w5500/ioLibrary_Driver/Ethernet/wizchip_conf.h"
#include "../Source/w5500/ioLibrary_Driver/Internet/DNS/do_dns.h"
#include "../Source/w5500/wiz_interface/wiz_interface.h"
#include "../Source/w5500/wiz_platform/wiz_platform.h"
#include "systick.h"

#define MQTT_ETHERNET_MAX_SIZE (1024 * 2)

MQTTClient c = {0};
Network n    = {0};

mqttconn mqtt_params = {
    .mqttHostUrl = "mqtts.heclouds.com", // MQTT服务器的URL地址
    .server_ip   = {
        0,
    },                                                                                                                                       // 连接端口号，1883为MQTT默认非加密端口
    .port     = 1883,                                                                                                                          /*Define the connection service port number*/
    .clientid = "light_1",                                                                                                                     /*Define the client ID*/
    .username = "0GNFSdkNeZ",                                                                                                                  /*Define the user name*/
    .passwd   = "version=2018-10-31&res=products%2F0GNFSdkNeZ%2Fdevices%2Flight_1&et=1924992000&method=md5&sign=TS9AJ8IVVrwGZ4S7u4Kk0w%3D%3D", /*Define user passwords*/

    .pubtopic       = "$sys/0GNFSdkNeZ/light_1/thing/property/post", /*Define the publication message*/
    .pubtopic_reply = "$sys/0GNFSdkNeZ/light_1/thing/property/post/reply",
    .subtopic       = "$sys/0GNFSdkNeZ/light_1/thing/property/set", /*Define subscription messages*/
    .subtopic_reply = "$sys/0GNFSdkNeZ/light_1/thing/property/set_reply",

    .pubQoS    = QOS0,               /*Defines the class of service for publishing messages*/
    .willtopic = "/wizchip/will",    /*Define the topic of the will*/
    .willQoS   = QOS0,               /*Defines the class of service for Will messages*/
    .willmsg   = "wizchip offline!", /*Define a will message*/
    .subQoS    = QOS0,               /*Defines the class of service for subscription messages*/
};

typedef enum {
    CONNECT_MQTT,
    SUB_TOPIC,
    KEEP_ALIVE,
    ERROR_STATUS
} mqtt_status_e;

MQTTMessage pubmessage = {
    .qos      = QOS0,
    .retained = 0,
    .dup      = 0,
    .id       = 0,
};

static char mqtt_recv_msg[512]  = {0};
static uint8_t mqtt_recv_flag   = 0;
MQTTPacket_willOptions willdata = MQTTPacket_willOptions_initializer; /* Will subject struct initialization */
MQTTPacket_connectData data     = MQTTPacket_connectData_initializer; /*Define the parameters of the MQTT connection*/
unsigned char *data_ptr         = NULL;

#define SOCKET_ID              0
#define ETHERNET_BUF_MAX_SIZE  (1024 * 2)

wiz_NetInfo default_net_info = {
    .mac  = {0x00, 0x08, 0xdc, 0x12, 0x22, 0x12},
    .ip   = {192, 168, 40, 30},
    .gw   = {192, 168, 39, 1},
    .sn   = {255, 255, 255, 0},
    .dns  = {8, 8, 8, 8},
    .dhcp = NETINFO_STATIC}; // static ip
uint8_t ethernet_buf[ETHERNET_BUF_MAX_SIZE] = {0};

static uint8_t mqtt_send_ethernet_buf[ETHERNET_BUF_MAX_SIZE] = {0};
static uint8_t mqtt_recv_ethernet_buf[ETHERNET_BUF_MAX_SIZE] = {0};

// 函数声明
static void timer_do_mqtt(void *arg);
static void app_mqtt_recv_msg(event_type_e event, void *params);
static void app_parse_msg(const char *msg);
void app_mqtt_init(uint8_t sn, uint8_t *send_buf, uint8_t *recv_buf);
void message_arrived(MessageData *md);
int connect_to_mqtt(void);
int subscribe_topic(void);
int keep_alive(void);

static void handle_light_switch(cJSON *item);
static void hanele_bind_info(cJSON *item);

static dispatch_map_t dispatch_table[] = {
    {"LightSwitch", handle_light_switch},
    {"BindInfo", hanele_bind_info},
};

void app_mqtt_model_init(void)
{
    wiz_timer_init();
    wiz_rst_int_init();
    wiz_spi_init();
    wizchip_initialize();

    network_init(ethernet_buf, &default_net_info); // 设置网络信息
    wiz_NetInfo net_info;
    wizchip_getnetinfo(&net_info);
    app_mqtt_init(SOCKET_ID, mqtt_send_ethernet_buf, mqtt_recv_ethernet_buf);
    app_eventbus_subscribe(app_mqtt_recv_msg);
    app_timer_start(1000, timer_do_mqtt, true, NULL, "timer_do_mqtt");
}

void app_mqtt_init(uint8_t sn, uint8_t *send_buf, uint8_t *recv_buf)
{
    wiz_NetInfo get_info = {0};
    wizchip_getnetinfo(&get_info);
    if (do_dns(send_buf, (uint8_t *)mqtt_params.mqttHostUrl, mqtt_params.server_ip)) {
        if (mqtt_params.server_ip[0] != 0) {
            printf("Use Default Server Internet Protocol Address:%d.%d.%d.%d\r\n", mqtt_params.server_ip[0], mqtt_params.server_ip[1], mqtt_params.server_ip[2], mqtt_params.server_ip[3]);
        } else {
            printf("No default server address, unable to connect to the server\r\n");
            while (1) {
            }
        }
    }
    NewNetwork(&n, sn);                                          // 获取网络配置信息
    ConnectNetwork(&n, mqtt_params.server_ip, mqtt_params.port); // 连接到 MQTT 服务器
    MQTTClientInit(&c, &n, 1000, send_buf, MQTT_ETHERNET_MAX_SIZE, recv_buf, MQTT_ETHERNET_MAX_SIZE);
    data.willFlag                     = 0;                                         /* will flag: If the will annotation bit is 0, the following will-related settings are invalid*/
    willdata.qos                      = mqtt_params.willQoS;                       /* will QoS */
    willdata.topicName.lenstring.data = mqtt_params.willtopic;                     /* will topic */
    willdata.topicName.lenstring.len  = strlen(willdata.topicName.lenstring.data); /* will topic len */
    willdata.message.lenstring.data   = mqtt_params.willmsg;                       /* will message */
    willdata.message.lenstring.len    = strlen(willdata.message.lenstring.data);   /* will message len */
    willdata.retained                 = 0;
    willdata.struct_version           = 3;
    data.will                         = willdata;
    data.MQTTVersion                  = 4;
    data.clientID.cstring             = mqtt_params.clientid;
    data.username.cstring             = mqtt_params.username;
    data.password.cstring             = mqtt_params.passwd;
    data.keepAliveInterval            = 10;
    data.cleansession                 = 1;
}

static uint8_t connect_status = CONNECT_MQTT;
static uint8_t retry_count    = 0;
#define RETRY_MAX_COUNT        3

static void timer_do_mqtt(void *arg)
{
    switch (connect_status) {
        case CONNECT_MQTT:
            if (connect_to_mqtt() == SUCCESSS) {
                connect_status = SUB_TOPIC;
                retry_count    = 0;
            } else {
                connect_status = ERROR_STATUS;
            }
            break;
        case SUB_TOPIC:
            if (subscribe_topic() == SUCCESSS) {
                connect_status = KEEP_ALIVE;
            } else {
                connect_status = ERROR_STATUS;
            }
            break;
        case KEEP_ALIVE:
            if (keep_alive() != SUCCESSS) {
                connect_status = ERROR_STATUS;
            }
            // APP_PRINTF("keep_alive\n");
            break;
        case ERROR_STATUS:
            if (retry_count < RETRY_MAX_COUNT) {
                retry_count++;
                APP_PRINTF("MQTT error! Retry %d/%d\n", retry_count, RETRY_MAX_COUNT);
                connect_status = CONNECT_MQTT;
            } else {
                APP_ERROR("Connect to MQTT failed after %d retries!\n", RETRY_MAX_COUNT);
            }
            break;
        default:
            break;
    }
}

static void app_mqtt_recv_msg(event_type_e event, void *params)
{
    switch (event) {
        case EVENT_MQTT_RECV_MSG: {
            const char *msg = (char *)params;
            app_parse_msg(msg);
            break;
        }
        default:
            break;
    }
}

// 解析 MQTT 收到的数据
static void app_parse_msg(const char *msg)
{
    int ret;
    char replymsg[128] = {0};  // 构造 MQTT 回复 ACK 的缓存
    cJSON *id          = NULL; // 指向 JSON 中 "id"
    cJSON *jsondata    = NULL; // 整个 JSON 的根节点
    cJSON *params      = NULL; // 指向 "params" 子对象
    cJSON *TAG         = NULL;
    jsondata           = cJSON_Parse(msg);
    if (jsondata == NULL) {
        APP_PRINTF("json parse fail.\r\n");
        return;
    }
    id     = cJSON_GetObjectItem(jsondata, "id");
    params = cJSON_GetObjectItem(jsondata, "params");

    for (uint8_t i = 0; i < sizeof(dispatch_table) / sizeof(dispatch_table[0]); i++) {

        TAG = cJSON_GetObjectItem(params, dispatch_table[i].key);
        if (TAG) {
            dispatch_table[i].handler(TAG);
        }
    }

    // 回复 ACK
    pubmessage.qos = QOS0;
    sprintf(replymsg, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", id->valuestring);
    APP_PRINTF("reply:%s\r\n", replymsg);
    pubmessage.payload    = replymsg;
    pubmessage.payloadlen = strlen(replymsg);
    ret                   = MQTTPublish(&c, mqtt_params.subtopic_reply, &pubmessage);
    if (ret != SUCCESSS) {
        connect_status = ERROR_STATUS;
        APP_ERROR("MQTTPublish error!\n");
    } else {
        APP_PRINTF("publish:%s,%s\r\n\r\n", mqtt_params.subtopic_reply, (char *)pubmessage.payload);
    }
    cJSON_Delete(jsondata);
}

// 连接 MQTT 服务器
int connect_to_mqtt(void)
{
    int ret;
    ret = MQTTConnect(&c, &data);
    if (ret != SUCCESSS) {
        APP_ERROR("counnect_to_mqtt error!\n");
    }
    return ret;
}

// 订阅 MQTT 主题
int subscribe_topic(void)
{
    int ret;
    ret = MQTTSubscribe(&c, mqtt_params.subtopic, mqtt_params.subQoS, message_arrived);
    if (ret != SUCCESSS) {
        APP_ERROR("subtopic error!\n");
    }
    ret = MQTTSubscribe(&c, mqtt_params.pubtopic_reply, mqtt_params.subQoS, message_arrived);

    if (ret != SUCCESSS) {
        APP_ERROR("pubtopic_reply error!\n");
    }
    return ret;
}

int keep_alive(void)
{
    int ret;
    ret = MQTTYield(&c, 30);
    if (ret != SUCCESSS) {
        APP_ERROR("keep_alive error!\n");
    }
    return ret;
}

void message_arrived(MessageData *md)
{
    char topicname[64] = {0};
    char msg[512]      = {0};
    sprintf(topicname, "%.*s", (int)md->topicName->lenstring.len, md->topicName->lenstring.data);
    sprintf(msg, "%.*s", (int)md->message->payloadlen, (char *)md->message->payload);

    if (strcmp(topicname, mqtt_params.subtopic) == 0) {
        memset(mqtt_recv_msg, 0, sizeof(mqtt_recv_msg));
        memcpy(mqtt_recv_msg, msg, strlen(msg));
        app_eventbus_publish(EVENT_MQTT_RECV_MSG, &mqtt_recv_msg);
    }
}

static void handle_light_switch(cJSON *item)
{
    APP_PRINTF("led_status:%d\n", item->valueint);
}

static void hanele_bind_info(cJSON *item)
{
    APP_PRINTF("config :%s\n", item->valuestring);
}
#endif