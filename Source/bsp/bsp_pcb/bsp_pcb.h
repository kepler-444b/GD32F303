#ifndef _BSP_PCB_H_
#define _BSP_PCB_H_

#include "gd32f30x_gpio.h"
#include "../Source/bsp/bsp_gpio/bsp_gpio.h"

// 面板通讯的收发使能
#define RD2_SET_H BSP_SET_GPIO(PE15, true)
#define RD2_SET_L BSP_SET_GPIO(PE15, false)

#define RD3_SET_H BSP_SET_GPIO(PD0, true)
#define RD3_SET_L BSP_SET_GPIO(PD0, false)

void pcb_usart0_init(void); // type-c
void pcb_usart1_init(void); // 串口屏
void pcb_usart2_init(void); // 面板通讯
void pcb_uart3_init(void);  // 扩展通讯
void pcb_uart4_init(void);  // 调试

void pcb_buzzer_init(void); // 蜂鸣器
void pcb_led_init(void);    // 指示灯
void pcb_key_init(void);    // 按键

void pcb_adc_key_init(void);
void pcb_adc_temp_init(void);

void pcb_rtc_ex_init(void);
void pcb_i2c_init(void);

#endif