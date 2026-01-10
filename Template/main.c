
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
    __enable_irq();

    systick_config();
    delay_1ms(100);

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
