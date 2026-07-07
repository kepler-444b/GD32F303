

#include "app_http_ota.h"
#include "systick.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../Source/w5500/ioLibrary_Driver/Ethernet/socket.h"
#include "../Source/app/app_timer/app_timer.h"
#include "../Source/bsp./bsp_usart/bsp_usart.h"
#include "../Source/bsp/bsp_flash/bsp_flash.h"
#include "../Source/bsp/bsp_board/bsp_board.h"
#include "../Source/app/app_md5/app_md5.h"
#include "../Source/cjson/cJSON.h"
#include "../Source/dev/dev_info.h"

// 函数声明
static uint16_t app_ota_post_version(uint8_t *buf, uint16_t buf_size, const char *s_version, const char *f_version);
static uint16_t app_ota_get_update_task(uint8_t *buf, uint16_t buf_size, const char *type, const char *version);
static uint16_t app_ota_get_bin(uint8_t *buf, uint16_t buf_size, uint32_t tid, const char *range);
static uint16_t app_ota_post_status(uint8_t *buf, uint16_t buf_size, uint32_t tid, uint8_t status_code);

static bool app_parse_http_data(uint8_t *data, uint16_t len);
static bool app_parse_http_bin(uint8_t *data, uint16_t len);

static void timer_do_ota(void *arg);
static void timer_timeout(void *arg);

// 全局变量
static uint8_t *http_buf = NULL;
static uint8_t ota_flag;
static ota_info_t my_ota_info;
static httpconn my_httpconn;
static dev_save_info_t my_dev;
static bool is_report = true; // 是否是上报软件版本

static uint8_t download_step = 0; // 静态变量,接收固件包步骤
static uint16_t total_len    = 0; // 静态变量,多次调用累加长度

// 检查更新
void app_ota_check(httpconn *http_params, dev_save_info_t *dev, uint8_t *shared_buf, bool report)
{
    APP_PRINTF("================= [app_ota_check] =================\n");
    if (ota_flag != OTA_IDLE) {
        APP_PRINTF("ota_flag:%d\n", ota_flag);
        APP_ERROR("ota_flag is not OTA_IDLE!");
        return;
    }
    is_report = report;
    http_buf  = shared_buf;
    memcpy(&my_dev, dev, sizeof(my_dev));
    memcpy(&my_httpconn, http_params, sizeof(my_httpconn));

    ota_flag = OTA_REPORT_VER;
    app_timer_start(100, timer_do_ota, true, NULL, "do_ota");
}

// http 发送
bool app_http_send_packet(uint8_t sn, uint8_t *buf, uint16_t len, uint8_t *destip, uint16_t destport)
{
    uint8_t sr = getSn_SR(sn);
    switch (sr) {
        case SOCK_CLOSED:
            socket(sn, Sn_MR_TCP, 50000, 0x00);
            return false;
        case SOCK_INIT:
            connect(sn, destip, destport);
            return false;
        case SOCK_ESTABLISHED:
            send(sn, buf, len);
            return true; // 发送完成
        default:
            return false;
    }
}

// http 接收
bool app_http_recv_packet(uint8_t sn, uint8_t *buf)
{
    bool ret = false;

    uint16_t len = getSn_RX_RSR(sn);
    if (len > 0) {
        len = recv(sn, buf, len);
        ret = app_parse_http_data(buf, len);
        disconnect(sn);
        close(sn);
        return ret;
    }
    // 如果服务器主动断开了连接
    if (getSn_SR(sn) == SOCK_CLOSE_WAIT) {
        APP_PRINTF("SOCK_CLOSE_WAIT\n");
        close(sn);
    }
    return ret;
}

// 专门用于接收固件流的函数
bool app_http_recv_bin_packet(uint8_t sn, uint8_t *buf)
{
    bool ret = false;

    uint16_t rsr_len = getSn_RX_RSR(sn);

    if (rsr_len > 0) {
        total_len += recv(sn, buf + total_len, rsr_len);
    }

    if (getSn_SR(sn) == SOCK_CLOSE_WAIT) {
        if (total_len > 0) {
            ret = app_parse_http_bin(buf, total_len); // 收齐了,一次性处理
        }
        total_len = 0; // 重置计数器,给下一次 Range 请求用
        disconnect(sn);
        close(sn);
        return ret; // 真正完成一包下载
    }

    return ret; // 数据可能还在路上,返回 0 让定时器下次再来看
}

