
#include "app_http_ota.h"
#include "systick.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../Source/w5500/ioLibrary_Driver/Ethernet/socket.h"

// 上报版本号
uint32_t ota_post_version(uint8_t *pkt, const char *pro_id, const char *dev_name, const char *auth, const char *s_version, const char *f_version)
{
    char body[128];

    sprintf(body, "{\"s_version\":\"%s\",\"f_version\":\"%s\"}", s_version, f_version);

    *pkt = 0;

    sprintf((char *)pkt, "POST /fuse-ota/%s/%s/version HTTP/1.1\r\n"
                         "Host: iot-api.heclouds.com\r\n"
                         "Content-Type: application/json\r\n"
                         "Authorization:%s\r\n"
                         "Content-Length:%d\r\n"
                         "\r\n"
                         "%s",
            pro_id,
            dev_name,
            auth,
            (int)strlen(body),
            body);

    return strlen((char *)pkt);
}

// 检测升级任务
uint32_t ota_get_update_task(uint8_t *pkt, uint32_t pkt_size, const char *pro_id, const char *dev_name, const char *type, const char *version, const char *auth)
{
    pkt[0] = '\0'; // 清空缓冲区首字节，初始化字符串

    snprintf((char *)pkt, pkt_size,
             "GET /fuse-ota/%s/%s/check?type=%s&version=%s HTTP/1.1\r\n"
             "Host: iot-api.heclouds.com\r\n"
             "Authorization:%s\r\n"
             "Connection: close\r\n"
             "\r\n",
             pro_id,
             dev_name,
             type,
             version,
             auth);

    return strlen((char *)pkt);
}

uint32_t g_tid = 0;
void handle_http_data(uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        printf("%c", data[i]); // 逐个字符打印
    }
    printf("\r\n"); // 打印完一段数据后换行
}

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
                    printf("Receive response:\r\n");
                    while (len > 0) {
                        len = recv(sn, buf, len);
                        handle_http_data(buf, len);
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
                    printf("Receive response:\r\n");
                    while (len > 0) {
                        len = recv(sn, buf, len);
                        for (uint16_t i = 0; i < len; i++) {
                            printf("%c", *(buf + i));
                        }
                        len = getSn_RX_RSR(sn);
                    }
                    printf("\r\n");
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

int http_recv_response(uint8_t sn, uint8_t *buf, uint16_t buf_len)
{
    static uint16_t recv_timeout[8] = {0};
    uint16_t len;

    switch (getSn_SR(sn)) {
        case SOCK_ESTABLISHED:
        case SOCK_CLOSE_WAIT:
            len = getSn_RX_RSR(sn);
            if (len > 0) {
                if (len > buf_len) len = buf_len; // 防止越界
                len = recv(sn, buf, len);
                printf("HTTP response:\r\n");
                for (uint16_t i = 0; i < len; i++) {
                    printf("%c", buf[i]);
                }
                printf("\r\n");
                recv_timeout[sn] = 0; // 收到数据重置超时计数
                return 1;             // 有数据返回 1
            } else {
                recv_timeout[sn]++;
                delay_1ms(100);               // 可以根据需要调整
                if (recv_timeout[sn] > 100) { // 超时
                    printf("HTTP receive timeout!\r\n");
                    disconnect(sn);
                    close(sn);
                    recv_timeout[sn] = 0;
                    return -1; // 超时
                }
            }
            break;

        case SOCK_CLOSED:
            close(sn);
            return -1;

        default:
            break;
    }
    return 0; // 暂无数据
}