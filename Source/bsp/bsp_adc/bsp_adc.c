#include "bsp_adc.h"
#include "../bsp_pcb/bsp_pcb.h"
#include "../bsp_usart/bsp_usart.h"
#include "gd32f30x_dma.h"
#include <string.h>
#include "systick.h"
#include <math.h>

#define NTC_R0              10000 // 25°C电阻 (Ω)
#define NTC_T0              29815 // 25°C * 100 (开尔文温度*100)
#define NTC_B_VALUE         3950  // B常数
#define NTC_R_FIXED         10000 // 固定电阻 (Ω)
#define NTC_VREF            3300  // 参考电压 (mV)

#define ADC_TO_VOL(adc_val) ((uint16_t)((uint32_t)(adc_val) * 330 / 4095))

#define ADC_TO_MV(adc_val)  ((uint16_t)(ADC_TO_VOL(adc_val) * 10))
static volatile uint16_t adc_values[2] = {0};

void bsp_adc_init(void)
{

    pcb_adc_key_init();
    pcb_adc_temp_init();

    rcu_periph_clock_enable(RCU_DMA0);
    rcu_periph_clock_enable(RCU_ADC0);
    rcu_adc_clock_config(RCU_CKADC_CKAPB2_DIV6);

    dma_deinit(DMA0, DMA_CH0);
    adc_deinit(ADC0);

    dma_parameter_struct dma_data_parameter;
    dma_struct_para_init(&dma_data_parameter);

    dma_data_parameter.periph_addr  = (uint32_t)(&ADC_RDATA(ADC0));
    dma_data_parameter.periph_inc   = DMA_PERIPH_INCREASE_DISABLE;
    dma_data_parameter.memory_addr  = (uint32_t)(adc_values);
    dma_data_parameter.memory_inc   = DMA_MEMORY_INCREASE_ENABLE;
    dma_data_parameter.periph_width = DMA_PERIPHERAL_WIDTH_16BIT;
    dma_data_parameter.memory_width = DMA_MEMORY_WIDTH_16BIT;
    dma_data_parameter.direction    = DMA_PERIPHERAL_TO_MEMORY;
    dma_data_parameter.number       = 2;
    dma_data_parameter.priority     = DMA_PRIORITY_HIGH;
    dma_init(DMA0, DMA_CH0, &dma_data_parameter);

    dma_circulation_enable(DMA0, DMA_CH0);
    dma_channel_enable(DMA0, DMA_CH0);

    adc_mode_config(ADC_MODE_FREE);
    adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, ENABLE);
    adc_special_function_config(ADC0, ADC_SCAN_MODE, ENABLE);
    adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT);

    adc_channel_length_config(ADC0, ADC_REGULAR_CHANNEL, 2);
    adc_regular_channel_config(ADC0, 0, ADC_CHANNEL_8, ADC_SAMPLETIME_239POINT5); // PB0
    adc_regular_channel_config(ADC0, 1, ADC_CHANNEL_9, ADC_SAMPLETIME_239POINT5); // PB1

    adc_external_trigger_source_config(ADC0, ADC_REGULAR_CHANNEL, ADC0_1_2_EXTTRIG_REGULAR_NONE);
    adc_external_trigger_config(ADC0, ADC_REGULAR_CHANNEL, ENABLE);

    adc_dma_mode_enable(ADC0);
    adc_enable(ADC0);
    delay_1ms(10); // 加长稳定时间
    adc_calibration_enable(ADC0);

    adc_software_trigger_enable(ADC0, ADC_REGULAR_CHANNEL);
}

float bsp_adc_to_temperature(uint16_t adc_value)
{

    uint16_t voltage_mv;
    float r_ntc;
    float temp_k;
    float temp_c;

    voltage_mv = ADC_TO_MV(adc_value);

    if (voltage_mv <= 0 || voltage_mv >= NTC_VREF) {
        return -999.0f;
    }
    r_ntc = (float)NTC_R_FIXED * voltage_mv / (NTC_VREF - voltage_mv);

    temp_k = 1.0f / (100.0f / NTC_T0 + (1.0f / NTC_B_VALUE) * logf(r_ntc / NTC_R0));
    temp_c = temp_k - 273.15f;
    temp_c = roundf(temp_c * 100) / 100; // 保留2位小数
    return temp_c;
}

uint16_t bsp_adc_read_key(void)
{
    return ADC_TO_VOL(adc_values[1]);
}

float bsp_adc_read_temp(void)
{
    return bsp_adc_to_temperature(adc_values[0]);
}
