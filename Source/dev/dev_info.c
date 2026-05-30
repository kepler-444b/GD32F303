#include "dev_info.h"
#include "systick.h"
#include "../Source/bsp/bsp_flash/bsp_flash.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"
#include "../Source/app/app_evenbus/app_eventbus.h"
#include "../app/app_base/app_base.h"
#include <string.h>

// 函数声明
static void dev_load_device_save_info(void);
static bool dev_save_device_save_info(void);
static bool dev_save_nw_cfg_info(void);

static void dev_load_nw_cfg_info(void);
static void dev_nw_cfg_event_handler(event_type_e event, void *params);
static void dev_nw_cfg_protocol_check(type_c_rx_t *buf);

// 全局变量
static dev_save_info_t my_dev_save_info;
static wiz_NetInfo my_nw_info;

// 默认配网信息
static const uint8_t default_ip[4]  = {192, 168, 42, 96};
static const uint8_t default_gw[4]  = {192, 168, 39, 1};
static const uint8_t default_sn[4]  = {255, 255, 240, 0};
static const uint8_t default_dns[4] = {8, 8, 8, 8};
static const uint8_t dhcp           = NETINFO_DHCP;

void dev_device_info_init(void)
{
    dev_load_device_save_info(); // 加载设备信息
    dev_load_nw_cfg_info();      // 加载配网信息
    app_eventbus_subscribe(dev_nw_cfg_event_handler);
}

static void dev_nw_cfg_event_handler(event_type_e event, void *params)
{
    switch (event) {
        case EVENT_USART0_SET: {
            type_c_rx_t *frame = (type_c_rx_t *)params;
            dev_nw_cfg_protocol_check(frame);
        } break;
        default:
            break;
    }
}

static void dev_nw_cfg_protocol_check(type_c_rx_t *buf)
{
    // 获取设备信息
    if (buf->buffer[0] == 0x01 && buf->buffer[1] == 0x02 && buf->buffer[2] == 0x03) {
        dev_packet_t packet;
        memcpy(&packet.net, &my_nw_info, sizeof(wiz_NetInfo));
        memcpy(packet.devices, my_dev_save_info.devices, sizeof(packet.devices));
        memcpy(packet.cur_ver, my_dev_save_info.cur_ver, sizeof(packet.cur_ver));
        memcpy(packet.key, my_dev_save_info.key, sizeof(packet.key));

        bsp_usart_tx_buf((const uint8_t *)&packet, sizeof(packet), USART0);
    }
    // 接收网络信息
    else {
        dev_packet_t packet;
        memcpy(&packet, buf, sizeof(dev_packet_t));
        memcpy(&my_nw_info, &packet.net, sizeof(my_nw_info));
        dev_save_nw_cfg_info();
        memcpy(&my_dev_save_info.devices, &packet.devices, sizeof(my_dev_save_info.devices));
        memcpy(&my_dev_save_info.key, &packet.key, sizeof(my_dev_save_info.key));

        if (dev_save_device_save_info() == true) {
            delay_1ms(100);
            NVIC_SystemReset(); // 重启系统
        }
    }
}

// 加载网络配置
static void dev_load_nw_cfg_info(void)
{
    fmc_state_enum status;
    status = app_flash_read_word(FLASH_DEV_CFG, (uint32_t *)&my_nw_info, sizeof(my_nw_info));

    if (status == FMC_READY) {
        if (my_nw_info.ip[0] == 0xFF) {
            memcpy(my_nw_info.ip, default_ip, sizeof(my_nw_info.ip)); // IP
            memcpy(my_nw_info.gw, default_gw, sizeof(my_nw_info.gw)); // WG
            my_nw_info.dhcp = NETINFO_STATIC;
        }
        APP_PRINTF("dev_load_nw_cfg success!\n");
    } else {
        APP_ERROR("dev_load_nw_cfg error\n");
    }

    uint8_t default_mac[12] = {0x00, 0x08, 0xDC, 0x12, 0x22, 0x12};
    app_get_uid(default_mac);
    default_mac[0] = default_mac[0] & 0xFE; // 强制将第一个字节的最低位设为0,确保是单播地址
    APP_PRINTF_BUF("mac", default_mac, 6);
    memcpy(my_nw_info.mac, default_mac, 6); // MAC 固定
    if (my_nw_info.dhcp == NETINFO_STATIC) {
        memcpy(my_nw_info.dns, default_dns, sizeof(default_dns)); // DNS
        memcpy(my_nw_info.sn, default_sn, sizeof(default_sn));    // SN
    }
    APP_PRINTF("\n");
}

// 存储网络配置
static bool dev_save_nw_cfg_info(void)
{
    fmc_state_enum status;
    status = app_flash_write_word(FLASH_DEV_CFG, (uint32_t *)&my_nw_info, sizeof(my_nw_info));

    if (status == FMC_READY) {
        APP_PRINTF("save_nw_info success!\n");
        return true;
    } else {
        APP_ERROR("save_nw_info error");
        return false;
    }
    return false;
}

// 加载设备信息
static void dev_load_device_save_info(void)
{
    APP_PRINTF("[load device_info] ===============================\n");
    fmc_state_enum status;
    status = app_flash_read_word(FLASH_OTA_INFO, (uint32_t *)&my_dev_save_info, sizeof(my_dev_save_info));

    if (status == FMC_READY) {

        if (my_dev_save_info.cur_ver[0] == 0xFF) {
            snprintf(my_dev_save_info.cur_ver, sizeof(my_dev_save_info.cur_ver), "%s", CUR_VER);
            snprintf(my_dev_save_info.devices, sizeof(my_dev_save_info.devices), "%s", "NULL");
            snprintf(my_dev_save_info.key, sizeof(my_dev_save_info.key), "%s", "NULL");
        }
        APP_PRINTF("dev_load_device_save_info success!\n");
    } else {
        APP_ERROR("dev_load_device_save_info error\n");
    }

    snprintf(my_dev_save_info.products, sizeof(my_dev_save_info.devices), "%s", "pzhKZPs57u");
    snprintf(my_dev_save_info.version, sizeof(my_dev_save_info.version), "%s", "2018-10-31");
    my_dev_save_info.et = 2095545600; // 2036-05-28 00:00:00 UTC
    APP_PRINTF("\n");
}

// 存储设备信息
static bool dev_save_device_save_info(void)
{
    fmc_state_enum status;
    status = app_flash_write_word(FLASH_OTA_INFO, (uint32_t *)&my_dev_save_info, sizeof(my_dev_save_info));

    if (status == FMC_READY) {
        APP_PRINTF("dev_save_device_save_info success!\n");
        return true;
    } else {
        APP_ERROR("dev_save_device_save_info error");
        return false;
    }
    return false;
}

// 设置网络信息
bool dev_set_nw_cfg_info(wiz_NetInfo *dev_info)
{
    memcpy(&my_nw_info, dev_info, sizeof(my_nw_info));
    return dev_save_nw_cfg_info();
}
// 获取网络信息
const wiz_NetInfo *dev_get_nw_cfg_info(void)
{
    return &my_nw_info;
}

// 获取设备信息
const dev_save_info_t *dev_get_save_device_info(void)
{
    return &my_dev_save_info;
}

// 设置设备信息
bool dev_set_save_device_info(dev_save_info_t *dev_info)
{
    memcpy(&my_dev_save_info, dev_info, sizeof(my_dev_save_info));
    return dev_save_device_save_info();
}
