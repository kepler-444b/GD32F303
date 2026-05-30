#ifndef _BSP_BOARD_H_
#define _BSP_BOARD_H_
#include <stdbool.h>
#include <stdint.h>

void bsp_board_init(void);
void bsp_set_led_status(bool status);
void bsp_set_buuzzer(uint8_t count);

#endif