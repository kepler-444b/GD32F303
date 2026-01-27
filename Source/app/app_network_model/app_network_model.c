#include "systick.h"
#include "app_network_model.h"
#include "app_network_info.h"
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
#include "../Source/app/app_token/app_token.h"

// 函数声明
static void app_mqttconn_init(void);
static void app_httpconn_init(void);

static void timer_do_mqtt(void *arg);
static void app_mqtt_recv_msg(event_type_e event, void *params);

static void app_parse_msg(const char *msg);
static void app_parse_msg_ota(const char *msg);
static bool mqtt_client_init(void);

static void app_network_process(void *arg);
static void message_arrived(MessageData *md);
static int connect_to_mqtt(void);
static int subscribe_topic(void);
static int keep_alive(void);

static void handle_set_switch(cJSON *item, const char *id);
static void handle_set_bind_cfg(cJSON *item, const char *id);
static void handle_set_panel_cfg(cJSON *item, const char *id);
static void handle_get_soft_ver(cJSON *item, const char *id);

static void app_mqtt_set_reply(const char *topic, const char *id, const char *msg);
static void app_mqtt_get_reply(const char *topic, const char *id, const char *data);

// 全局变量
MQTTClient mqtt_client = {0}; // MQTT客户端对象
Network mqtt_net       = {0}; // MQTT底层网络接口
MQTTMessage pubmessage = {0}; // MQTT发布消息对象

mqttconn mqtt_params = {0}; // MQTT连接参数结构体
httpconn http_params = {0};

device_info_t my_device_info = {
    .products = "0GNFSdkNeZ",
    .devices  = "light_1",
    .key      = "RnI3RlpFTW1HOGZ1V2o1S1d6ZXdmdnFSVm1ManRzYmg=",
    .et       = 1779936000,
    .version  = "2018-10-31",
};

MQTTPacket_willOptions willdata = MQTTPacket_willOptions_initializer; // MQTT 遗嘱消息结构体初始化
MQTTPacket_connectData data     = MQTTPacket_connectData_initializer; // MQTT 连接参数结构体初始化

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

static uint8_t connect_status = CONNECT_MQTT; // MQTT 连接状态

static dispatch_map_t dispatch_table[] = {

    {"LightSwitch", handle_set_switch},
    {"BindInfo", handle_set_bind_cfg},
    {"PanelInfo", handle_set_panel_cfg},
    {"SoftVer", handle_get_soft_ver},
};

static uint8_t network_flag = 0;

void app_network_model_init(void)
{
    // W5500 硬件初始化
    wiz_timer_init();
    wiz_rst_int_init();
    wiz_spi_init();
    wizchip_initialize();

    network_init(ethernet_buf, &default_net_info); // 设置网络信息

    app_mqttconn_init();
    app_httpconn_init();

    app_timer_start(500, app_network_process, true, NULL, "NetWork");
    // app_eventbus_subscribe(app_mqtt_recv_msg);
}

