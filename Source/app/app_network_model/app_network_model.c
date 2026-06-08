#include "systick.h"
#include <stdlib.h>
#include "app_network_model.h"
#include "app_network_info.h"
#include "../Source/app/app_public_protocol/app_public_protocol.h"
#include "../Source/app/app_public_protocol/app_public.h"
#include "../Source/app/app_evenbus/app_eventbus.h"
#include "../Source/app/app_base/app_base.h"
#include "../Source/app/app_timer/app_timer.h"
#include "../Source/app/app_rtc/app_rtc.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"
#include "../Source/bsp/bsp_gpio/bsp_gpio.h"
#include "../Source/bsp/bsp_adc/bsp_adc.h"
#include "../Source/w5500/ioLibrary_Driver/Internet/DNS/dns.h"
#include "../Source/w5500/wiz_interface/wiz_interface.h"
#include "../Source/w5500/wiz_platform/wiz_platform.h"
#include "../Source/w5500/ioLibrary_Driver/Ethernet/socket.h"
#include "../Source/app/app_network_model/app_http_ota.h"
#include "../Source/app/app_token/app_token.h"
#include "../Source/dev/dev_info.h"

// 函数声明
static void app_network_init(void);
static void app_mqttconn_init(void);
static void app_httpconn_init(void);
static bool mqtt_client_init(void);
static int connect_to_mqtt(void);
static int subscribe_topic(void);
static void timer_do_mqtt(void *arg);

static void app_networt_status(event_type_e event, void *params);
static void app_parse_msg(const char *msg, mqtt_type_e type);
static void app_parse_msg_ota(const char *msg);

static void app_network_mqtt_task(void *arg);
static void app_network_http_task(void *arg);

static void message_arrived(MessageData *md);
static int keep_alive(void);

// post 类型
static void handle_board_temp_post(float temp);
static void handle_dev_time_post(void);
static void handle_soft_ver_post(void);

static void handle_dev_time(cJSON *item, const char *id, mqtt_type_e type);
static void handle_board_temp(cJSON *item, const char *id, mqtt_type_e type);
static void handle_extend_state(cJSON *item, const char *id, mqtt_type_e type);
static void handle_set_info(cJSON *item, const char *id, mqtt_type_e type);
static void handle_soft_ver(cJSON *item, const char *id, mqtt_type_e type);
static void handle_timer_task(cJSON *item, const char *id, mqtt_type_e type);

static void app_mqtt_post(const char *topic, const char *id, const char *data);
static void app_mqtt_set_reply(const char *topic, const char *id, const char *msg);
static void app_mqtt_get_reply(const char *topic, const char *id, const char *data);
static void app_mqtt_service_reply(const char *topic, const char *id, char *data);

// 全局变量
MQTTClient mqtt_client = {0}; // MQTT客户端对象
Network mqtt_net       = {0}; // MQTT底层网络接口
MQTTMessage pubmessage = {0}; // MQTT发布消息对象

mqttconn mqtt_params = {0}; // MQTT连接参数结构体
httpconn http_params = {0};

dev_save_info_t my_device_info;
MQTTPacket_willOptions willdata = MQTTPacket_willOptions_initializer; // MQTT 遗嘱消息结构体初始化
MQTTPacket_connectData data     = MQTTPacket_connectData_initializer; // MQTT 连接参数结构体初始化

wiz_NetInfo net_info = {0};

uint8_t ethernet_buf[ETHERNET_BUF_SIZE] = {0};

static uint8_t mqtt_send_ethernet_buf[MQTT_ETHERNET_BUF_SIZE] = {0};
static uint8_t mqtt_recv_ethernet_buf[MQTT_ETHERNET_BUF_SIZE] = {0};

static uint8_t do_mqtt_flag     = CONNECT_MQTT; // MQTT 客户端状态机
static uint8_t do_mqtt_err_flag = 0;

