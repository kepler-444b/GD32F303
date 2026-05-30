#include "app_watchdog.h"
#include "gd32f30x.h"

void app_watchdog_init(void)
{
    // 1. 启用 IRC40K 时钟（FWDGT固定的时钟源）
    rcu_osci_on(RCU_IRC40K);
    while (SUCCESS != rcu_osci_stab_wait(RCU_IRC40K)); // 等待时钟稳定

    // 2. 使能对 FWDGT 寄存器的写访问
    fwdgt_write_enable();

    // 3. 配置分频系数和重装载值 (40000Hz / 64 = 625Hz, 1875 / 625Hz = 3秒)
    fwdgt_config(1875, FWDGT_PSC_DIV64);

    // 4. 使能并启动看门狗
    fwdgt_enable();
}

void app_watchdog_feed(void)
{
    fwdgt_counter_reload(); // 3秒内调用一次此函数喂狗
}