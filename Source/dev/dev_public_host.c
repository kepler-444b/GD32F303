#include "dev_public_host.h"
#include "../Source/app/app_public_protocol/app_public_protocol.h"
#include "../Source/app/app_network_model/app_network_model.h"
#include "../Source/bsp/bsp_pcb/bsp_pcb.h"

// 函数声明
static void dev_board_init(void);

void dev_public_host(void)
{
    app_public_protocol_init(); // 初始化面板,可扩展协议层
    app_network_model_init();   // 初始化网络协议层
}

static void dev_board_init(void)
{
    bsp_buzzer_init();
    bsp_led_init();
}