static uint8_t mqtt_task_flag = MQTT_DNS_INIT; // 连接 MQTT 服务器任务状态
static uint8_t mqtt_task_err  = 0;

static uint8_t http_task_flag = HTTP_DNS_INIT; // 连接 HTTQ 服务器任务状态
static uint8_t http_task_err  = 0;
static bool g_report          = true;

#define DISPATCH_SIZE (sizeof(dispatch_table) / sizeof(dispatch_table[0]))
static const mqtt_dispatch_t dispatch_table[] =
    {
        {"SetInfo", handle_set_info}, // 服务
        {"DevTime", handle_dev_time},
        {"BoardTemp", handle_board_temp},
        {"ExtendState", handle_extend_state},
        {"SoftVer", handle_soft_ver},
        {"TimerTask", handle_timer_task},
};

void app_network_model_init(void)
{
    // W5500 硬件初始化
    wiz_timer_init();
    wiz_rst_int_init();
    wiz_spi_init();
    wizchip_initialize();
    app_eventbus_subscribe(app_networt_status);
    my_device_info = *dev_get_save_device_info();
}

static void app_networt_status(event_type_e event, void *params)
{
    switch (event) {
        case EVENT_USER_LINK_ON: // 手动联网
        case EVENT_PHY_LINK_ON:  // 网线插入
            app_network_init();
            break;
        case EVENT_PHY_LINK_OFF: // 网线拔出
            app_timer_stop("do_mqtt");
            app_timer_stop("mqtt_task");
            app_timer_stop("http_task");
            break;
        case EVENT_MQTT_CONNECT: // mqtt 建立连接
            app_mqttconn_init();
            app_timer_start(500, app_network_mqtt_task, true, NULL, "mqtt_task");
            break;
        case EVENT_REPORT_TEMP: {           // 上报主板温度
            float value = *(float *)params; // 取浮点值
            handle_board_temp_post(value);
        } break;
        case EVENT_REPORT_GET_TIME:
            handle_dev_time_post();
            handle_soft_ver_post();
            break;
        case EVENT_USART0_SET_CFG: {
            type_c_rx_t *temp = (type_c_rx_t *)params;
            char msg[512];
            uint8_t *data = temp->buffer;
            uint16_t len  = temp->length;

            if (len >= 8 && memcmp(data, "SetScene", 8) == 0) {
                app_bytes_to_string(data + 8, len - 8, msg);
                app_public_scene_cfg_parse(msg);

            } else if (len >= 9 && memcmp(data, "BindScene", 9) == 0) {
                app_bytes_to_string(data + 9, len - 9, msg);
                app_public_bind_scene_cfg_parse(msg);
            } else if (len >= 8 && memcmp(data, "DelScene", 8) == 0) {
                app_public_del_cfg("DelScene");
            } else if (len >= 7 && memcmp(data, "DelBind", 7) == 0) {
                app_public_del_cfg("DelBind");
            }
        } break;
        default:
            break;
    }
}

