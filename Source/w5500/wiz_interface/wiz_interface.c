#include "wiz_interface.h"
#include "../ioLibrary_Driver/Ethernet/wizchip_conf.h"
#include "../ioLibrary_Driver/Internet/DHCP/dhcp.h"
#include "../wiz_platform/wiz_platform.h"
#include "../Source/app/app_timer/app_timer.h"
#include "../Source/app/app_evenbus/app_eventbus.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"
#include "../Source/dev/dev_info.h"
#include "systick.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W5500_VERSION 0x04

// PHY状态结构体(用于网线插拔检测)
typedef struct
{
    bool cur_status;     // 网线当前状态
    bool last_status;    // 网线上次状态
    uint8_t retry_count; // 重试次数
} phy_status_t;

// 全局变量
static phy_status_t my_phy_status = {0};
static uint8_t dhcp_init_flag     = 0;
static wiz_NetInfo conf_info;

// 函数声明
static void wizinit_timer(void *arg);
static void print_network_information(wiz_NetInfo *conf_info);
static void wiz_print_phy_info(void);
static void wizchip_version_check(void);
static void wiz_dhcp_task(void);

// W5500初始化流程
void wizchip_initialize(void)
{
    APP_PRINTF("[wizchip_init] ===================================\n");
    wizchip_spi_cb_reg();    // 注册 W5500 芯片回调函数
    wizchip_reset();         // W5500 芯片复位
    wizchip_version_check(); // 检测 W5500 芯片版本

    app_timer_start(1000, wizinit_timer, true, NULL, "wizinit"); // 周期检测网线插口
}

// 检查W5500芯片版本是否正确
static void wizchip_version_check(void)
{
    uint8_t error_count = 0;
    uint8_t version     = 0;

    while (1) {
        delay_1ms(100);
        version = getVERSIONR();
        if (version == W5500_VERSION) { // 读到正确版本
            APP_PRINTF("W5500 VERSION OK: 0X%02X\r\n", version);
            return; // 成功
        }
        error_count++; // 版本错误
        APP_ERROR("W5500 version mismatch: read=0x%02X, expect=0x%02X, retry=%d\r\n", version, W5500_VERSION, error_count);
        if (error_count >= 5) { // 5 次失败,则退出
            APP_ERROR("W5500 init failed (SPI or hardware error)\r\n");
            return; // 失败退出
        }
    }
}

// 打印PHY物理层信息(速率/双工模式)
static void wiz_print_phy_info(void)
{
    uint8_t get_phy_conf;
    get_phy_conf = getPHYCFGR();
    APP_PRINTF("The current Mbtis speed : %dMbps\r\n", get_phy_conf & 0x02 ? 100 : 10);
    APP_PRINTF("The current Duplex Mode : %s\r\n", get_phy_conf & 0x04 ? "Full-Duplex" : "Half-Duplex");
}

// wizinit_timer 定时回调函数
static void wizinit_timer(void *arg)
{
    ctlwizchip(CW_GET_PHYLINK, (void *)&my_phy_status.cur_status);
    if ((my_phy_status.cur_status == PHY_LINK_ON)) {
        if (my_phy_status.last_status != PHY_LINK_ON) {
            APP_PRINTF("PHY link\r\n");
            wiz_print_phy_info();
            app_eventbus_publish(EVENT_PHY_LINK_ON, NULL); // 检测到网线插入
            my_phy_status.last_status = my_phy_status.cur_status;
        }
    } else {
        if (my_phy_status.last_status != PHY_LINK_OFF) { // 检测到网线拔出
            APP_PRINTF("PHY no link\r\n");
            app_eventbus_publish(EVENT_PHY_LINK_OFF, NULL);
            my_phy_status.last_status = my_phy_status.cur_status;
        }
    }
    if (dhcp_init_flag == 1) { // dhcp 初始化完成
        wiz_dhcp_task();
    }
}

// 打印当前网络配置信息(IP/MAC等)
static void print_network_information(wiz_NetInfo *conf_info)
{
    APP_PRINTF("[network configuration] ==========================\n");

    if (conf_info->dhcp == NETINFO_DHCP) {
        APP_PRINTF(" %s network configuration : DHCP\r\n", _WIZCHIP_ID_);
    } else {
        APP_PRINTF("%s network configuration : STATIC\r\n", _WIZCHIP_ID_);
    }

    APP_PRINTF("MAC         : %02X:%02X:%02X:%02X:%02X:%02X\r\n", conf_info->mac[0], conf_info->mac[1], conf_info->mac[2], conf_info->mac[3], conf_info->mac[4], conf_info->mac[5]);
    APP_PRINTF("IP          : %d.%d.%d.%d\r\n", conf_info->ip[0], conf_info->ip[1], conf_info->ip[2], conf_info->ip[3]);
    APP_PRINTF("Subnet Mask : %d.%d.%d.%d\r\n", conf_info->sn[0], conf_info->sn[1], conf_info->sn[2], conf_info->sn[3]);
    APP_PRINTF("Gateway     : %d.%d.%d.%d\r\n", conf_info->gw[0], conf_info->gw[1], conf_info->gw[2], conf_info->gw[3]);
    APP_PRINTF("DNS         : %d.%d.%d.%d\r\n", conf_info->dns[0], conf_info->dns[1], conf_info->dns[2], conf_info->dns[3]);
    APP_PRINTF("\n");
}

// DHCP获取任务
static void wiz_dhcp_task(void)
{
    uint8_t ret = DHCP_run();
    switch (ret) {
        case DHCP_IP_LEASED:
            getIPfromDHCP(conf_info.ip);
            getGWfromDHCP(conf_info.gw);
            getSNfromDHCP(conf_info.sn);
            getDNSfromDHCP(conf_info.dns);

            conf_info.dhcp = NETINFO_DHCP;
            getSHAR(conf_info.mac);
            wizchip_setnetinfo(&conf_info);
            APP_PRINTF("DHCP success!\r\n");
            app_eventbus_publish(EVENT_MQTT_CONNECT, NULL); // 发送消息给 app_network_model 进行 mqtt 连接
            print_network_information(&conf_info);
            dev_set_nw_cfg_info(&conf_info);
            dhcp_init_flag = 0;
            break;

        case DHCP_RUNNING:
            APP_PRINTF("DHCP_RUNNING\n");
            break;
        default:
            return;
    }
}

// 网络初始化(支持DHCP/静态IP)
void network_init(uint8_t *ethernet_buff, wiz_NetInfo *conf_info)
{
    wizchip_setnetinfo(conf_info);
    APP_PRINTF("[network_init] ===================================\n");
    if (conf_info->dhcp == NETINFO_DHCP) { // 如果是自动IP
        DHCP_init(0, ethernet_buff);
        dhcp_init_flag = 1;
        APP_PRINTF("DHCP running\r\n");
    } else {

        // delay_1ms(10000); // 200ms 或更长，根据网线和交换机延迟
        print_network_information(conf_info);
        app_eventbus_publish(EVENT_MQTT_CONNECT, NULL);
    }
}
