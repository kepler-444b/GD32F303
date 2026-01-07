
#include "main.h"
#include "../Source/app/app_evenbus/app_eventbus.h"
#include "../Source/app/app_protocol/app_protocol.h"
#include "../Source/app/app_timer/app_timer.h"
#include "../Source/app/app_network_model/app_network_model.h"
#include "../Source/dev/dev_manager.h"
#include "gd32f30x.h"
#include "systick.h"
#include <stdio.h>

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

    app_network_model_init();

    while (1) {
        app_timer_poll();
        app_eventbus_poll();
    }
}