static void app_network_init(void)
{
    APP_PRINTF("[app_network_init] ===============================\n");
    if (strcmp(my_device_info.devices, "NULL") == 0) { // 如果设备名为NULL,即代表没有创建该产品,不启动网络连接
        APP_ERROR("devices is NULL");
        return;
    }
    mqtt_task_flag = MQTT_DNS_INIT;
    mqtt_task_err  = 0;
    app_timer_stop("do_mqtt");
    app_timer_stop("mqtt_task");
    do_mqtt_flag = CONNECT_MQTT;
    memcpy(&net_info, dev_get_nw_cfg_info(), sizeof(net_info));

    network_init(ethernet_buf, &net_info); // 设置网络信息
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

    snprintf(mqtt_params.service_invoke, sizeof(mqtt_params.service_invoke), "$sys/%s/%s/thing/service/SetInfo/invoke", my_device_info.products, my_device_info.devices);
    snprintf(mqtt_params.service_invoke_reply, sizeof(mqtt_params.service_invoke_reply), "$sys/%s/%s/thing/service/SetInfo/invoke_reply", my_device_info.products, my_device_info.devices);

    snprintf(mqtt_params.ota_inform, sizeof(mqtt_params.ota_inform), "$sys/%s/%s/ota/inform", my_device_info.products, my_device_info.devices);
    snprintf(mqtt_params.ota_inform_reply, sizeof(mqtt_params.ota_inform_reply), "$sys/%s/%s/ota/inform_reply", my_device_info.products, my_device_info.devices);

    snprintf(mqtt_params.willtopic, sizeof(mqtt_params.willtopic), "%s", MQTT_WILLTOPIC);
    snprintf(mqtt_params.willmsg, sizeof(mqtt_params.willmsg), "%s", MQTT_WILLMSG);

    mqtt_params.pubQoS  = QOS0;
    mqtt_params.willQoS = QOS0;
    mqtt_params.subQoS  = QOS0;

    app_token_generate(&my_device_info); // 获取 token
    snprintf(mqtt_params.passwd, sizeof(mqtt_params.passwd), "%s", my_device_info.passwd);
    snprintf(mqtt_params.clientid, sizeof(mqtt_params.clientid), "%s", my_device_info.devices);
    snprintf(mqtt_params.username, sizeof(mqtt_params.username), "%s", my_device_info.products);
    APP_PRINTF("devices:%s\n", my_device_info.devices);
    APP_PRINTF("products:%s\n", my_device_info.products);
}

static void app_httpconn_init(void)
{
    snprintf(http_params.httpHostUrl, sizeof(http_params.httpHostUrl), "%s", HTTP_HOSTURL);
    memcpy(http_params.server_ip, HTTP_SERVER_IP, sizeof(http_params.server_ip));
    http_params.port = 80;
    http_params.sn   = HTTP_SOCKET_ID;
}

// 连接 mqtt 服务器任务
static void app_network_mqtt_task(void *arg)
{
    switch (mqtt_task_flag) {

        case MQTT_DNS_INIT: //  解析 MQTT dns
            wizchip_getnetinfo(&net_info);
            DNS_init(MQTT_SOCKET_ID, ethernet_buf);
            int8_t ret = DNS_run_nb(net_info.dns, (uint8_t *)mqtt_params.mqttHostUrl, mqtt_params.server_ip);
            if (ret == 1) {
                APP_PRINTF("dns_run MQTT SUCCESS\n");
                mqtt_task_flag = CLIENT_INIT;
            } else if (ret == -1) {
                APP_PRINTF("dns_run MQTT BEING\n");
            } else if (ret == 0) {
                APP_PRINTF("dns_run MQTT RETRY\n");
                mqtt_task_err++;
            }
            break;

        case CLIENT_INIT: // 创建 MQTT 客户端
            if (mqtt_client_init()) {
                APP_PRINTF("mqtt_client_init SUCCESS\n");
                mqtt_task_flag = CLIENT_SUCC;
            } else {
                APP_PRINTF("mqtt_client_init RETRY");
            }
            break;

        case CLIENT_SUCC:
            app_timer_stop("mqtt_task"); // 成功后关闭定时器

            // 向OTA升级上报软件版本
            app_httpconn_init();
            app_timer_start(500, app_network_http_task, true, NULL, "http_task");

            break;
        default:
            return;
    }

    if (mqtt_task_err >= 5) { // 无网络连接
        mqtt_task_err = 0;
        app_timer_stop("mqtt_task");
        APP_PRINTF("not network connection\n");
    }
}

