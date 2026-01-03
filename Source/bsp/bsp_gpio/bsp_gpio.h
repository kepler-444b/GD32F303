#ifndef _BSP_GPIO_H_
#define _BSP_GPIO_H_

#include <stdint.h>
#include <stdbool.h>
#include "gd32f30x_gpio.h"

// 检查GPIO是否有效
#define GPIO_IS_VALID(obj) ((obj).pin || (obj).port)

// 设置GPIO
#define BSP_SET_GPIO(obj, status) \
    ((status) ? (GPIO_BOP((obj).port) = (uint32_t)(obj).pin) : (GPIO_BC((obj).port) = (uint32_t)(obj).pin))

// 获取GPIO
#define BSP_GET_GPIO(obj) \
    (((bool)false != (GPIO_ISTAT((obj).port) & ((obj).pin))) ? true : false)

typedef struct {
    uint32_t port;
    uint32_t pin;
} gpio_pin_t;

// 通用GPIO宏定义
#define DEFINE_GPIO(port, pin) ((gpio_pin_t){port, pin})

// 默认无效引脚
#define DEF  DEFINE_GPIO(0, 0)
#define PA0  DEFINE_GPIO(GPIOA, GPIO_PIN_0)
#define PA1  DEFINE_GPIO(GPIOA, GPIO_PIN_1)
#define PA2  DEFINE_GPIO(GPIOA, GPIO_PIN_2)
#define PA3  DEFINE_GPIO(GPIOA, GPIO_PIN_3)
#define PA4  DEFINE_GPIO(GPIOA, GPIO_PIN_4)
#define PA5  DEFINE_GPIO(GPIOA, GPIO_PIN_5)
#define PA6  DEFINE_GPIO(GPIOA, GPIO_PIN_6)
#define PA7  DEFINE_GPIO(GPIOA, GPIO_PIN_7)
#define PA8  DEFINE_GPIO(GPIOA, GPIO_PIN_8)
#define PA9  DEFINE_GPIO(GPIOA, GPIO_PIN_9)
#define PA10 DEFINE_GPIO(GPIOA, GPIO_PIN_10)
#define PA11 DEFINE_GPIO(GPIOA, GPIO_PIN_11)
#define PA12 DEFINE_GPIO(GPIOA, GPIO_PIN_12)
#define PA13 DEFINE_GPIO(GPIOA, GPIO_PIN_13)
#define PA14 DEFINE_GPIO(GPIOA, GPIO_PIN_14)
#define PA15 DEFINE_GPIO(GPIOA, GPIO_PIN_15)
#define PB0  DEFINE_GPIO(GPIOB, GPIO_PIN_0)
#define PB1  DEFINE_GPIO(GPIOB, GPIO_PIN_1)
#define PB2  DEFINE_GPIO(GPIOB, GPIO_PIN_2)
#define PB3  DEFINE_GPIO(GPIOB, GPIO_PIN_3)
#define PB4  DEFINE_GPIO(GPIOB, GPIO_PIN_4)
#define PB5  DEFINE_GPIO(GPIOB, GPIO_PIN_5)
#define PB6  DEFINE_GPIO(GPIOB, GPIO_PIN_6)
#define PB7  DEFINE_GPIO(GPIOB, GPIO_PIN_7)
#define PB8  DEFINE_GPIO(GPIOB, GPIO_PIN_8)
#define PB9  DEFINE_GPIO(GPIOB, GPIO_PIN_9)
#define PB10 DEFINE_GPIO(GPIOB, GPIO_PIN_10)
#define PB11 DEFINE_GPIO(GPIOB, GPIO_PIN_11)
#define PB12 DEFINE_GPIO(GPIOB, GPIO_PIN_12)
#define PB13 DEFINE_GPIO(GPIOB, GPIO_PIN_13)
#define PB14 DEFINE_GPIO(GPIOB, GPIO_PIN_14)
#define PB15 DEFINE_GPIO(GPIOB, GPIO_PIN_15)

#define PD1  DEFINE_GPIO(GPIOD, GPIO_PIN_1)
#define PD0  DEFINE_GPIO(GPIOD, GPIO_PIN_0)

#endif