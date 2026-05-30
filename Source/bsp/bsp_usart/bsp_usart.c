
#include "bsp_usart.h"
#include "../Source/bsp/bsp_pcb/bsp_pcb.h"
#include "gd32f30x_dma.h"
#include "gd32f30x_gpio.h"
#include "systick.h"
#include <string.h>
#if defined RTT_ENABLE
#include "../Source/rtt/SEGGER_RTT.h"
#endif

#define USART2_DATA_ADDRESS ((uint32_t)&USART_DATA(USART2))

#define USART2_TX_DMA_CH    DMA_CH1
#define USART2_RX_DMA_CH    DMA_CH2

/* **************** 注册 USART0 回调函数 **************** */
static usart0_rx_buf_t rx0_buf = {0};

static usart_rx0_callback_t rx0_callback = NULL;
void bsp_usart0_rx_callback(usart_rx0_callback_t callback)
{
    rx0_callback = callback;
}

/* **************** 注册 USART1 回调函数 **************** */
static usart1_rx_buf_t rx1_buf = {0};
static usart1_tx_buf_t tx1_buf = {0};

static usart_rx1_callback_t rx1_callback = NULL;
void bsp_usart1_rx_callback(usart_rx1_callback_t callback)
{
    rx1_callback = callback;
}

/* **************** 注册 USART2 回调函数 **************** */
static usart2_rx_buf_t rx2_buf = {0};
static usart2_tx_buf_t tx2_buf = {0};

static usart_rx2_callback_t rx2_callback = NULL;
void bsp_usart2_rx_callback(usart_rx2_callback_t callback)
{
    rx2_callback = callback;
}

/* **************** 注册 UART3 回调函数 **************** */
static usart3_rx_buf_t rx3_buf = {0};

static usart_rx3_callback_t rx3_callback = NULL;
void bsp_usart3_rx_callback(usart_rx3_callback_t callback)
{
    rx3_callback = callback;
}