// 连接 http 服务器
static void app_network_http_task(void *arg)
{
    switch (http_task_flag) {
        case HTTP_DNS_INIT: { //  解析 HTTP dns
            wizchip_getnetinfo(&net_info);
            DNS_init(http_params.sn, ethernet_buf);
            int8_t ret = DNS_run_nb(net_info.dns, (uint8_t *)http_params.httpHostUrl, http_params.server_ip);
            if (ret == 1) {
                APP_PRINTF("dns_run HTTP SUCCESS\n");
                http_task_flag = HTTP_OTA_CHECK;
            } else if (ret == 0) {
                APP_PRINTF("dns_run HTTP RETRY");
                mqtt_task_err++;
            }
        } break;
        case HTTP_OTA_CHECK: { // 检查软件版本
            app_ota_check(&http_params, &my_device_info, ethernet_buf, g_report);
            app_timer_stop("http_task");

        } break;
        default:
            return;
    }
}

// 创建 mqtt 客户端
static bool mqtt_client_init(void)
{
    NewNetwork(&mqtt_net, MQTT_SOCKET_ID); // 获取网络配置信息

    if (ConnectNetwork_nb(&mqtt_net, mqtt_params.server_ip, mqtt_params.port) != SOCK_OK) {
        return false; // 连接失败
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
    data.keepAliveInterval            = 30;
    data.cleansession                 = 1;

    app_timer_start(1000, timer_do_mqtt, true, NULL, "do_mqtt");
    return true; // 连接成功
}

// mqtt 客户端状态机
static void timer_do_mqtt(void *arg)
{
    switch (do_mqtt_flag) {
        case CONNECT_MQTT:
            if (connect_to_mqtt() == SUCCESSS) {
                do_mqtt_flag = SUB_TOPIC;
            } else {
                do_mqtt_flag = ERROR_STATUS;
            }
            break;
        case SUB_TOPIC:
            if (subscribe_topic() == SUCCESSS) {
                APP_PRINTF("subscribe_topic SUCCESSS\n");
                do_mqtt_flag = KEEP_ALIVE;
            } else {
                do_mqtt_flag = ERROR_STATUS;
            }
            break;
        case KEEP_ALIVE:
            if (keep_alive() == SUCCESSS) {
                // APP_PRINTF("keep_alive SUCCESSS\n");
            } else {
                APP_PRINTF("keep_alive ERROR\n");
                do_mqtt_flag = ERROR_STATUS;
            }
            break;
        case ERROR_STATUS:
            do_mqtt_flag = CONNECT_MQTT;
            APP_ERROR("ERROR_STATUS");
            do_mqtt_err_flag++;
            break;
        default:
            break;
    }

    if (do_mqtt_err_flag >= 10) {
        app_timer_stop("do_mqtt");
        do_mqtt_err_flag = 0;
        APP_ERROR("MQTT reconnect failed 10 times\r\n");
    }
}

// 解析 MQTT 收到的数据
static void app_parse_msg(const char *msg, mqtt_type_e type)
{
    cJSON *jsondata = cJSON_Parse(msg); // 字符串转换为json
    if (!jsondata) {
        APP_PRINTF("json parse fail\r\n");
        return;
    }

    cJSON *params      = cJSON_GetObjectItem(jsondata, "params");
    cJSON *id          = cJSON_GetObjectItem(jsondata, "id");
    const char *id_str = cJSON_IsString(id) ? id->valuestring : NULL;
    APP_PRINTF("%s\n", msg);

    if (!params) {
        APP_PRINTF("no params\r\n");
        cJSON_Delete(jsondata);
        return;
    }

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, params)
    {
        const char *key = NULL;
        // 根据类型统一取 key
        if (type == MQTT_PRO_GET) { // GET 属性是数组
            if (cJSON_IsString(item)) key = item->valuestring;
        } else { // SET 或 SERVICE 是对象
            key = item->string;
        }

        if (!key) continue;
        for (uint8_t i = 0; i < DISPATCH_SIZE; i++) {
            if (strcmp(dispatch_table[i].key, key) == 0) {
                dispatch_table[i].handler(item, id_str, type);
                break;
            }
        }
    }

    cJSON_Delete(jsondata);
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
        do_mqtt_flag = ERROR_STATUS;
        APP_ERROR("MQTTPublish error ota!\n");
    } else {
        app_ota_check(&http_params, &my_device_info, ethernet_buf, false);
        APP_PRINTF("publish ack:%s\r\n", replymsg);
    }
    cJSON_Delete(json);
}

