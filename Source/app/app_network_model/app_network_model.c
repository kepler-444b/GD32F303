#include "app_network_model.h"
#include "app_network_info.h"
#include "systick.h"
#include "../Source/app/app_evenbus/app_eventbus.h"
#include "../Source/app/app_base/app_base.h"
#include "../Source/app/app_timer/app_timer.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"
#include "../Source/w5500/ioLibrary_Driver/Internet/DNS/do_dns.h"
#include "../Source/w5500/ioLibrary_Driver/Internet/DNS/dns.h"
#include "../Source/w5500/wiz_interface/wiz_interface.h"
#include "../Source/w5500/wiz_platform/wiz_platform.h"
#include "../Source/w5500/ioLibrary_Driver/Ethernet/socket.h"
#include "../Source/app/app_network_model/app_http_ota.h"

MQTTClient c = {0};
Network n    = {0};

mqttconn mqtt_params = {
    .mqttHostUrl = "mqtts.heclouds.com", // MQTT服务器的URL地址
    .server_ip   = {
        0,
    },                                                             // 连接端口号，1883为MQTT默认非加密端口
    .port           = 1883,                                          /*Define the connection service port number*/
    .clientid       = DEVICES,                                       /*Define the client ID*/
    .username       = PRODUCTS,                                      /*Define the user name*/
    .passwd         = PASSWD,                                        /*Define user passwords*/
    .pubtopic       = "$sys/0GNFSdkNeZ/light_1/thing/property/post", /*Define the publication message*/
    .pubtopic_reply = "$sys/0GNFSdkNeZ/light_1/thing/property/post/reply",
    .subtopic       = "$sys/0GNFSdkNeZ/light_1/thing/property/set", /*Define subscription messages*/
    .subtopic_reply = "$sys/0GNFSdkNeZ/light_1/thing/property/set_reply",
    .pubQoS         = QOS0,               /*Defines the class of service for publishing messages*/
    .willtopic      = "/wizchip/will",    /*Define the topic of the will*/
    .willQoS        = QOS0,               /*Defines the class of service for Will messages*/
    .willmsg        = "wizchip offline!", /*Define a will message*/
    .subQoS         = QOS0,               /*Defines the class of service for subscription messages*/
};

MQTTMessage pubmessage = {
    .qos      = QOS0,
    .retained = 0,
    .dup      = 0,
    .id       = 0,
};

MQTTPacket_willOptions willdata = MQTTPacket_willOptions_initializer; /* Will subject struct initialization */
MQTTPacket_connectData data     = MQTTPacket_connectData_initializer; /*Define the parameters of the MQTT connection*/
unsigned char *data_ptr         = NULL;

wiz_NetInfo default_net_info = {
    .mac  = {0x00, 0x08, 0xdc, 0x12, 0x22, 0x12},
    .ip   = {192, 168, 40, 117},
    .gw   = {192, 168, 39, 1},
    .sn   = {255, 255, 255, 0},
    .dns  = {8, 8, 8, 8},
    .dhcp = NETINFO_STATIC};

uint8_t ethernet_buf[ETHERNET_BUF_SIZE] = {0};

static uint8_t mqtt_send_ethernet_buf[MQTT_ETHERNET_BUF_SIZE] = {0};
static uint8_t mqtt_recv_ethernet_buf[MQTT_ETHERNET_BUF_SIZE] = {0};

// 函数声明
static void timer_do_mqtt(void *arg);
static void timer_do_http(void *arg);
static void app_mqtt_recv_msg(event_type_e event, void *params);
static void app_parse_msg(const char *msg);
static bool app_mqtt_init(void);
static bool app_http_init(void);

static void app_network_init(void *arg);
static void message_arrived(MessageData *md);
static int connect_to_mqtt(void);
static int subscribe_topic(void);
static int keep_alive(void);

static void handle_light_switch(cJSON *item);
static void handle_bind_cfg(cJSON *item);
static void handle_pinel_cfg(cJSON *item);

static dispatch_map_t dispatch_table[] = {
    {"LightSwitch", handle_light_switch},
    {"BindInfo", handle_bind_cfg},
    {"PanelInfo", handle_pinel_cfg},
};

uint8_t org_server_name[] = "iot-api.heclouds.com";
uint8_t org_server_ip[4]  = {0}; /*httpbin.org IP adress */
uint8_t org_port          = 80;  /*httpbin.org port*/
uint16_t len;

static uint8_t network_flag = 0;

void app_network_model_init(void)
{
    wiz_timer_init();
    wiz_rst_int_init();
    wiz_spi_init();
    wizchip_initialize();
    network_init(ethernet_buf, &default_net_info); // 设置网络信息

    app_timer_start(500, app_network_init, true, NULL, "NetWork"); // 初始化 MQTT 与 HTTP
    app_eventbus_subscribe(app_mqtt_recv_msg);
}

