
#include "app_http_ota.h"
#include "app_network_info.h"
#include "systick.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../Source/w5500/ioLibrary_Driver/Ethernet/socket.h"
#include "../Source/app/app_timer/app_timer.h"
#include "../Source/bsp./bsp_usart/bsp_usart.h"
#include "../Source/bsp/bsp_flash/bsp_flash.h"
#include "../Source/app/app_md5/app_md5.h"
#include "../Source/cjson/cJSON.h"

// 函数声明
static uint16_t app_ota_post_version(uint8_t *buf, uint16_t buf_size, const char *s_version, const char *f_version);
static uint16_t app_ota_get_update_task(uint8_t *buf, uint16_t buf_size, const char *type, const char *version);
static uint16_t app_ota_get_bin(uint8_t *buf, uint16_t buf_size, uint32_t tid, const char *range);

static void app_parse_http_data(uint8_t *data, uint16_t len);
static void app_parse_http_bin(uint8_t *data, uint16_t len);

static void app_ota_start(void);
static void timer_do_ota(void *arg);

// 全局变量
static uint8_t *http_buf = NULL;
extern uint8_t ethernet_buf[];

static uint8_t ota_flag;
static ota_info_t my_ota_info;

static uint8_t *server_name;
static uint8_t *server_ip;
static uint8_t server_port;

int app_ota_check(uint8_t sn, uint8_t *name, uint8_t *ip, uint8_t port, uint8_t *shared_buf)
{
    MD5_Test();
    server_name = name;
    server_ip   = ip;
    server_port = port;
    http_buf    = shared_buf;
    app_timer_start(500, timer_do_ota, true, NULL, "do_ota");
    return 0;
}

// http 发送
uint8_t app_http_send_packet(uint8_t sn, uint8_t *buf, uint16_t len, uint8_t *destip, uint16_t destport)
{
    uint8_t sr = getSn_SR(sn);
    switch (sr) {
        case SOCK_CLOSED:
            socket(sn, Sn_MR_TCP, 50000, 0x00);
            return 0;
        case SOCK_INIT:
            connect(sn, destip, destport);
            return 0;
        case SOCK_ESTABLISHED:
            send(sn, buf, len);
            return 1; // 发送完成
        default:
            return 0;
    }
}

// http 接收
uint8_t app_http_recv_packet(uint8_t sn, uint8_t *buf)
{
    uint16_t len = getSn_RX_RSR(sn);
    if (len > 0) {
        len = recv(sn, buf, len);
        app_parse_http_data(buf, len);
        disconnect(sn);
        close(sn);
        return 1;
    }
    // 如果服务器主动断开了连接
    if (getSn_SR(sn) == SOCK_CLOSE_WAIT) {
        APP_PRINTF("SOCK_CLOSE_WAIT\n");
        close(sn);
    }
    return 0;
}

// 专门用于接收固件流的函数
uint8_t app_http_recv_bin_packet(uint8_t sn, uint8_t *buf)
{
    static uint16_t total_len = 0; // 静态变量,多次调用累加长度

    uint16_t rsr_len = getSn_RX_RSR(sn);

    // 1. 只要缓存有数据,就追加到 buf
    if (rsr_len > 0) {
        total_len += recv(sn, buf + total_len, rsr_len);
    }

    // 2. 只有当服务器发完所有数据（Connection: close 触发断开信号）
    if (getSn_SR(sn) == SOCK_CLOSE_WAIT) {
        if (total_len > 0) {
            app_parse_http_bin(buf, total_len); // 收齐了,一次性处理
        }

        total_len = 0; // 重置计数器,给下一次 Range 请求用
        disconnect(sn);
        close(sn);
        return 1; // 真正完成一包下载
    }

    return 0; // 数据可能还在路上，返回 0 让定时器下次再来看
}