// 连接 MQTT 服务器
static int connect_to_mqtt(void)
{
    int ret;
    ret = MQTTConnect(&mqtt_client, &data);
    if (ret == SUCCESSS) {
        APP_PRINTF("connect_to_mqtt SUCCESSS\n");
    } else if (ret != SUCCESSS) {
        APP_ERROR("counnect_to_mqtt ERROR!\n");
    }
    return ret;
}

// 订阅 MQTT 主题
static int subscribe_topic(void)
{
    int ret;

    // 订阅云端 set 的控制命令
    ret = MQTTSubscribe(&mqtt_client, mqtt_params.property_set, mqtt_params.subQoS, message_arrived);
    if (ret != SUCCESSS) {
        APP_ERROR("property_set error!\n");
    }

    // 订阅云端对 post 消息的回复
    ret = MQTTSubscribe(&mqtt_client, mqtt_params.property_post_reply, mqtt_params.subQoS, message_arrived);
    if (ret != SUCCESSS) {
        APP_ERROR("property_post_reply error!\n");
    }

    // 订阅云端 get 的控制命令
    ret = MQTTSubscribe(&mqtt_client, mqtt_params.property_get, mqtt_params.subQoS, message_arrived);
    if (ret != SUCCESSS) {
        APP_ERROR("property_get error!\n");
    }

    // 订阅云端 service 的控制命令
    ret = MQTTSubscribe(&mqtt_client, mqtt_params.service_invoke, mqtt_params.subQoS, message_arrived);
    if (ret != SUCCESSS) {
        APP_ERROR("service_invoke_SoftUptate error!\n");
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
    ret = MQTTYield(&mqtt_client, 100);
    if (ret != SUCCESSS) {
        APP_ERROR("keep_alive error!\n");
    }
    return ret;
}

// 分发云端数据
static void message_arrived(MessageData *md)
{
    char topicname[64] = {0};
    char msg[512]      = {0};
    memset(topicname, 0, sizeof(topicname));
    memset(msg, 0, sizeof(msg));

    snprintf(topicname, sizeof(topicname), "%.*s", (int)md->topicName->lenstring.len, md->topicName->lenstring.data);
    snprintf(msg, sizeof(msg), "%.*s", (int)md->message->payloadlen, (char *)md->message->payload);

    if (strcmp(topicname, mqtt_params.ota_inform) == 0) { // OTA 相关数据
        app_parse_msg_ota(msg);
        return;
    }
    if (strcmp(topicname, mqtt_params.property_set) == 0) { // set 相关数据
        app_parse_msg(msg, MQTT_PRO_SET);
        return;
    }
    if (strcmp(topicname, mqtt_params.property_get) == 0) { // get 相关数据
        app_parse_msg(msg, MQTT_PRO_GET);
        return;
    }
    if (strcmp(topicname, mqtt_params.service_invoke) == 0) { // 服务相关数据
        app_parse_msg(msg, MQTT_SERVICE);
        return;
    }
    if (strcmp(topicname, mqtt_params.property_post_reply) == 0) { // 云端回复 post 数据
        APP_PRINTF("post_reply:%s\n", msg);
    }
}

// 设备时间
static void handle_dev_time(cJSON *item, const char *id, mqtt_type_e type)
{
    APP_PRINTF("handle_dev_time\n");
    switch (type) {
        case MQTT_PRO_SET: {
            uint32_t unix_time = (uint32_t)strtoul(item->valuestring, NULL, 10);
            rct_set_unix_time(unix_time);
            app_mqtt_set_reply(mqtt_params.property_set_reply, id, NULL); // 回复ACK
            break;
        }
        case MQTT_PRO_GET: {
            char get_buf[32];
            uint32_t time = 0;
            rct_get_unix_time(&time);
            snprintf(get_buf, sizeof(get_buf), "{\"DevTime\":\"%d\"}", time);
            app_mqtt_get_reply(mqtt_params.property_get_reply, id, get_buf);
        } break;
        default:
            break;
    }
}

// 主动获取当前时间
static void handle_dev_time_post(void)
{
    char post_buf[64];
    snprintf(post_buf, sizeof(post_buf), "{\"DevTime\":{\"value\":\"get_time\"}}");
    app_mqtt_post(mqtt_params.property_post, "000", post_buf);
}

// 软件版本
static void handle_soft_ver(cJSON *item, const char *id, mqtt_type_e type)
{
    switch (type) {
        case MQTT_PRO_GET: {
            const dev_save_info_t *temp_info = dev_get_save_device_info();

            char property_buf[32] = {0};
            snprintf(property_buf, sizeof(property_buf), "{\"SoftVer\":\"%s\"}", temp_info->cur_ver);
            app_mqtt_get_reply(mqtt_params.property_get_reply, id, property_buf);
        } break;
        default:
            break;
    }
}

// 主动上报软件版本
static void handle_soft_ver_post(void)
{
    char post_buf[64];
    const dev_save_info_t *temp_info = dev_get_save_device_info();
    snprintf(post_buf, sizeof(post_buf), "{\"SoftVer\":{\"value\":\"%s\"}}", temp_info->cur_ver);
    app_mqtt_post(mqtt_params.property_post, "000", post_buf);
}

// 定时任务
static void handle_timer_task(cJSON *item, const char *id, mqtt_type_e type)
{
    switch (type) {
        case MQTT_PRO_SET: {
            app_public_timer_task_cfg_parse(item->valuestring);
            app_mqtt_set_reply(mqtt_params.property_set_reply, id, NULL); // 回复ACK
        } break;
        case MQTT_PRO_GET: {
            const timer_task_t *temp_info = app_public_get_timer_task();

            char get_buf[160] = {0};
            int offset        = 0;
            int len           = 0;

            offset = snprintf(get_buf, sizeof(get_buf), "{\"TimerTask\":\"");
            for (uint8_t i = 0; i < TIMER_TASK_MAX && offset < sizeof(get_buf); i++) {
                len = snprintf(get_buf + offset, sizeof(get_buf) - offset, "%02x%02x%02x%02x%02x%02x",
                               i, temp_info[i].scene_id, temp_info[i].enable, temp_info[i].hour, temp_info[i].min, temp_info[i].reserve);

                if (len < 0 || len >= sizeof(get_buf) - offset) {
                    APP_PRINTF("buffer overflow!\n");
                    break;
                }
                offset += len;
            }
            snprintf(get_buf + offset, sizeof(get_buf) - offset, "\"}");
            app_mqtt_get_reply(mqtt_params.property_get_reply, id, get_buf);

        } break;
        default:
            break;
    }
}

// 扩展状态
static void handle_extend_state(cJSON *item, const char *id, mqtt_type_e type)
{
    switch (type) {
        case MQTT_PRO_SET: {
            const char *str = cJSON_GetStringValue(item);
            app_puublic_set_extend(str);
            app_mqtt_set_reply(mqtt_params.property_set_reply, id, NULL); // 回复ACK

        } break;
        case MQTT_PRO_GET: {
            extend_all_status_t *obj = app_public_get_extend();

            char relay_str[72 + 1] = {0}; // 存储继电器状态字符串
            char led_str[64 + 1]   = {0}; // 存储led状态字符串

            char state_str[200] = {0};

            app_unpack_bits(obj->relay_sel_1, 6, relay_str);
            app_unpack_bits(obj->relay_sel_2, 3, relay_str + (6 * 8));

            uint8_t led_buf[32];
            memcpy(led_buf, obj->led_sel_1, 4);
            memcpy(led_buf + 4, obj->led_sel_2, 12);
            memcpy(led_buf + 16, obj->led_sel_3, 8);
            memcpy(led_buf + 24, obj->led_sel_4, 8);

            app_bytes_to_string(led_buf, 32, led_str);
            snprintf(state_str, sizeof(state_str), "{\"ExtendState\":\"%s%s\"}", relay_str, led_str);
            app_mqtt_get_reply(mqtt_params.property_get_reply, id, state_str);
        } break;
        default:
            break;
    }
}

// 主板温度
static void handle_board_temp(cJSON *item, const char *id, mqtt_type_e type)
{
    switch (type) {
        case MQTT_PRO_GET: {
            char get_buf[32];
            float temp = bsp_adc_read_temp();
            snprintf(get_buf, sizeof(get_buf), "{\"BoardTemp\":\"%.2f\"}", temp);
            app_mqtt_get_reply(mqtt_params.property_get_reply, id, get_buf);
        } break;
        default:
            break;
    }
}

// 主动上报主板温度
static void handle_board_temp_post(float temp)
{
    char post_buf[32];
    snprintf(post_buf, sizeof(post_buf), "{\"BoardTemp\":{\"value\":\"%.2f\"}}", temp);
    app_mqtt_post(mqtt_params.property_post, "000", post_buf);
}

// 服务类
static void handle_set_info(cJSON *item, const char *id, mqtt_type_e type)
{
#if 0
    if (type != MQTT_SERVICE) { // 不是服务类型,直接退出
        return;
    }

    char msg[512];
    strncpy(msg, item->valuestring, sizeof(msg) - 1);
    msg[sizeof(msg) - 1] = '\0';

    if (strncmp(msg, "SoftUpdate", strlen("SoftUpdate")) == 0) { // 检查更新

        g_report = false; // 检查并更新
        app_timer_start(500, app_network_http_task, true, NULL, "http_task");

    }
    // 设置场景数据
    else if (strncmp(msg, "SetScene", strlen("SetScene")) == 0) {
        const char *scene_param = msg + strlen("SetScene");
        app_public_scene_cfg_parse(scene_param);
    }
    // 绑定场景
    else if (strncmp(msg, "BindScene", strlen("BindScene")) == 0) {
        const char *bind_scene_param = msg + strlen("BindScene");
        app_public_bind_scene_cfg_parse(bind_scene_param);
    }
    // 绑定群组
    else if (strncmp(msg, "BindGroup", strlen("BindGroup")) == 0) {
        const char *bind_group_param = msg + strlen("BindGroup");
        app_public_bind_group_cfg_parse(bind_group_param);
    }
    // 执行场景
    else if (strncmp(msg, "ExeScene", strlen("ExeScene")) == 0) {
        const char *exe_sence_param = msg + strlen("ExeScene");
        app_public_exe_scene(exe_sence_param);
    }
    // 删除配置
    else if (strncmp(msg, "DelConfig", strlen("DelConfig")) == 0) {
        const char *del_cfg_param = msg + strlen("DelConfig");
        app_public_del_cfg(del_cfg_param);
    } // 设备重启
    else if (strncmp(msg, "DeviceReset", strlen("DeviceReset")) == 0) {
        app_mqtt_service_reply(mqtt_params.service_invoke_reply, id, NULL);
        delay_1ms(100);
        NVIC_SystemReset(); // 重启系统
    }
    app_mqtt_service_reply(mqtt_params.service_invoke_reply, id, NULL);

#endif
    if (type != MQTT_SERVICE || item == NULL || item->valuestring == NULL) {
        return;
    }
    const char *msg = item->valuestring;

    // 检查更新
    if (strncmp(msg, "SoftUpdate", strlen("SoftUpdate")) == 0) {
        g_report = false;
        app_timer_start(500, app_network_http_task, true, NULL, "http_task");
    }
    // 设置场景
    else if (strncmp(msg, "SetScene", strlen("SetScene")) == 0) {
        app_public_scene_cfg_parse(msg + strlen("SetScene"));
    }
    // 绑定场景
    else if (strncmp(msg, "BindScene", strlen("BindScene")) == 0) {
        app_public_bind_scene_cfg_parse(msg + strlen("BindScene"));
    }
    // 绑定群组
    else if (strncmp(msg, "BindGroup", strlen("BindGroup")) == 0) {
        app_public_bind_group_cfg_parse(msg + strlen("BindGroup"));
    }
    // 执行场景
    else if (strncmp(msg, "ExeScene", strlen("ExeScene")) == 0) {
        app_public_exe_scene(msg + strlen("ExeScene"));
    }
    // 删除配置
    else if (strncmp(msg, "DelConfig", strlen("DelConfig")) == 0) {
        app_public_del_cfg(msg + strlen("DelConfig"));
    }
    // 设备重启
    else if (strcmp(msg, "DeviceReset") == 0) {
        app_mqtt_service_reply(mqtt_params.service_invoke_reply, id, NULL);
        delay_1ms(100);
        NVIC_SystemReset();
        return;
    }
    app_mqtt_service_reply(mqtt_params.service_invoke_reply, id, NULL);
}

// 设置属性回复ACK
static void app_mqtt_set_reply(const char *topic, const char *id, const char *msg)
{
    if (!topic || !id) {
        return;
    }
    int ret;
    char replymsg[128]  = {0};
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
}

// 获取属性回复ACK
static void app_mqtt_get_reply(const char *topic, const char *id, const char *data)
{
    if (!topic || !id) {
        return;
    }

    int ret;
    char replymsg[256] = {0};

    pubmessage.qos      = QOS0;
    pubmessage.retained = 0;
    pubmessage.dup      = 0;

    snprintf(replymsg, sizeof(replymsg), "{\"id\":\"%s\",\"code\":%d,\"data\":%s}", id, 200, data);

    pubmessage.payload    = replymsg;
    pubmessage.payloadlen = strlen(replymsg);

    ret = MQTTPublish(&mqtt_client, topic, &pubmessage);

    if (ret != SUCCESSS) {
        APP_ERROR("MQTT reply publish failed: %s\n", topic);
    }
}

// 设备主动上报信息
static void app_mqtt_post(const char *topic, const char *id, const char *data)
{
    APP_PRINTF("app_mqtt_post\n");
    if (!topic || !id || !data)
        return;

    char msg[128] = {0};

    // OneNET 主动上报属性必须是 params,不要 code/data
    snprintf(msg, sizeof(msg), "{\"id\":\"%s\",\"version\":\"1.0\",\"params\":%s}", id, data);

    // 发布 MQTT
    pubmessage.qos        = QOS0;
    pubmessage.retained   = 0;
    pubmessage.dup        = 0;
    pubmessage.payload    = msg;
    pubmessage.payloadlen = strlen(msg);

    int ret = MQTTPublish(&mqtt_client, topic, &pubmessage);
    APP_PRINTF("app_mqtt_post msg: %s\n", msg);

    if (ret != SUCCESSS) {
        APP_ERROR("MQTT report publish failed: %s\n", topic);
    }
}

static void app_mqtt_service_reply(const char *topic, const char *id, char *data)
{
    if (!topic || !id) {
        return;
    }

    int ret;
    char replymsg[256] = {0};

    pubmessage.qos      = QOS0;
    pubmessage.retained = 0;
    pubmessage.dup      = 0;

    snprintf(replymsg, sizeof(replymsg), "{\"id\":\"%s\",\"code\":%d,\"data\":%s}", id, 200, (data ? data : "{}"));

    pubmessage.payload    = replymsg;
    pubmessage.payloadlen = strlen(replymsg);

    ret = MQTTPublish(&mqtt_client, topic, &pubmessage);

    if (ret != SUCCESSS) {
        APP_ERROR("MQTT service reply failed: %s\n", topic);
    }
}
