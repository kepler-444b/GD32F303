#include "bsp_board.h"
#include "systick.h"
#include "../bsp_pcb/bsp_pcb.h"
#include "../bsp_adc/bsp_adc.h"

void bsp_board_init(void)
{
    pcb_buzzer_init(); // 蜂鸣器初始化
    pcb_led_init();    // 指示灯led初始化
    pcb_key_init();    // 按键初始化
    bsp_adc_init();    // adc初始化
}

void bsp_set_buuzzer(uint8_t count)
{
    if (count == 0xFF) { // 打开蜂鸣器
        BSP_SET_GPIO(PD9, true);
        return;
    }
    if (count == 0xFE) { // 关闭蜂鸣器
        BSP_SET_GPIO(PD9, false);
        return;
    }
    for (uint8_t i = 0; i < count; i++) {
        BSP_SET_GPIO(PD9, true);
        delay_1ms(100);
        BSP_SET_GPIO(PD9, false);
        delay_1ms(100);
    }
}

void bsp_set_led_status(bool status)
{
    BSP_SET_GPIO(PD10, !status);
}