// 解析收到的固件流的函数
static bool app_parse_http_bin(uint8_t *data, uint16_t len)
{
    char *header_end = strstr((char *)data, "\r\n\r\n");

    if (header_end == NULL) {
        APP_PRINTF("Error: Invalid HTTP response (no header end found)\n");
        return false;
    }

    if (strstr((char *)data, "HTTP/1.1 206") == NULL && strstr((char *)data, "HTTP/1.1 200") == NULL) {
        APP_PRINTF("Error: Server returned error code!\n");
        return false;
    }

    uint8_t *bin_start = (uint8_t *)(header_end + 4);
    uint16_t bin_len   = len - (bin_start - data);

    if (bin_len > 0) {
        uint32_t target_addr  = FLASH_OTA_SADDR + (my_ota_info.current_packet * OTA_PACKET_SIZE);  // 计算目标地址
        fmc_state_enum status = app_flash_write_page(target_addr, (uint32_t *)bin_start, bin_len); // 整页写入flash

        if (status == FMC_READY) {
            my_ota_info.current_packet++;
            APP_PRINTF("step[%d/%d] page write OK!\n", my_ota_info.current_packet, my_ota_info.total_packets);
            return true;
        } else {
            APP_PRINTF("page write error: %d\n", status);
            return false;
        }
    }

    APP_PRINTF("Warning: Header found but Body is empty!\n");
    return false;
}

// 统一 OTA 核心状态机
static void timer_do_ota(void *arg)
{
    static uint16_t send_len         = 0;
    const dev_save_info_t *temp_info = dev_get_save_device_info();

    switch (ota_flag) {
        case OTA_REPORT_VER: { // 上报固件版本
            send_len = app_ota_post_version(http_buf, ETHERNET_BUF_SIZE, temp_info->cur_ver, "V1.1");
            app_http_send_packet(my_httpconn.sn, http_buf, send_len, my_httpconn.server_ip, my_httpconn.port);
            app_http_recv_packet(my_httpconn.sn, http_buf);
        } break;
        case OTA_GET_TASK: { // 获取软件更新
            send_len = app_ota_get_update_task(http_buf, ETHERNET_BUF_SIZE, "2", temp_info->cur_ver);
            app_http_send_packet(my_httpconn.sn, http_buf, send_len, my_httpconn.server_ip, my_httpconn.port);
            app_http_recv_packet(my_httpconn.sn, http_buf);
        } break;

        case OTA_DOWNLOADING: {
            // 校验是否全部下载完成
            if (my_ota_info.current_packet >= my_ota_info.total_packets) {
                APP_PRINTF("Download Success! All %d packets saved\n", my_ota_info.total_packets);
                char cur_file_md5[32];
                app_md5_calculate_file(FLASH_OTA_SADDR, my_ota_info.size, cur_file_md5);

                if (memcmp(cur_file_md5, my_ota_info.md5, sizeof(cur_file_md5)) == 0) {
                    APP_PRINTF("MD5 check passed\n");
                    ota_flag = OTA_REPORT_OK;
                } else {
                    APP_PRINTF("MD5 check failed\n");
                    ota_flag = OTA_REPORT_CHECK_FAIL;
                }
                break;
            }

            // 固件块分片请求与接收子状态机
            if (download_step == 0) {
                total_len = 0;

                uint32_t start_byte = my_ota_info.current_packet * OTA_PACKET_SIZE;
                uint32_t end_byte   = start_byte + OTA_PACKET_SIZE - 1;
                if (end_byte >= my_ota_info.size) end_byte = my_ota_info.size - 1;

                char range_str[32];
                snprintf(range_str, sizeof(range_str), "%u-%u", (unsigned int)start_byte, (unsigned int)end_byte);
                send_len = app_ota_get_bin(http_buf, ETHERNET_BUF_SIZE, my_ota_info.tid, range_str);
                if (app_http_send_packet(my_httpconn.sn, http_buf, send_len, my_httpconn.server_ip, my_httpconn.port) == 1) {
                    download_step = 1;
                }
            } else if (download_step == 1) {
                if (app_http_recv_bin_packet(my_httpconn.sn, http_buf)) {
                    download_step = 0;
                }
            }
            break;
        }
        case OTA_REPORT_OK: { // 上报升级成功
            send_len = app_ota_post_status(http_buf, ETHERNET_BUF_SIZE, my_ota_info.tid, 201);
            app_http_send_packet(my_httpconn.sn, http_buf, send_len, my_httpconn.server_ip, my_httpconn.port);

            if (app_http_recv_packet(my_httpconn.sn, http_buf)) {
                app_timer_stop("do_ota");
                ota_flag = OTA_IDLE;

                // 更新固件版本
                dev_save_info_t temp_info = *dev_get_save_device_info();
                snprintf(temp_info.md5, sizeof(temp_info.md5), "%s", my_ota_info.md5);            // 保存MD5
                snprintf(temp_info.cur_ver, sizeof(temp_info.cur_ver), "%s", my_ota_info.target); // 保存软件版本
                temp_info.file_size = my_ota_info.size;
                temp_info.ota_flag  = OTA_SET_FLAG;

                bool ret = dev_set_save_device_info(&temp_info); // 设置ota标志位
                if (ret) {
                    APP_PRINTF("set device info success!\n");
                    delay_1ms(100);
                    NVIC_SystemReset(); // 重启系统
                } else {
                    APP_PRINTF("set device info error!\n");
                }
            }
        } break;
        case OTA_REPORT_CHECK_FAIL: { // 上报 MD5 校验失败
            send_len = app_ota_post_status(http_buf, ETHERNET_BUF_SIZE, my_ota_info.tid, 205);
            app_http_send_packet(my_httpconn.sn, http_buf, send_len, my_httpconn.server_ip, my_httpconn.port);
            if (app_http_recv_packet(my_httpconn.sn, http_buf)) {
                app_timer_stop("do_ota");
                ota_flag = OTA_IDLE;
            }
        } break;

        case OTA_IDLE: { // 空闲状态
            return;
        } break;
        default:
            break;
    }
}