void bsp_usart_init(uint32_t usart_com, uint32_t baudrate)
{
    if (usart_com == USART0) { // type-c

        pcb_usart0_init();
        usart_deinit(usart_com);
        usart_baudrate_set(usart_com, baudrate);
        usart_word_length_set(usart_com, USART_WL_8BIT);
        usart_stop_bit_set(usart_com, USART_STB_1BIT);
        usart_parity_config(usart_com, USART_PM_NONE);
        usart_hardware_flow_rts_config(usart_com, USART_RTS_DISABLE);
        usart_hardware_flow_cts_config(usart_com, USART_CTS_DISABLE);
        usart_transmit_config(usart_com, USART_TRANSMIT_ENABLE);
        usart_receive_config(usart_com, USART_RECEIVE_ENABLE);
        usart_enable(usart_com);

        usart_interrupt_enable(usart_com, USART_INT_IDLE); // USART 空闲中断,用于触发回调函数
        usart_interrupt_enable(usart_com, USART_INT_RBNE); // USART 非空中断
        nvic_irq_enable(USART0_IRQn, 0, 1);
    }

    if (usart_com == USART1) { // 串口屏

        pcb_usart1_init();

        usart_deinit(usart_com);
        usart_baudrate_set(usart_com, baudrate);
        usart_word_length_set(usart_com, USART_WL_8BIT);
        usart_stop_bit_set(usart_com, USART_STB_1BIT);
        usart_parity_config(usart_com, USART_PM_NONE);
        usart_hardware_flow_rts_config(usart_com, USART_RTS_DISABLE);
        usart_hardware_flow_cts_config(usart_com, USART_CTS_DISABLE);
        usart_transmit_config(usart_com, USART_TRANSMIT_ENABLE);
        usart_receive_config(usart_com, USART_RECEIVE_ENABLE);
        usart_enable(usart_com);

        usart_interrupt_enable(usart_com, USART_INT_IDLE); // USART 空闲中断,用于触发回调函数
        usart_interrupt_enable(usart_com, USART_INT_RBNE); // USART 非空中断
        nvic_irq_enable(USART1_IRQn, 0, 1);
    }

    if (usart_com == USART2) { // 用于面板通讯
        pcb_usart2_init();

        RD2_SET_L; // 拉低 485使能脚(接收模式)
        rcu_periph_clock_enable(RCU_DMA0);
        rcu_periph_clock_enable(RCU_DMA1);

        // USART2 配置
        usart_deinit(usart_com);
        usart_baudrate_set(usart_com, baudrate);
        usart_word_length_set(usart_com, USART_WL_8BIT);
        usart_stop_bit_set(usart_com, USART_STB_1BIT);
        usart_parity_config(usart_com, USART_PM_NONE);
        usart_hardware_flow_rts_config(usart_com, USART_RTS_DISABLE);
        usart_hardware_flow_cts_config(usart_com, USART_CTS_DISABLE);
        usart_transmit_config(usart_com, USART_TRANSMIT_ENABLE);
        usart_receive_config(usart_com, USART_RECEIVE_ENABLE);
        usart_dma_receive_config(usart_com, USART_RECEIVE_DMA_ENABLE);
        usart_enable(usart_com);
        usart_interrupt_enable(usart_com, USART_INT_IDLE); // USART 空闲中断,用于 DMA RX
        usart_interrupt_enable(usart_com, USART_INT_TC);   // USART 发送完成标志位
        nvic_irq_enable(USART2_IRQn, 0, 1);

        dma_parameter_struct dma_init_struct;
        // DMA RX 初始化
        dma_deinit(DMA0, USART2_RX_DMA_CH);
        dma_struct_para_init(&dma_init_struct);

        dma_init_struct.direction    = DMA_PERIPHERAL_TO_MEMORY;
        dma_init_struct.memory_addr  = (uint32_t)rx2_buf.buffer;
        dma_init_struct.memory_inc   = DMA_MEMORY_INCREASE_ENABLE;
        dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
        dma_init_struct.number       = (uint32_t)USART2_RECV_SIZE;
        dma_init_struct.periph_addr  = USART2_DATA_ADDRESS;
        dma_init_struct.periph_inc   = DMA_PERIPH_INCREASE_DISABLE;
        dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
        dma_init_struct.priority     = DMA_PRIORITY_HIGH;

        dma_init(DMA0, USART2_RX_DMA_CH, &dma_init_struct);
        dma_circulation_disable(DMA0, USART2_RX_DMA_CH);
        dma_memory_to_memory_disable(DMA0, USART2_RX_DMA_CH);
        dma_channel_enable(DMA0, USART2_RX_DMA_CH);

        // DMA TX 初始化
        dma_deinit(DMA0, USART2_TX_DMA_CH);
        dma_struct_para_init(&dma_init_struct);

        dma_init_struct.direction    = DMA_MEMORY_TO_PERIPHERAL;
        dma_init_struct.memory_addr  = 0;
        dma_init_struct.memory_inc   = DMA_MEMORY_INCREASE_ENABLE;
        dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
        dma_init_struct.number       = 0;
        dma_init_struct.periph_addr  = USART2_DATA_ADDRESS;
        dma_init_struct.periph_inc   = DMA_PERIPH_INCREASE_DISABLE;
        dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
        dma_init_struct.priority     = DMA_PRIORITY_HIGH;

        dma_init(DMA0, USART2_TX_DMA_CH, &dma_init_struct);
        dma_circulation_disable(DMA0, USART2_TX_DMA_CH);
        dma_memory_to_memory_disable(DMA0, USART2_TX_DMA_CH);
        dma_channel_enable(DMA0, USART2_TX_DMA_CH);
    }

    if (usart_com == UART3) { // 用于扩展通讯

        pcb_uart3_init();

        usart_deinit(usart_com);
        usart_baudrate_set(usart_com, baudrate);
        usart_word_length_set(usart_com, USART_WL_8BIT);
        usart_stop_bit_set(usart_com, USART_STB_1BIT);
        usart_parity_config(usart_com, USART_PM_NONE);
        usart_hardware_flow_rts_config(usart_com, USART_RTS_DISABLE);
        usart_hardware_flow_cts_config(usart_com, USART_CTS_DISABLE);
        usart_transmit_config(usart_com, USART_TRANSMIT_ENABLE);
        usart_receive_config(usart_com, USART_RECEIVE_ENABLE);
        usart_enable(usart_com);

        usart_interrupt_enable(usart_com, USART_INT_IDLE); // USART 空闲中断,用于触发回调函数
        usart_interrupt_enable(usart_com, USART_INT_TC);   // USART 发送完成标志位
        nvic_irq_enable(UART3_IRQn, 0, 1);
    }

    if (usart_com == UART4) { // 调试串口

        pcb_uart4_init();

        usart_deinit(usart_com);
        usart_baudrate_set(usart_com, baudrate);
        usart_word_length_set(usart_com, USART_WL_8BIT);
        usart_stop_bit_set(usart_com, USART_STB_1BIT);
        usart_parity_config(usart_com, USART_PM_NONE);
        usart_hardware_flow_rts_config(usart_com, USART_RTS_DISABLE);
        usart_hardware_flow_cts_config(usart_com, USART_CTS_DISABLE);
        usart_transmit_config(usart_com, USART_TRANSMIT_ENABLE);
        usart_receive_config(usart_com, USART_RECEIVE_ENABLE);
        usart_enable(usart_com);
    }
}

