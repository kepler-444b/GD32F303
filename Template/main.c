
#include "main.h"
#include "../Source/app/app_evenbus/app_eventbus.h"
#include "../Source/app/app_protocol/app_protocol.h"
#include "../Source/app/app_timer/app_timer.h"
#include "../Source/app/app_mqtt_model/app_mqtt_model.h"
#include "../Source/dev/dev_manager.h"
#include "../Source/w5500/ioLibrary_Driver/Ethernet/wizchip_conf.h"
#include "../Source/w5500/wiz_interface/wiz_interface.h"
#include "../Source/w5500/wiz_platform/wiz_platform.h"
#include "gd32f30x.h"
#include "systick.h"
#include <stdio.h>

#if 0
#define SOCKET_ID             0
#define ETHERNET_BUF_MAX_SIZE (1024 * 2)

/* network information */
wiz_NetInfo default_net_info = {
    .mac = {0x00, 0x08, 0xdc, 0x12, 0x22, 0x12},
    .ip = {192, 168, 40, 30},
    .gw = {192, 168, 39, 1},
    .sn = {255, 255, 255, 0},
    .dns = {8, 8, 8, 8},
    .dhcp = NETINFO_STATIC}; // static ip
uint8_t ethernet_buf[ETHERNET_BUF_MAX_SIZE] = {0};

static uint8_t mqtt_send_ethernet_buf[ETHERNET_BUF_MAX_SIZE] = {0};
static uint8_t mqtt_recv_ethernet_buf[ETHERNET_BUF_MAX_SIZE] = {0};
#endif
int main(void)
{
#ifdef __FIRMWARE_VERSION_DEFINE
    uint32_t fw_ver = 0;
#endif

    /* configure systick */
    systick_config();
    delay_1ms(100);
    printf("\r\nCK_SYS is %d", rcu_clock_freq_get(CK_SYS));
    printf("\r\nCK_AHB is %d", rcu_clock_freq_get(CK_AHB));
    printf("\r\nCK_APB1 is %d", rcu_clock_freq_get(CK_APB1));
    printf("\r\nCK_APB2 is %d", rcu_clock_freq_get(CK_APB2));
    printf("\n");

#ifdef __FIRMWARE_VERSION_DEFINE
    fw_ver = gd32f30x_firmware_version_get();
    /* print firmware version */
    // printf("\r\nGD32F30x series firmware version: V%d.%d.%d", (uint8_t)(fw_ver >> 24), (uint8_t)(fw_ver >> 16), (uint8_t)(fw_ver >> 8));
#endif /* __FIRMWARE_VERSION_DEFINE */

    app_timer_init();
    app_eventbus_init();
    app_protocol_init();
    dev_manmager_init();
    app_mqtt_model_init();

    while (1) {
        app_timer_poll();
        app_eventbus_poll();
    }
}