static void app_mqttconn_init(void)
{
    snprintf(mqtt_params.mqttHostUrl, sizeof(mqtt_params.mqttHostUrl), "%s", MQTT_HOSTURL);
    memcpy(mqtt_params.server_ip, MQTT_SERVER_IP, sizeof(mqtt_params.server_ip));

    mqtt_params.port = 1883;

    snprintf(mqtt_params.property_post, sizeof(mqtt_params.property_post), "$sys/%s/%s/thing/property/post", my_device_info.products, my_device_info.devices);
    snprintf(mqtt_params.property_post_reply, sizeof(mqtt_params.property_post_reply), "$sys/%s/%s/thing/property/post/reply", my_device_info.products, my_device_info.devices);

    snprintf(mqtt_params.property_set, sizeof(mqtt_params.property_set), "$sys/%s/%s/thing/property/set", my_device_info.products, my_device_info.devices);
    snprintf(mqtt_params.property_set_reply, sizeof(mqtt_params.property_set_reply), "$sys/%s/%s/thing/property/set_reply", my_device_info.products, my_device_info.devices);

    snprintf(mqtt_params.property_get, sizeof(mqtt_params.property_get), "$sys/%s/%s/thing/property/get", my_device_info.products, my_device_info.devices);
    snprintf(mqtt_params.property_get_reply, sizeof(mqtt_params.property_get_reply), "$sys/%s/%s/thing/property/get_reply", my_device_info.products, my_device_info.devices);

    snprintf(mqtt_params.ota_inform, sizeof(mqtt_params.ota_inform), "$sys/%s/%s/ota/inform", my_device_info.products, my_device_info.devices);
    snprintf(mqtt_params.ota_inform_reply, sizeof(mqtt_params.ota_inform_reply), "$sys/%s/%s/ota/inform_reply", my_device_info.products, my_device_info.devices);

    snprintf(mqtt_params.willtopic, sizeof(mqtt_params.willtopic), "%s", MQTT_WILLTOPIC);
    snprintf(mqtt_params.willmsg, sizeof(mqtt_params.willmsg), "%s", MQTT_WILLMSG);

    mqtt_params.pubQoS  = QOS0;
    mqtt_params.willQoS = QOS0,
    mqtt_params.subQoS  = QOS0;

    app_token_generate(&my_device_info); // 获取 token

    snprintf(mqtt_params.passwd, sizeof(mqtt_params.passwd), "%s", my_device_info.passwd);
    snprintf(mqtt_params.clientid, sizeof(mqtt_params.clientid), "%s", my_device_info.devices);
    snprintf(mqtt_params.username, sizeof(mqtt_params.username), "%s", my_device_info.products);
}

static void app_httpconn_init(void)
{
    snprintf(http_params.httpHostUrl, sizeof(http_params.httpHostUrl), "%s", HTTP_HOSTURL);
    memcpy(http_params.server_ip, HTTP_SERVER_IP, sizeof(http_params.server_ip));
    http_params.port = 80;
    http_params.sn   = HTTP_SOCKET_ID;
}

static void app_network_process(void *arg)
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
        case 1: { // 连接 MQTT
            if (mqtt_client_init()) {
                APP_PRINTF("mqtt_client_init SUCCESS\n");
                network_flag++;
            } else {
                APP_ERROR("mqtt_client_init FAIL");
            }
        } break;
        case 2: { // 解析 HTTP dns
            DNS_init(http_params.sn, ethernet_buf);
            if (DNS_run(net_info.dns, (uint8_t *)http_params.httpHostUrl, http_params.server_ip)) {
                APP_PRINTF("DNS_run HTTP SUCCESS\n");
                network_flag++;
            } else {
                APP_ERROR("DNS_run HTTP FAIL");
            }
        } break;
        case 3: { // 检查 ota 任务
            if (app_ota_check(&http_params, &my_device_info, ethernet_buf)) {
                app_timer_stop("NetWork");
                APP_PRINTF("NetWork stop\n");
            }
            break;
        }
    }
}

