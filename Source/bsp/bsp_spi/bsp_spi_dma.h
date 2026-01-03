// #ifndef _BSP_SPI_DMA_H_
// #define _BSP_SPI_DMA_H_
// #include <stdint.h>

// #define SPI_RX_BUFFER 512

// typedef struct
// {
//     uint8_t data[SPI_RX_BUFFER];
//     uint16_t length;
// } spi_rx_t;

// typedef void (*spi_rx_callback_t)(uint8_t *data, uint16_t length);
// void bsp_spi_register_rx_callback(spi_rx_callback_t callback);

// void bsp_spi_init(void);
// void spi_dma_tx(uint8_t *tx_buf, uint16_t length);
// #endif