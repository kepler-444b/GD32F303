// #include "bsp_spi_dma.h"
// #include "gd32f30x.h"
// #include <stdio.h>

// #define SPI1_NSS_PIN   GPIO_PIN_12
// #define SPI1_SCK_PIN   GPIO_PIN_13
// #define SPI1_MISO_PIN  GPIO_PIN_14
// #define SPI1_MOSI_PIN  GPIO_PIN_15
// #define SPI1_GPIO_PORT GPIOB

// /* DMA0 固定通道宏定义 */
// #define SPI1_TX_DMA_CH DMA_CH0
// #define SPI1_RX_DMA_CH DMA_CH1
// #define SPI1_DMA       DMA0

// /* NSS 控制宏 */
// #define SPI1_NSS_LOW()  gpio_bit_reset(SPI1_GPIO_PORT, SPI1_NSS_PIN)
// #define SPI1_NSS_HIGH() gpio_bit_set(SPI1_GPIO_PORT, SPI1_NSS_PIN)

// static void rcu_config(void);
// static void gpio_config(void);
// static void spi_config(void);
// static void spi_dma_rx_config(void);

// static spi_rx_t m_spi_rx;
// static spi_rx_callback_t spi_rx_callback = NULL;
// void bsp_spi_register_rx_callback(spi_rx_callback_t callback)
// {
//     spi_rx_callback = callback;
// }

// void bsp_spi_init(void)
// {
//     rcu_config();
//     gpio_config();
//     spi_config();
// }

// static void rcu_config(void)
// {
//     rcu_periph_clock_enable(RCU_GPIOB);
//     rcu_periph_clock_enable(RCU_AF);
//     rcu_periph_clock_enable(RCU_SPI1);
//     rcu_periph_clock_enable(RCU_DMA0);
// }

// static void gpio_config(void)
// {
//     gpio_init(SPI1_GPIO_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, SPI1_NSS_PIN);
//     gpio_init(SPI1_GPIO_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, SPI1_SCK_PIN | SPI1_MOSI_PIN);
//     gpio_init(SPI1_GPIO_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, SPI1_MISO_PIN);
// }

// static void spi_config(void)
// {
//     spi_parameter_struct spi_init_struct;

//     spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX; // 全双工
//     spi_init_struct.device_mode          = SPI_MASTER;               // 主机模式
//     spi_init_struct.frame_size           = SPI_FRAMESIZE_8BIT;       // 8位数据帧
//     spi_init_struct.clock_polarity_phase = SPI_CK_PL_LOW_PH_1EDGE;   // 时钟极性/相位
//     spi_init_struct.nss                  = SPI_NSS_SOFT;             // 软件控制 NSS
//     spi_init_struct.prescale             = SPI_PSC_32;               // 波特率预分频
//     spi_init_struct.endian               = SPI_ENDIAN_MSB;           // MSB 先行

//     spi_init(SPI1, &spi_init_struct);
//     spi_enable(SPI1);
// }

// static void spi_dma_rx_config(void)
// {
//     dma_parameter_struct dma_init_struct;

//     dma_deinit(SPI1_DMA, SPI1_RX_DMA_CH);
//     dma_init_struct.periph_addr  = (uint32_t)&SPI_DATA(SPI1);
//     dma_init_struct.memory_addr  = (uint32_t)m_spi_rx.data;
//     dma_init_struct.direction    = DMA_PERIPHERAL_TO_MEMORY;
//     dma_init_struct.number       = m_spi_rx.length;
//     dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
//     dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
//     dma_init_struct.priority     = DMA_PRIORITY_HIGH;
//     dma_init_struct.periph_inc   = DMA_PERIPH_INCREASE_DISABLE;
//     dma_init_struct.memory_inc   = DMA_MEMORY_INCREASE_ENABLE;

//     dma_init(SPI1_DMA, SPI1_RX_DMA_CH, &dma_init_struct);
//     dma_circulation_enable(SPI1_DMA, SPI1_RX_DMA_CH);
//     dma_memory_to_memory_disable(SPI1_DMA, SPI1_RX_DMA_CH);

//     dma_channel_enable(SPI1_DMA, SPI1_RX_DMA_CH);
//     dma_interrupt_enable(SPI1_DMA, SPI1_RX_DMA_CH, DMA_INT_FTF);

//     nvic_irq_enable(DMA0_Channel1_IRQn, 1, 0);
// }

// void spi_dma_tx(uint8_t *tx_buf, uint16_t length)
// {
//     dma_parameter_struct dma_init_struct;

//     SPI1_NSS_LOW();

//     dma_deinit(SPI1_DMA, SPI1_TX_DMA_CH);
//     dma_init_struct.periph_addr  = (uint32_t)&SPI_DATA(SPI1);
//     dma_init_struct.memory_addr  = (uint32_t)tx_buf;
//     dma_init_struct.direction    = DMA_MEMORY_TO_PERIPHERAL;
//     dma_init_struct.number       = length;
//     dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
//     dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
//     dma_init_struct.priority     = DMA_PRIORITY_MEDIUM;
//     dma_init_struct.periph_inc   = DMA_PERIPH_INCREASE_DISABLE;
//     dma_init_struct.memory_inc   = DMA_MEMORY_INCREASE_ENABLE;

//     dma_init(SPI1_DMA, SPI1_TX_DMA_CH, &dma_init_struct);
//     dma_circulation_disable(SPI1_DMA, SPI1_TX_DMA_CH);
//     dma_memory_to_memory_disable(SPI1_DMA, SPI1_TX_DMA_CH);

//     dma_channel_enable(SPI1_DMA, SPI1_TX_DMA_CH);
//     spi_dma_enable(SPI1, SPI_DMA_TRANSMIT);

//     while (!dma_flag_get(SPI1_DMA, SPI1_TX_DMA_CH, DMA_FLAG_FTF));

//     SPI1_NSS_HIGH();
// }

// void DMA0_Channel1_IRQHandler(void)
// {
//     if (dma_flag_get(SPI1_DMA, SPI1_RX_DMA_CH, DMA_FLAG_FTF)) {
//         dma_flag_clear(SPI1_DMA, SPI1_RX_DMA_CH, DMA_FLAG_FTF);

//         if (spi_rx_callback) {
//             spi_rx_callback(m_spi_rx.data, m_spi_rx.length);
//         }
//     }
// }