// USART0 (type-c 通讯,采用阻塞发送)
static void USART0_send_data(const uint8_t *data, uint16_t len)
{
    if (!data || len == 0) return;

    for (uint16_t i = 0; i < len; i++) {
        while (RESET == usart_flag_get(USART0, USART_FLAG_TBE));
        usart_data_transmit(USART0, data[i]);
    }
}

// USART1 (串口屏,采用阻塞发送)
static void USART1_send_data(const uint8_t *data, uint16_t len)
{
    if (!data || len == 0) return;

    for (uint16_t i = 0; i < len; i++) {
        while (RESET == usart_flag_get(USART1, USART_FLAG_TBE));
        usart_data_transmit(USART1, data[i]);
    }
}

// UART2 (面板通讯,采用DMA发送)
static void USART2_dma_send_data(const uint8_t *data, uint16_t len)
{
    if (!data || len == 0) {
        return;
    }

    while (BSP_GET_GPIO(PE15) == true); // 等待使能脚被拉低
    tx2_buf.tx_ptr = data;
    tx2_buf.tx_len = len;

    usart_receive_config(USART2, USART_RECEIVE_DISABLE); // 禁用接收器
    usart_interrupt_disable(USART2, USART_INT_IDLE);     // 禁用空闲中断

    RD2_SET_H; // 拉高,准备发送

    dma_channel_disable(DMA0, USART2_TX_DMA_CH);
    dma_memory_address_config(DMA0, USART2_TX_DMA_CH, (uint32_t)tx2_buf.tx_ptr);
    dma_transfer_number_config(DMA0, USART2_TX_DMA_CH, tx2_buf.tx_len);

    usart_dma_transmit_config(USART2, USART_TRANSMIT_DMA_ENABLE);
    dma_channel_enable(DMA0, USART2_TX_DMA_CH);
}

// UART3 (扩展通讯,采用阻塞发送)
static void UART3_send_data(const uint8_t *data, uint16_t len)
{
    if (!data || len == 0) return;
    while (BSP_GET_GPIO(PD1) == true);

    RD3_SET_H; // 拉高,准备发送
    for (uint16_t i = 0; i < len; i++) {
        while (RESET == usart_flag_get(UART3, USART_FLAG_TBE));
        usart_data_transmit(UART3, data[i]);
    }
}

void bsp_usart_tx_buf(const uint8_t *data, uint16_t length, uint32_t usart_periph)
{
    switch (usart_periph) {
        case USART0: // type-c
            USART0_send_data(data, length);
            break;
        case USART1: // 串口屏
            USART1_send_data(data, length);
            break;
        case USART2: // 面板通讯
            USART2_dma_send_data(data, length);
            break;
        case UART3: // 扩展通讯
            UART3_send_data(data, length);
            break;
        case UART4:
            // UART4_send_data(data, length);
            break;
        default:
            return;
    }
}

/* ******************************************** 中断服务函数 ******************************************** */

// USART0 中断服务函数
void USART0_IRQHandler(void)
{
    if (usart_interrupt_flag_get(USART0, USART_INT_FLAG_RBNE)) { // 非空中断

        uint8_t byte = (uint8_t)usart_data_receive(USART0);
        if (rx0_buf.length < USART0_RECV_SIZE) {
            rx0_buf.buffer[rx0_buf.length++] = byte;
        }
    }

    if (usart_interrupt_flag_get(USART0, USART_INT_FLAG_IDLE) != RESET) { // 空闲中断
        usart_data_receive(USART0);                                       // 先读 USART0 DR 寄存器以清除 IDLE 标志位
        if (rx0_callback && rx0_buf.length > 0) {
            rx0_callback(&rx0_buf);
        }
        rx0_buf.length = 0;
    }
}

