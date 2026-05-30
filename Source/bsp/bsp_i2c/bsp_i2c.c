#include "bsp_i2c.h"
#include "../Source/bsp/bsp_pcb/bsp_pcb.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"

#define I2C_TIMEOUT 100000 // i2c 写超时

#define I2C_WAIT(cond)                        \
    {                                         \
        volatile uint32_t timeout = 100000;   \
        while (!(cond)) {                     \
            if (--timeout == 0) return false; \
        }                                     \
    }

void bsp_i2c_init(uint32_t i2c_periph, uint8_t i2c_addr)
{
    if (i2c_periph == I2C0) {
        pcb_i2c_init();
        i2c_clock_config(i2c_periph, 100000, I2C_DTCY_2);
        i2c_mode_addr_config(i2c_periph, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, 0x32);
        i2c_enable(i2c_periph);
        i2c_ack_config(i2c_periph, I2C_ACK_ENABLE);
    }
}

// 硬件要求：清除 ADDSEND 必须读 STAT0 再读 STAT1
static void i2c_clear_addr(uint32_t i2c_periph)
{
    (void)I2C_STAT0(i2c_periph);
    (void)I2C_STAT1(i2c_periph);
}
bool bsp_i2c_write(uint32_t i2c_periph, uint8_t i2c_addr, uint8_t reg, uint8_t data)
{
    I2C_WAIT(!i2c_flag_get(i2c_periph, I2C_FLAG_I2CBSY)); // 1. 等待总线空闲

    i2c_start_on_bus(i2c_periph); // 2. 起始位
    I2C_WAIT(i2c_flag_get(i2c_periph, I2C_FLAG_SBSEND));

    i2c_master_addressing(i2c_periph, i2c_addr, I2C_TRANSMITTER); // 3. 设备地址
    I2C_WAIT(i2c_flag_get(i2c_periph, I2C_FLAG_ADDSEND));
    i2c_clear_addr(i2c_periph);

    i2c_data_transmit(i2c_periph, reg); // 4. 寄存器地址
    I2C_WAIT(i2c_flag_get(i2c_periph, I2C_FLAG_TBE));

    i2c_data_transmit(i2c_periph, data); // 5. 数据内容
    I2C_WAIT(i2c_flag_get(i2c_periph, I2C_FLAG_TBE));

    i2c_stop_on_bus(i2c_periph); // 6. 停止位
    return true;
}

// 读寄存器：Start -> Addr(W) -> Reg -> Restart -> Addr(R) -> Stop -> Read -> Data
bool bsp_i2c_read(uint32_t i2c_periph, uint8_t i2c_addr, uint8_t reg, uint8_t *data)
{
    I2C_WAIT(!i2c_flag_get(i2c_periph, I2C_FLAG_I2CBSY));

    // 指定寄存器地址
    i2c_start_on_bus(i2c_periph);
    I2C_WAIT(i2c_flag_get(i2c_periph, I2C_FLAG_SBSEND));
    i2c_master_addressing(i2c_periph, i2c_addr, I2C_TRANSMITTER);
    I2C_WAIT(i2c_flag_get(i2c_periph, I2C_FLAG_ADDSEND));
    i2c_clear_addr(i2c_periph);
    i2c_data_transmit(i2c_periph, reg);
    I2C_WAIT(i2c_flag_get(i2c_periph, I2C_FLAG_TBE));

    // 读取该寄存器数据
    i2c_start_on_bus(i2c_periph); // 重复起始信号
    I2C_WAIT(i2c_flag_get(i2c_periph, I2C_FLAG_SBSEND));
    i2c_master_addressing(i2c_periph, i2c_addr, I2C_RECEIVER);
    I2C_WAIT(i2c_flag_get(i2c_periph, I2C_FLAG_ADDSEND));

    // 单字节读取关键时序：先关ACK -> 清地址位 -> 发STOP
    i2c_ack_config(i2c_periph, I2C_ACK_DISABLE);
    i2c_clear_addr(i2c_periph);
    i2c_stop_on_bus(i2c_periph);

    I2C_WAIT(i2c_flag_get(i2c_periph, I2C_FLAG_RBNE)); // 等待接收完成
    *data = i2c_data_receive(i2c_periph);

    i2c_ack_config(i2c_periph, I2C_ACK_ENABLE); // 恢复ACK供下次使用
    return true;
}