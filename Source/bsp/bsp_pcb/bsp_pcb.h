#ifndef _BSP_PCB_H_
#define _BSP_PCB_H_

#include "gd32f30x_gpio.h"
#include "../Source/bsp/bsp_gpio/bsp_gpio.h"

#define RD2_SET_H BSP_SET_GPIO(PD0, true)
#define RD2_SET_L BSP_SET_GPIO(PD0, false)

#define RD4_SET_H BSP_SET_GPIO(PD1, true)
#define RD4_SET_L BSP_SET_GPIO(PD1, false)

void bsp_usart2_init(void);
void bsp_usart4_init(void);

#endif