static void app_parse_http_bin(uint8_t *data, uint16_t len)
{
    char *header_end = strstr((char *)data, "\r\n\r\n");

    if (header_end == NULL) {
        APP_PRINTF("Error: Invalid HTTP response (no header end found)\n");
        return;
    }

    if (strstr((char *)data, "HTTP/1.1 206") == NULL && strstr((char *)data, "HTTP/1.1 200") == NULL) {
        APP_PRINTF("Error: Server returned error code!\n");
        return;
    }

    uint8_t *bin_start = (uint8_t *)(header_end + 4);
    uint16_t bin_len   = len - (bin_start - data);

    if (bin_len > 0) {

        uint32_t target_addr  = FLASH_OTA_SADDR + (my_ota_info.current_packet * OTA_PACKET_SIZE);  // 计算目标地址
        fmc_state_enum status = app_flash_write_page(target_addr, (uint32_t *)bin_start, bin_len); // 整页写入flash

        if (status == FMC_READY) {
            my_ota_info.current_packet++;
            APP_PRINTF("page write OK!\n");

        } else {
            APP_PRINTF("page write error: %d\n", status);
        }
    } else {
        APP_PRINTF("Warning: Header found but Body is empty!\n");
    }
}

static void timer_do_ota(void *arg)
{
    static uint16_t send_len = 0;

    switch (ota_flag) {
        case 0: { // 上报固件版本
            send_len = app_ota_post_version(http_buf, ETHERNET_BUF_SIZE, "V1.1", "V1.0");
            app_http_send_packet(HTTP_SOCKET_ID, http_buf, send_len, server_ip, server_port);
            if (app_http_recv_packet(HTTP_SOCKET_ID, http_buf)) {
                ota_flag++;
            }
        } break;
        case 1: { // 获取升级任务
            send_len = app_ota_get_update_task(http_buf, ETHERNET_BUF_SIZE, "2", "1.1");
            app_http_send_packet(HTTP_SOCKET_ID, http_buf, send_len, server_ip, server_port);
            if (app_http_recv_packet(HTTP_SOCKET_ID, http_buf)) {
                ota_flag++;
            }
        } break;
        case 2: {
            app_timer_stop("do_ota");
            return;
        }
    }
}

static void timer_do_bin(void *arg)
{
    static uint8_t download_step = 0; // 0:发送请求, 1:等待接收
    uint16_t send_len;
    char range_str[32];

    if (my_ota_info.current_packet >= my_ota_info.total_packets) {
        APP_PRINTF(">>> Download Success! All %d packets saved.\n", my_ota_info.total_packets);

        uint8_t md5_result[16];

        Calculate_File_MD5(FLASH_OTA_SADDR, my_ota_info.size, md5_result);
        APP_PRINTF("my_ota_info.size:%d\n", my_ota_info.size);

        APP_PRINTF_BUF("md5_result", md5_result, sizeof(md5_result));
        app_timer_stop("do_bin");
        return;
    }

    if (download_step == 0) {
        uint32_t start_byte = my_ota_info.current_packet * OTA_BLOCK_SIZE;
        uint32_t end_byte   = start_byte + OTA_BLOCK_SIZE - 1;
        if (end_byte >= my_ota_info.size) end_byte = my_ota_info.size - 1;

        snprintf(range_str, sizeof(range_str), "%u-%u", (unsigned int)start_byte, (unsigned int)end_byte);
        // APP_PRINTF("%s\n", range_str);
        send_len = app_ota_get_bin(http_buf, ETHERNET_BUF_SIZE, my_ota_info.tid, range_str);

        if (app_http_send_packet(HTTP_SOCKET_ID, http_buf, send_len, server_ip, server_port) == 1) {
            download_step = 1; // 切换到等待接收状态
        }
    } else if (download_step == 1) {
        if (app_http_recv_bin_packet(HTTP_SOCKET_ID, http_buf)) {
            download_step = 0; // 准备发送下一包
        }

        // (可选) 这里可以加一个简单的超时计数，如果 10 秒没收到，强制 download_step = 0 重试
    }
}