static void timer_timeout(void *arg)
{
    APP_PRINTF("OTA timer_timeout\n");
    delay_1ms(100);
    NVIC_SystemReset(); // 重启系统
}

// 解析普通 http 数据
static bool app_parse_http_data(uint8_t *data, uint16_t len)
{
    char *json_start = strstr((char *)data, "\r\n\r\n");
    if (json_start == NULL) {
        return false;
    }
    json_start += 4;

    cJSON *root = cJSON_Parse(json_start);
    if (root == NULL) {
        APP_PRINTF("JSON Parse Error!\n");
        return false;
    }

    cJSON *code   = cJSON_GetObjectItem(root, "code");
    cJSON *msg    = cJSON_GetObjectItem(root, "msg");
    cJSON *req_id = cJSON_GetObjectItem(root, "request_id");

    if (!cJSON_IsNumber(code)) {
        APP_PRINTF("code invalid\n");
        cJSON_Delete(root);
        return false;
    }

    if (ota_flag == OTA_REPORT_VER) {
        if (is_report) {
            ota_flag = OTA_IDLE;
            app_timer_stop("do_ota");
            APP_PRINTF("OTA report ver, no task\n");
        } else {
            ota_flag = OTA_GET_TASK;
            APP_PRINTF("OTA report ver, have task\n");
        }
    }

    if (ota_flag == OTA_GET_TASK) { // 接收升级任务

        if (code->valueint != 0) {
            if (code->valueint == not_exist) {
                APP_PRINTF("NOT EXIST\n"); // not exist 任务不存在
                ota_flag = OTA_IDLE;
                app_timer_stop("do_ota");
            }
        } else {
            cJSON *item_data = cJSON_GetObjectItem(root, "data");

            if (item_data) { // 有 OTA 任务
                cJSON *target = cJSON_GetObjectItem(item_data, "target");
                cJSON *tid    = cJSON_GetObjectItem(item_data, "tid");
                cJSON *size   = cJSON_GetObjectItem(item_data, "size");
                cJSON *md5    = cJSON_GetObjectItem(item_data, "md5");
                cJSON *status = cJSON_GetObjectItem(item_data, "status");
                cJSON *type   = cJSON_GetObjectItem(item_data, "type");

                snprintf(my_ota_info.target, sizeof(my_ota_info.target), "%s", target->valuestring);
                snprintf(my_ota_info.md5, sizeof(my_ota_info.md5), "%s", md5->valuestring);
                my_ota_info.tid    = (uint32_t)tid->valueint;
                my_ota_info.size   = (uint32_t)size->valueint;
                my_ota_info.status = (uint8_t)status->valueint;
                my_ota_info.type   = (uint8_t)type->valueint;

                APP_PRINTF("%-10s: %s\r\n", "target", my_ota_info.target);
                APP_PRINTF("%-10s: %u\r\n", "tid", (unsigned int)my_ota_info.tid);
                APP_PRINTF("%-10s: %u\r\n", "size", (unsigned int)my_ota_info.size);
                APP_PRINTF("%-10s: %s\r\n", "md5", my_ota_info.md5);
                APP_PRINTF("%-10s: %d\r\n", "status", my_ota_info.status);
                APP_PRINTF("%-10s: %d\r\n", "type", my_ota_info.type);

                my_ota_info.total_packets  = (my_ota_info.size + OTA_PACKET_SIZE - 1) / OTA_PACKET_SIZE;
                my_ota_info.current_packet = 0;

                ota_flag = OTA_DOWNLOADING;
                app_timer_start(60000, timer_timeout, false, NULL, "timeout");
                bsp_set_buuzzer(3);
            }
        }
    }
    cJSON_Delete(root);
    return true;
}

