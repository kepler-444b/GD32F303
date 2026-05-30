#ifndef _BSP_ADC_H_
#define _BSP_ADC_H_
#include <stdint.h>

void bsp_adc_init(void);

uint16_t bsp_adc_read_key(void);
float bsp_adc_read_temp(void);

#endif