static bool mqtt_client_init(void)
{
    NewNetwork(&mqtt_net, MQTT_SOCKET_ID);                                     // 获取网络配置信息
    if (!ConnectNetwork(&mqtt_net, mqtt_params.server_ip, mqtt_params.port)) { // 连接到 MQTT 服务器
        APP_ERROR("ConnectNetwork");
        return false;
    }

    MQTTClientInit(&mqtt_client, &mqtt_net, 1000, mqtt_send_ethernet_buf, MQTT_ETHERNET_BUF_SIZE, mqtt_recv_ethernet_buf, MQTT_ETHERNET_BUF_SIZE);
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

static void timer_do_mqtt(void *arg)
{
    switch (connect_status) {
        case CONNECT_MQTT:
            if (connect_to_mqtt() == SUCCESSS) {
                connect_status = SUB_TOPIC;
            } else {
                connect_status = ERROR_STATUS;
            }
            break;
        case SUB_TOPIC:
            if (subscribe_topic() == SUCCESSS) {
                APP_PRINTF("subscribe_topic success\n");
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
            connect_status = CONNECT_MQTT;
            APP_ERROR("ERROR_STATUS");

            break;
        default:
            break;
    }
}

/* static void app_mqtt_recv_msg(event_type_e event, void *params)
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
} */

// 解析 MQTT 收到的数据
static void app_parse_msg(const char *msg)
{
    cJSON *jsondata = cJSON_Parse(msg);
    if (!jsondata) {
        APP_PRINTF("json parse fail.\r\n");
        return;
    }

    cJSON *params      = cJSON_GetObjectItem(jsondata, "params");
    cJSON *id          = cJSON_GetObjectItem(jsondata, "id");
    const char *id_str = id ? id->valuestring : NULL;

    if (!params) {
        APP_PRINTF("no params in msg.\r\n");
        cJSON_Delete(jsondata);
        return;
    }

    for (uint8_t i = 0; i < sizeof(dispatch_table) / sizeof(dispatch_table[0]); i++) {
        const char *key = dispatch_table[i].key;

        if (cJSON_IsObject(params)) {
            cJSON *item = cJSON_GetObjectItem(params, key);
            if (item) {
                dispatch_table[i].handler(item, id_str);
            }
        } else if (cJSON_IsArray(params)) {
            cJSON *item = NULL;
            cJSON_ArrayForEach(item, params)
            {
                if (cJSON_IsString(item) && strcmp(item->valuestring, key) == 0) {
                    dispatch_table[i].handler(item, id_str);
                    break;
                }
            }
        }
    }

    cJSON_Delete(jsondata);
#if 0
    // 回复 ACK
    char replymsg[128] = {0}; // 构造 MQTT 回复 ACK 的缓存

    pubmessage.qos = QOS0;
    sprintf(replymsg, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", id->valuestring);

    pubmessage.payload    = replymsg;
    pubmessage.payloadlen = strlen(replymsg);

    ret = MQTTPublish(&mqtt_client, mqtt_params.property_set_reply, &pubmessage);

    if (ret != SUCCESSS) {
        connect_status = ERROR_STATUS;
        APP_ERROR("MQTTPublish error!\n");
    } else {
        APP_PRINTF("publish:%s,%s\r\n\r\n", mqtt_params.property_set_reply, (char *)pubmessage.payload);
    }
    cJSON_Delete(jsondata);
#endif
}

// 解析 MQTT 收到的数据
static void app_parse_msg_ota(const char *msg)
{
    int ret;

    APP_PRINTF("msg:%s\n", msg);
    cJSON *json = NULL;
    cJSON *id   = NULL;

    char replymsg[128] = {0};

    json = cJSON_Parse(msg);
    if (!json) {
        APP_PRINTF("json parse fail ota.\r\n");
        return;
    }

    id = cJSON_GetObjectItem(json, "id");
    if (!cJSON_IsString(id)) {
        APP_PRINTF("json no id ota.\r\n");
        cJSON_Delete(json);
        return;
    }

    // 构造 ACK
    snprintf(replymsg, sizeof(replymsg), "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", id->valuestring);

    pubmessage.qos        = QOS0;
    pubmessage.payload    = replymsg;
    pubmessage.payloadlen = strlen(replymsg);

    ret = MQTTPublish(&mqtt_client, mqtt_params.ota_inform_reply, &pubmessage);

    if (ret != SUCCESSS) {
        connect_status = ERROR_STATUS;
        APP_ERROR("MQTTPublish error ota!\n");
    } else {
        app_ota_check(&http_params, &my_device_info, ethernet_buf);
        APP_PRINTF("publish ack:%s\r\n", replymsg);
    }

    cJSON_Delete(json);
}

// 连接 MQTT 服务器
static int connect_to_mqtt(void)
{
    int ret;
    ret = MQTTConnect(&mqtt_client, &data);
    if (ret != SUCCESSS) {
        APP_ERROR("counnect_to_mqtt error!\n");
    }
    return ret;
}

// 订阅 MQTT 主题
static int subscribe_topic(void)
{
    int ret;
    // 订阅云端主动下发的控制命令
    ret = MQTTSubscribe(&mqtt_client, mqtt_params.property_set, mqtt_params.subQoS, message_arrived);
    if (ret != SUCCESSS) {
        APP_ERROR("property_set error!\n");
    }

    // 订阅云端对设备上报消息的回复
    ret = MQTTSubscribe(&mqtt_client, mqtt_params.property_post_reply, mqtt_params.subQoS, message_arrived);
    if (ret != SUCCESSS) {
        APP_ERROR("property_post_reply error!\n");
    }

    ret = MQTTSubscribe(&mqtt_client, mqtt_params.property_get, mqtt_params.subQoS, message_arrived);
    if (ret != SUCCESSS) {
        APP_ERROR("property_get error!\n");
    }

    // 订阅 OTA 升级通知
    ret = MQTTSubscribe(&mqtt_client, mqtt_params.ota_inform, mqtt_params.subQoS, message_arrived);
    if (ret != SUCCESSS) {
        APP_ERROR("ota inform sub error!\n");
    }

    return ret;
}

// 保活心跳包
static int keep_alive(void)
{
    int ret;
    ret = MQTTYield(&mqtt_client, 30);
    if (ret != SUCCESSS) {
        APP_ERROR("keep_alive error!\n");
    }
    return ret;
}

static void message_arrived(MessageData *md)
{
    // APP_PRINTF("Payload:%s\n", (char *)md->message->payload);

    char topicname[64] = {0};
    char msg[512]      = {0};
    memset(topicname, 0, sizeof(topicname));
    memset(msg, 0, sizeof(msg));

    sprintf(topicname, "%.*s", (int)md->topicName->lenstring.len, md->topicName->lenstring.data);
    sprintf(msg, "%.*s", (int)md->message->payloadlen, (char *)md->message->payload);

    if (strcmp(topicname, mqtt_params.ota_inform) == 0) { // OTA 相关通知
        app_parse_msg_ota(msg);

    } else {
        app_parse_msg(msg);
    }
}

static void handle_set_switch(cJSON *item, const char *id)
{
    APP_PRINTF("led_status:%d\n", item->valueint);

    app_mqtt_set_reply(mqtt_params.property_set_reply, id, NULL);
}

// 解析绑定信息
static void handle_set_bind_cfg(cJSON *item, const char *id)
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
static void handle_set_panel_cfg(cJSON *item, const char *id)
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

static void handle_get_soft_ver(cJSON *item, const char *id)
{
    APP_PRINTF("handle_get_soft_ver\n");
    const char *soft_ver_data = "{\"SoftVer\":\"V1.0.0\"}";
    app_mqtt_get_reply(mqtt_params.property_get_reply, id, soft_ver_data);
}

// 设置属性回复ACK
static void app_mqtt_set_reply(const char *topic, const char *id, const char *msg)
{
    if (!topic || !id) {
        return;
    }

    int ret;
    char replymsg[128] = {0};

    pubmessage.qos      = QOS0;
    pubmessage.retained = 0;
    pubmessage.dup      = 0;

    snprintf(replymsg, sizeof(replymsg), "{\"id\":\"%s\",\"code\":%d,\"msg\":\"%s\"}", id, 200, msg ? msg : "");

    pubmessage.payload    = replymsg;
    pubmessage.payloadlen = strlen(replymsg);

    ret = MQTTPublish(&mqtt_client, topic, &pubmessage);

    if (ret != SUCCESSS) {
        APP_ERROR("MQTT reply publish failed: %s\n", topic);
    }

    APP_PRINTF("publish:%s,%s\r\n\r\n", topic, replymsg);
}

// 获取属性回复ACK
static void app_mqtt_get_reply(const char *topic, const char *id, const char *data)
{
    if (!topic || !id) {
        return;
    }

    int ret;
    char replymsg[256] = {0}; // 获取属性可能数据较长，建议稍微加大缓冲区

    pubmessage.qos      = QOS0;
    pubmessage.retained = 0;
    pubmessage.dup      = 0;

    // 根据是否有 data 字段，拼接不同的 JSON
    if (data && strlen(data) > 0) {
        snprintf(replymsg, sizeof(replymsg), "{\"id\":\"%s\",\"code\":%d,\"data\":%s}", id, 200, data);
    } else {
        snprintf(replymsg, sizeof(replymsg), "{\"id\":\"%s\",\"code\":%d}", id, 200);
    }

    pubmessage.payload    = replymsg;
    pubmessage.payloadlen = strlen(replymsg);

    ret = MQTTPublish(&mqtt_client, topic, &pubmessage);

    if (ret != SUCCESSS) {
        APP_ERROR("MQTT reply publish failed: %s\n", topic);
    }

    APP_PRINTF("Publish Reply: Topic=%s, Payload=%s\r\n", topic, replymsg);
}