// USART1 中断服务函数
void USART1_IRQHandler(void)
{
    if (usart_interrupt_flag_get(USART1, USART_INT_FLAG_RBNE)) { // 非空中断

        uint8_t byte = (uint8_t)usart_data_receive(USART1);
        if (rx1_buf.length < USART0_RECV_SIZE) {
            rx1_buf.buffer[rx1_buf.length++] = byte;
        }
    }

    if (usart_interrupt_flag_get(USART1, USART_INT_FLAG_IDLE) != RESET) { // 空闲中断

        usart_data_receive(USART1); // 先读 USART1 DR 寄存器以清除 IDLE 标志位
        if (rx1_callback && rx1_buf.length > 0) {
            rx1_callback(&rx1_buf);
        }
        rx1_buf.length = 0;
    }
}

// USART2 中断服务函数
void USART2_IRQHandler(void)
{
    if (usart_interrupt_flag_get(USART2, USART_INT_FLAG_IDLE) != RESET) { // 空闲中断

        usart_data_receive(USART2); // 先读 USART2 DR 寄存器以清除 IDLE 标志位

        uint16_t recv_length = USART2_RECV_SIZE - dma_transfer_number_get(DMA0, USART2_RX_DMA_CH); //  DMA + 空闲中断 来实现接收数据
        if (recv_length > USART2_RECV_SIZE) {
            recv_length = USART2_RECV_SIZE;
        }
        rx2_buf.length = recv_length;
        if (rx2_buf.length > 0 && rx2_callback) {
            rx2_callback(&rx2_buf);
        }

        // dma_channel_disable(DMA0, USART2_RX_DMA_CH);
        // dma_interrupt_flag_clear(DMA0, USART2_RX_DMA_CH, DMA_INT_FLAG_G);
        // dma_transfer_number_config(DMA0, USART2_RX_DMA_CH, USART2_RECV_SIZE);
        // // dma_memory_address_config(DMA0, USART2_RX_DMA_CH, (uint32_t)rx2_buf.buffer);
        // dma_channel_enable(DMA0, USART2_RX_DMA_CH);

        dma_channel_disable(DMA0, USART2_RX_DMA_CH);
        dma_interrupt_flag_clear(DMA0, USART2_RX_DMA_CH, DMA_INT_FLAG_G);
        dma_transfer_number_config(DMA0, USART2_RX_DMA_CH, USART2_RECV_SIZE);
        dma_channel_enable(DMA0, USART2_RX_DMA_CH);
    }

    if (usart_interrupt_flag_get(USART2, USART_INT_FLAG_TC) != RESET) { // 发送完成中断
        usart_flag_clear(USART2, USART_FLAG_TC);

        RD2_SET_L;
        usart_receive_config(USART2, USART_RECEIVE_ENABLE); // 重新启用接收器
        usart_interrupt_enable(USART2, USART_INT_IDLE);     // 重新启用空闲中断
    }
}

// UART3 中断服务函数
void UART3_IRQHandler(void)
{
    if (usart_interrupt_flag_get(UART3, USART_INT_FLAG_IDLE) != RESET) {

        usart_data_receive(UART3); // 先读 UART3 DR 寄存器以清除 IDLE 标志位
        if (rx3_callback && rx3_buf.length > 0) {
            rx3_callback(&rx3_buf);
        }
        rx3_buf.length = 0;
    }

    if (usart_interrupt_flag_get(UART3, USART_INT_FLAG_RBNE)) { // 非空中断
        uint8_t byte = (uint8_t)usart_data_receive(UART3);
        if (rx3_buf.length < UART3_RECV_SIZE) {
            rx3_buf.buffer[rx3_buf.length++] = byte;
        }
    }

    if (usart_interrupt_flag_get(UART3, USART_INT_FLAG_TC) != RESET) { // 发送完成中断
        usart_flag_clear(UART3, USART_FLAG_TC);
        RD3_SET_L;
    }
}

int fputc(int ch, FILE *f)
{
#if defined RTT_ENABLE
    SEGGER_RTT_PutChar(0, ch);
#else
    usart_data_transmit(UART4, (uint8_t)ch);
    while (RESET == usart_flag_get(UART4, USART_FLAG_TBE)) {
        ;
    }
#endif
    return ch;
}
