#ifndef _BSP_I2C_H_
#define _BSP_I2C_H_
#include <stdbool.h>
#include <stdint.h>

void bsp_i2c_init(uint32_t i2c_periph, uint8_t i2c_addr);
bool bsp_i2c_write(uint32_t i2c_periph, uint8_t i2c_addr, uint8_t reg, uint8_t data);
bool bsp_i2c_read(uint32_t i2c_periph, uint8_t i2c_addr, uint8_t reg, uint8_t *data);

#endif