// 解析普通 http 数据
static void app_parse_http_data(uint8_t *data, uint16_t len)
{
    char *json_start = strstr((char *)data, "\r\n\r\n");
    if (json_start == NULL) return;
    json_start += 4;

    cJSON *root = cJSON_Parse(json_start);
    if (root == NULL) {
        APP_PRINTF("JSON Parse Error!\n");
        return;
    }

    APP_PRINTF("\n");
    cJSON *code   = cJSON_GetObjectItem(root, "code");
    cJSON *msg    = cJSON_GetObjectItem(root, "msg");
    cJSON *req_id = cJSON_GetObjectItem(root, "request_id");

    if (code) APP_PRINTF("%-10s: %d\n", "code", code->valueint);
    if (msg) APP_PRINTF("%-10s: %s\n", "msg", msg->valuestring);
    if (req_id) APP_PRINTF("%-10s: %s\n", "request_id", req_id->valuestring);

    cJSON *item_data = cJSON_GetObjectItem(root, "data");
    if (item_data) {

        cJSON *target = cJSON_GetObjectItem(item_data, "target");
        cJSON *tid    = cJSON_GetObjectItem(item_data, "tid");
        cJSON *size   = cJSON_GetObjectItem(item_data, "size");
        cJSON *md5    = cJSON_GetObjectItem(item_data, "md5");
        cJSON *status = cJSON_GetObjectItem(item_data, "status");
        cJSON *type   = cJSON_GetObjectItem(item_data, "type");

        if (target && target->valuestring) {
            strncpy(my_ota_info.target, target->valuestring, sizeof(my_ota_info.target) - 1);
            my_ota_info.target[sizeof(my_ota_info.target) - 1] = '\0';
        }
        if (md5 && md5->valuestring) {
            strncpy(my_ota_info.md5, md5->valuestring, sizeof(my_ota_info.md5) - 1);
            my_ota_info.md5[sizeof(my_ota_info.md5) - 1] = '\0';
        }
        if (tid) my_ota_info.tid = (uint32_t)tid->valueint;
        if (size) my_ota_info.size = (uint32_t)size->valueint;
        if (status) my_ota_info.status = (uint8_t)status->valueint;
        if (type) my_ota_info.type = (uint8_t)type->valueint;

        APP_PRINTF("%-10s: %s\r\n", "target", my_ota_info.target);
        APP_PRINTF("%-10s: %u\r\n", "tid", (unsigned int)my_ota_info.tid);
        APP_PRINTF("%-10s: %u\r\n", "size", (unsigned int)my_ota_info.size);
        APP_PRINTF("%-10s: %s\r\n", "md5", my_ota_info.md5);
        APP_PRINTF("%-10s: %d\r\n", "status", my_ota_info.status);
        APP_PRINTF("%-10s: %d\r\n", "type", my_ota_info.type);

        if (my_ota_info.size > 0) {
            my_ota_info.total_packets  = (my_ota_info.size + OTA_BLOCK_SIZE - 1) / OTA_BLOCK_SIZE;
            my_ota_info.current_packet = 0;
        } else {
            my_ota_info.total_packets = 0;
        }
        app_ota_start();
    }
    APP_PRINTF("\n");
    cJSON_Delete(root);
}

static void app_ota_start(void)
{
    my_ota_info.current_packet = 0;
    my_ota_info.total_packets  = (my_ota_info.size + OTA_BLOCK_SIZE - 1) / OTA_BLOCK_SIZE;

    APP_PRINTF(">>> Start Download Timer: Total %d packets\n", my_ota_info.total_packets);

    app_timer_start(100, timer_do_bin, true, NULL, "do_bin");
}

// 上报版本号
static uint16_t app_ota_post_version(uint8_t *buf, uint16_t buf_size, const char *s_version, const char *f_version)
{
    int len = snprintf(NULL, 0, "{\"s_version\":\"%s\",\"f_version\":\"%s\"}", s_version, f_version);

    snprintf((char *)buf, buf_size,
             "POST /fuse-ota/%s/%s/version HTTP/1.1\r\n"
             "Host: iot-api.heclouds.com\r\n"
             "Content-Type: application/json\r\n"
             "Authorization:%s\r\n"
             "Content-Length:%d\r\n"
             "Connection: close\r\n"
             "\r\n"
             "{\"s_version\":\"%s\",\"f_version\":\"%s\"}",
             PRODUCTS, DEVICES, PASSWD, len, s_version, f_version);

    return (uint16_t)strlen((char *)buf);
}

// 检测升级任务
static uint16_t app_ota_get_update_task(uint8_t *buf, uint16_t buf_size, const char *type, const char *version)
{
    int len = snprintf((char *)buf, buf_size,
                       "GET /fuse-ota/%s/%s/check?type=%s&version=%s HTTP/1.1\r\n"
                       "Host: iot-api.heclouds.com\r\n"
                       "Authorization: %s\r\n"
                       "Connection: close\r\n"
                       "\r\n",
                       PRODUCTS, DEVICES, type, version, PASSWD);

    return (uint16_t)len;
}

