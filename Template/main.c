#include "main.h"
#include "../Source/app/app_evenbus/app_eventbus.h"
#include "../Source/app/app_timer/app_timer.h"
#include "../Source/app/app_watchdog/app_watchdog.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"
#include "../Source/dev/dev_manager.h"
#include "gd32f30x.h"
#include "systick.h"
#include <stdio.h>

static void app_feed_dog(void *arg);

int main(void)
{
    __enable_irq();

    systick_config();
    delay_1ms(100);

    bsp_usart_init(UART4, 115200); // 初始化调试串口

    app_timer_init();    // 初始化软定时器
    app_watchdog_init(); // 初始化看门狗

    app_eventbus_init(); // 初始化 evenvbus
    dev_manmager_init(); // 设备初始化

    app_timer_start(1000, app_feed_dog, true, NULL, "feeddog");
    while (1) {
        app_timer_poll();
        app_eventbus_poll();
    }
}

static void app_feed_dog(void *arg)
{
    app_watchdog_feed();
}
