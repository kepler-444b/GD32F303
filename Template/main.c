#include "main.h"
#include "../Source/app/app_evenbus/app_eventbus.h"
#include "../Source/app/app_timer/app_timer.h"
#include "../Source/dev/dev_manager.h"
#include "gd32f30x.h"
#include "systick.h"
#include <stdio.h>

int main(void)
{
    __enable_irq();

    systick_config();
    delay_1ms(100);

    app_timer_init();    // 初始化软定时器
    app_eventbus_init(); // 初始化 evenvbus

    dev_manmager_init(); // 设备初始化

    while (1) {
        app_timer_poll();
        app_eventbus_poll();
    }
}