// 下载固件包
static uint16_t app_ota_get_bin(uint8_t *buf, uint16_t buf_size, uint32_t tid, const char *range)
{
    int len;
    if (range && range[0] != '\0') { // 分包下载
        len = snprintf((char *)buf, buf_size,
                       "GET /fuse-ota/%s/%s/%u/download HTTP/1.1\r\n"
                       "Host: iot-api.heclouds.com\r\n"
                       "Authorization: %s\r\n"
                       "Range: bytes=%s\r\n"
                       "Connection: close\r\n"
                       "\r\n",
                       PRODUCTS, DEVICES, tid, PASSWD, range);
    } else { // 整包下载
        len = snprintf((char *)buf, buf_size,
                       "GET /fuse-ota/%s/%s/%u/download HTTP/1.1\r\n"
                       "Host: iot-api.heclouds.com\r\n"
                       "Authorization: %s\r\n"
                       "Connection: close\r\n"
                       "\r\n",
                       PRODUCTS, DEVICES, tid, PASSWD);
    }
    return (uint16_t)len;
}

#if 0
uint32_t ota_get_download(uint8_t *pkt, uint32_t pkt_size, const char *pro_id, const char *dev_name, uint32_t tid, const char *auth, const char *range)
{
    pkt[0] = '\0'; // 清空缓冲区首字节

    if (range && range[0] != '\0') {
        // 带 Range
        snprintf((char *)pkt, pkt_size,
                 "GET /fuse-ota/%s/%s/%u/download HTTP/1.1\r\n"
                 "Host: iot-api.heclouds.com\r\n"
                 "Authorization: %s\r\n"
                 "Range: %s\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 pro_id, dev_name, tid, auth, range);
    } else {
        // 不带 Range
        snprintf((char *)pkt, pkt_size,
                 "GET /fuse-ota/%s/%s/%u/download HTTP/1.1\r\n"
                 "Host: iot-api.heclouds.com\r\n"
                 "Authorization: %s\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 pro_id, dev_name, tid, auth);
    }
    return strlen((char *)pkt);
}
#endif
#if 0 
uint8_t do_http_request(uint8_t sn, uint8_t *buf, uint16_t len, uint8_t *destip, uint16_t destport)
{
    uint16_t local_port   = 50000; // 本地端口号
    uint16_t recv_timeout = 0;     // 接收超时计数
    uint8_t send_flag     = 0;     // 请求是否发送标志

    while (1) {
        switch (getSn_SR(sn)) { // 获取套接字当前状态

            case SOCK_INIT: // 套接字已初始化,但未连接
                connect(sn, destip, destport);
                break;
            case SOCK_ESTABLISHED: // TCP 已建立连接
                if (send_flag == 0) {
                    send(sn, buf, len); // 发送 http 请求
                    send_flag = 1;
                }
                len = getSn_RX_RSR(sn); // 获取接收缓冲区的数据长度
                if (len > 0) {
                    // printf("Receive response:\r\n");
                    while (len > 0) {
                        len = recv(sn, buf, len);
                        app_parse_http_data(buf, len);
                        len = getSn_RX_RSR(sn); // 再次查看缓冲区里有多少字节
                    }
                    disconnect(sn);
                    close(sn);
                    return 1;
                } else {
                    recv_timeout++;
                    delay_1ms(1000);
                }
                // timeout handling
                if (recv_timeout > 10) {
                    printf("request fail!\r\n");
                    disconnect(sn);
                    close(sn);
                    return 0;
                }
                break;
            case SOCK_CLOSE_WAIT: // 服务器请求关闭连接(可能有错误响应)

                len = getSn_RX_RSR(sn);
                if (len > 0) {
                    // printf("Receive response:\r\n");
                    while (len > 0) {
                        len = recv(sn, buf, len);
                        app_parse_http_data(buf, len);
                        len = getSn_RX_RSR(sn);
                    }
                    // printf("\r\n");
                    disconnect(sn);
                    close(sn);
                    return 1;
                }
                close(sn);
                break;
            case SOCK_CLOSED:                            // 套接字关闭状态
                close(sn);                               // 确保套接字关闭
                socket(sn, Sn_MR_TCP, local_port, 0x00); // 打开套接字并准备简历 TCP 连接
                break;
            default:
                break;
        }
    }
}
#endif