static void app_network_init(void *arg)
{
    wiz_NetInfo net_info = {0};
    wizchip_getnetinfo(&net_info); // 获取当前W5500网络配置信息

    switch (network_flag) {
        case 0: { //  解析 MQTT dns
            DNS_init(MQTT_SOCKET_ID, ethernet_buf);
            if (DNS_run(net_info.dns, (uint8_t *)mqtt_params.mqttHostUrl, mqtt_params.server_ip)) {
                APP_PRINTF("DNS_run MQTT SUCCESS\n");
                network_flag++;
            } else {
                APP_ERROR("DNS_run MQTT FAIL");
            }
        } break;
        case 1: { // 初始化 MQTT
            if (app_mqtt_init()) {
                APP_PRINTF("mqtt_init SUCCESS\n");
                network_flag++;
            } else {
                APP_ERROR("mqtt_init FAIL");
            }
        } break;
        case 2: { // 解析 HTTP dns
            DNS_init(HTTP_SOCKET_ID, ethernet_buf);
            if (DNS_run(net_info.dns, org_server_name, org_server_ip)) {
                APP_PRINTF("DNS_run HTTP SUCCESS\n");
                network_flag++;
            } else {
                APP_ERROR("DNS_run HTTP FAIL");
            }
        } break;
        case 3: { // 初始化 HTTP
            if (app_http_init()) {
                app_timer_stop("NetWork");
            }
            break;
        }
    }
}

static bool app_mqtt_init(void)
{
    NewNetwork(&n, MQTT_SOCKET_ID);                                     // 获取网络配置信息
    if (!ConnectNetwork(&n, mqtt_params.server_ip, mqtt_params.port)) { // 连接到 MQTT 服务器
        return false;
    }

    MQTTClientInit(&c, &n, 1000, mqtt_send_ethernet_buf, MQTT_ETHERNET_BUF_SIZE, mqtt_recv_ethernet_buf, MQTT_ETHERNET_BUF_SIZE);
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

    app_timer_start(500, timer_do_mqtt, true, NULL, "do_mqtt");
    return true;
}

static bool app_http_init(void)
{
    app_ota_check(HTTP_SOCKET_ID, org_server_name, org_server_ip, org_port, ethernet_buf);
    return true;
}

static uint8_t connect_status = CONNECT_MQTT;
static uint8_t retry_count    = 0;
#define RETRY_MAX_COUNT 3

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

    cJSON *id       = NULL; // 指向 JSON 中 "id"
    cJSON *jsondata = NULL; // 整个 JSON 的根节点
    cJSON *params   = NULL; // 指向 "params" 子对象
    cJSON *TAG      = NULL;

    jsondata = cJSON_Parse(msg);
    if (jsondata == NULL) {
        APP_PRINTF("json parse fail.\r\n");
        return;
    }
    id     = cJSON_GetObjectItem(jsondata, "id");
    params = cJSON_GetObjectItem(jsondata, "params");
    APP_PRINTF("id:%s\n", id->valuestring);
    for (uint8_t i = 0; i < sizeof(dispatch_table) / sizeof(dispatch_table[0]); i++) {

        TAG = cJSON_GetObjectItem(params, dispatch_table[i].key);
        if (TAG) {
            dispatch_table[i].handler(TAG);
        }
    }

    // 回复 ACK
    char replymsg[128] = {0}; // 构造 MQTT 回复 ACK 的缓存

    pubmessage.qos = QOS0;
    sprintf(replymsg, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", id->valuestring);

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
static int connect_to_mqtt(void)
{
    int ret;
    ret = MQTTConnect(&c, &data);
    if (ret != SUCCESSS) {
        APP_ERROR("counnect_to_mqtt error!\n");
    }
    return ret;
}

// 订阅 MQTT 主题
static int subscribe_topic(void)
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

// 保活心跳包
static int keep_alive(void)
{
    int ret;
    ret = MQTTYield(&c, 30);
    if (ret != SUCCESSS) {
        APP_ERROR("keep_alive error!\n");
    }
    return ret;
}

static void message_arrived(MessageData *md)
{
    static char topicname[64] = {0};
    static char msg[512]      = {0};
    memset(topicname, 0, sizeof(topicname));
    memset(msg, 0, sizeof(msg));

    sprintf(topicname, "%.*s", (int)md->topicName->lenstring.len, md->topicName->lenstring.data);
    sprintf(msg, "%.*s", (int)md->message->payloadlen, (char *)md->message->payload);

    if (strcmp(topicname, mqtt_params.subtopic) == 0) {
        app_parse_msg(msg);
    }
}

static void handle_light_switch(cJSON *item)
{
    APP_PRINTF("led_status:%d\n", item->valueint);
}

static void timer_do_http(void *arg)
{
}

// 解析绑定信息
static void handle_bind_cfg(cJSON *item)
{
    static bind_cfg_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    uint16_t msg_len = strlen(item->valuestring);

    if (msg_len > sizeof(msg.bind_info)) {
        APP_ERROR("msg_len too long");
        return;
    }
    strncpy(msg.bind_info, item->valuestring, msg_len);
    msg.bind_info[msg_len] = '\0';
    app_eventbus_publish(MQTT_BIND_CFG_MSG, &msg);
}

// 解析面板配置信息
static void handle_pinel_cfg(cJSON *item)
{
    static panel_cfg_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    uint16_t msg_len = strlen(item->valuestring);
    APP_PRINTF("msg_len:%d\n", msg_len);

    if (msg_len > sizeof(msg.panel_info)) {
        APP_ERROR("msg_len too long");
        return;
    }
    strncpy(msg.panel_info, item->valuestring, msg_len);
    msg.panel_info[msg_len] = '\0';
    app_eventbus_publish(MQTT_PANEL_CFG_MSG, &msg);
}