// 上报设备版本号
// f_version(模组版本号,固定为 V1.1)
// s_version(mcu固件版本号)
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
             my_dev.products, my_dev.devices, my_dev.passwd, len, s_version, f_version);

    // APP_PRINTF("report:%s\n", buf);
    return (uint16_t)strlen((char *)buf);
}

// 检测升级任务
// type(1:fota(对模组进行升级),2:sota(对mcu进行升级))
// version(当前设备版本)
static uint16_t app_ota_get_update_task(uint8_t *buf, uint16_t buf_size, const char *type, const char *version)
{
    int len = snprintf((char *)buf, buf_size,
                       "GET /fuse-ota/%s/%s/check?type=%s&version=%s HTTP/1.1\r\n"
                       "Host: iot-api.heclouds.com\r\n"
                       "Authorization: %s\r\n"
                       "Connection: close\r\n"
                       "\r\n",
                       my_dev.products, my_dev.devices, type, version, my_dev.passwd);

    return (uint16_t)len;
}

// 下载升级包
// tid 任务id(由app_ota_get_update_task获得)
// range 分片下载字段
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
                       my_dev.products, my_dev.devices, tid, my_dev.passwd, range);
    } else { // 整包下载
        len = snprintf((char *)buf, buf_size,
                       "GET /fuse-ota/%s/%s/%u/download HTTP/1.1\r\n"
                       "Host: iot-api.heclouds.com\r\n"
                       "Authorization: %s\r\n"
                       "Connection: close\r\n"
                       "\r\n",
                       my_dev.products, my_dev.devices, tid, my_dev.passwd);
    }
    return (uint16_t)len;
}

// 上报升级状态
// tid 任务id(由app_ota_get_update_task获得)
// status_code 状态码(参考文档:https://open.iot.10086.cn/doc/aiot/fuse/detail/1449)
static uint16_t app_ota_post_status(uint8_t *buf, uint16_t buf_size, uint32_t tid, uint8_t status_code)
{
    int len = snprintf(NULL, 0, "{\"step\":%d}", status_code);

    snprintf((char *)buf, buf_size,
             "POST /fuse-ota/%s/%s/%u/status HTTP/1.1\r\n"
             "Host: iot-api.heclouds.com\r\n"
             "Content-Type: application/json\r\n"
             "Authorization: %s\r\n"
             "Content-Length:%d\r\n"
             "Connection: close\r\n"
             "\r\n"
             "{\"step\":%d}",
             my_dev.products, my_dev.devices, tid, my_dev.passwd, len, status_code);

    return (uint16_t)strlen((char *)buf);
}
