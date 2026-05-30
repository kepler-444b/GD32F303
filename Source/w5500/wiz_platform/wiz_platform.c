#include "gd32f30x.h"
#include "gd32f30x_gpio.h"
#include "gd32f30x_spi.h"
#include "systick.h"
#include <stdint.h>
#include <stdio.h>

#include "../ioLibrary_Driver/Ethernet/wizchip_conf.h"
#include "../ioLibrary_Driver/Internet/DHCP/dhcp.h"
#include "../ioLibrary_Driver/Internet/DNS/dns.h"
#include "../ioLibrary_Driver/Internet/MQTT/mqtt_interface.h"
#include "../wiz_interface/wiz_interface.h"
#include "wiz_platform.h"

#define SYSTEM_CLOCK_FREQ 120000000 ///< 系统时钟频率
#define TIMER_PERIOD      999       ///< 定时器周期(1ms中断)

// W5500 所需定时器
void wiz_timer_init(void)
{
    timer_parameter_struct timer_initpara;

    rcu_periph_clock_enable(RCU_TIMER2); // 使能TIMER2时钟
    timer_deinit(TIMER2);

    timer_initpara.prescaler         = (SYSTEM_CLOCK_FREQ / 1000000) - 1; // 120 MHz / 120 = 1 MHz
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = TIMER_PERIOD; // 1 MHz / 1000 = 1ms
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(TIMER2, &timer_initpara);

    timer_auto_reload_shadow_enable(TIMER2);

    timer_interrupt_flag_clear(TIMER2, TIMER_INT_FLAG_UP);
    timer_interrupt_enable(TIMER2, TIMER_INT_UP);

    nvic_irq_enable(TIMER2_IRQn, 0, 1);

    timer_enable(TIMER2);
}

void wiz_spi_init(void)
{
    rcu_periph_clock_enable(RCU_SPI1);

    spi_parameter_struct spi_initstructure;
    spi_struct_para_init(&spi_initstructure); // 默认初始化

    spi_initstructure.device_mode          = SPI_MASTER;
    spi_initstructure.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_initstructure.frame_size           = SPI_FRAMESIZE_8BIT;
    spi_initstructure.nss                  = SPI_NSS_SOFT;
    spi_initstructure.endian               = SPI_ENDIAN_MSB;
    spi_initstructure.clock_polarity_phase = SPI_CK_PL_LOW_PH_1EDGE; // CPOL=0, CPHA=0
    spi_initstructure.prescale             = SPI_PSC_8;

    spi_init(SPI1, &spi_initstructure);
    spi_enable(SPI1);

    gpio_bit_set(WIZ_SCS_PORT, WIZ_SCS_PIN);
}

// 配置SPI所用引脚
void wiz_rst_int_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_AF);

    gpio_init(GPIOB, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_13 | GPIO_PIN_15); // SCK, MOSI
    gpio_init(GPIOB, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_14);         // MISO
    gpio_init(WIZ_RST_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, WIZ_RST_PIN);       // RST
    gpio_init(WIZ_SCS_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, WIZ_SCS_PIN);       // SCS
    gpio_init(WIZ_INT_PORT, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, WIZ_INT_PIN);          // INT

    gpio_bit_set(WIZ_RST_PORT, WIZ_RST_PIN);
}

// 选择W5500芯片(CS拉低)
void wizchip_select(void)
{
    gpio_bit_reset(WIZ_SCS_PORT, WIZ_SCS_PIN);
}

// 取消选择W5500芯片(CS拉高)
void wizchip_deselect(void)
{
    gpio_bit_set(WIZ_SCS_PORT, WIZ_SCS_PIN);
}

// SPI发送1字节数据
void wizchip_write_byte(uint8_t dat)
{
    while (spi_i2s_flag_get(SPI1, SPI_FLAG_TBE) == RESET);
    SPI_DATA(SPI1) = dat;
    while (spi_i2s_flag_get(SPI1, SPI_FLAG_RBNE) == RESET);
    SPI_DATA(SPI1);
}

// SPI读取1字节数据
uint8_t wizchip_read_byte(void)
{
    while (spi_i2s_flag_get(SPI1, SPI_FLAG_TBE) == RESET);
    SPI_DATA(SPI1) = 0xFF;
    while (spi_i2s_flag_get(SPI1, SPI_FLAG_RBNE) == RESET);
    return (uint8_t)SPI_DATA(SPI1);
}

// SPI批量写入数据
void wizchip_write_buff(uint8_t *buf, uint16_t len)
{
    uint16_t idx = 0;
    for (idx = 0; idx < len; idx++) {
        wizchip_write_byte(buf[idx]);
    }
}

// SPI批量读取数据
void wizchip_read_buff(uint8_t *buf, uint16_t len)
{
    uint16_t idx = 0;
    for (idx = 0; idx < len; idx++) {
        buf[idx] = wizchip_read_byte();
    }
}

// W5500硬件复位
void wizchip_reset(void)
{
    gpio_bit_set(WIZ_RST_PORT, WIZ_RST_PIN);
    delay_1ms(10);
    gpio_bit_reset(WIZ_RST_PORT, WIZ_RST_PIN);
    delay_1ms(10);
    gpio_bit_set(WIZ_RST_PORT, WIZ_RST_PIN);
    delay_1ms(10);
}

// 注册W5500 SPI回调函数
void wizchip_spi_cb_reg(void)
{
    reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);            // CS控制
    reg_wizchip_spi_cbfunc(wizchip_read_byte, wizchip_write_byte);      // 单字节SPI
    reg_wizchip_spiburst_cbfunc(wizchip_read_buff, wizchip_write_buff); // 批量SPI
}

// TIMER2中断服务函数
void TIMER2_IRQHandler(void)
{
    static uint32_t wiz_timer_1ms_count = 0;
    if (SET == timer_interrupt_flag_get(TIMER2, TIMER_INT_FLAG_UP)) {
        timer_interrupt_flag_clear(TIMER2, TIMER_INT_FLAG_UP);

        wiz_timer_1ms_count++;
        MilliTimer_Handler(); // MQTT 毫秒计时器
        if (wiz_timer_1ms_count >= 1000) {
            DHCP_time_handler(); // DHCP 1秒计时器
            DNS_time_handler();  // DNS 1秒计时器
            wiz_timer_1ms_count = 0;
        }
    }
}
