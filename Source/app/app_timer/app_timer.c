#include "app_timer.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"
#include "gd32f30x_timer.h"
#include <stdio.h>

#define SYSTEM_CLOCK_FREQ 120000000 ///< 系统时钟频率
#define TIMER_PERIOD      999       ///< 定时器周期(1ms中断)

static soft_timer_t my_soft_timer[MAX_SOFT_TIMERS]; ///< 软定时器数组
static volatile uint32_t system_ticks = 0;          ///< 系统基准时间(毫秒),定时时间不得超过49.7天

// 查找空闲定时器
static int find_free_timer(void)
{
    for (int i = 0; i < MAX_SOFT_TIMERS; i++) {
        if (my_soft_timer[i].state == TIMER_STATE_INACTIVE) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 通过名称查找定时器
 * @param name 定时器名称
 * @return 定时器索引, -1表示未找到
 */
static int find_timer_by_name(const char *name)
{
    if (name == NULL || strlen(name) == 0) {
        return -1;
    }
    for (int i = 0; i < MAX_SOFT_TIMERS; i++) {
        if (my_soft_timer[i].state != TIMER_STATE_INACTIVE &&
            strcmp(my_soft_timer[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

void app_timer_init(void)
{
    // 初始化所有定时器状态
    for (int i = 0; i < MAX_SOFT_TIMERS; i++) {
        my_soft_timer[i].state   = TIMER_STATE_INACTIVE;
        my_soft_timer[i].name[0] = '\0';
    }
    timer_parameter_struct timer_initpara;
    rcu_periph_clock_enable(RCU_TIMER6);
    timer_deinit(TIMER6);
    // 配置定时器为1ms中断
    timer_initpara.prescaler        = (SYSTEM_CLOCK_FREQ / 1000000) - 1; // 分频到1KHz
    timer_initpara.alignedmode      = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection = TIMER_COUNTER_UP;
    timer_initpara.period           = TIMER_PERIOD; // 1000次计数 = 1ms
    timer_initpara.clockdivision    = TIMER_CKDIV_DIV1;
    timer_init(TIMER6, &timer_initpara);

    // 使能中断
    timer_interrupt_flag_clear(TIMER6, TIMER_INT_FLAG_UP);
    timer_interrupt_enable(TIMER6, TIMER_INT_UP);
    timer_enable(TIMER6);
    nvic_irq_enable(TIMER6_IRQn, 0, 1); // 配置NVIC,优先级1
    APP_PRINTF("app_timer_init\n");
}

void TIMER6_IRQHandler(void)
{
    if (timer_interrupt_flag_get(TIMER6, TIMER_INT_FLAG_UP)) {
        timer_interrupt_flag_clear(TIMER6, TIMER_INT_FLAG_UP);
        system_ticks++;

        // 遍历定时器，标记到期事件
        for (uint8_t i = 0; i < MAX_SOFT_TIMERS; i++) {
            if (my_soft_timer[i].state != TIMER_STATE_ACTIVE)
                continue;

            uint32_t elapsed = system_ticks - my_soft_timer[i].start_time; // 计算已经过去的时间
            if (elapsed >= my_soft_timer[i].interval_ms) {                 // 如果已经超过定时时间间隔
                if (my_soft_timer[i].state != TIMER_STATE_PENDING) {
                    my_soft_timer[i].state = TIMER_STATE_PENDING; // 标记为TIMER_STATE_PENDING(待执行)
                }
            }
        }
    }
}

timer_error_e app_timer_start(uint32_t interval_ms, SoftTimerCallback callback, bool repeat, void *arg, const char *name)
{
    // 参数检查
    if (!callback || interval_ms == 0) {
        return TIMER_ERR_INVALID_PARAM;
    }
    // 检查名称长度
    if (name && strlen(name) >= MAX_TIMER_NAME_LEN) {
        return TIMER_ERR_NAME_TOO_LONG;
    }

    int id = find_free_timer();
    if (id >= 0) {
        my_soft_timer[id] = (soft_timer_t){
            .state       = TIMER_STATE_ACTIVE,
            .repeat      = repeat,
            .start_time  = system_ticks,
            .interval_ms = interval_ms,
            .callback    = callback,
            .user_arg    = arg};

        // 设置定时器名称
        if (name && name[0] != '\0') {
            strncpy(my_soft_timer[id].name, name, MAX_TIMER_NAME_LEN - 1);
            my_soft_timer[id].name[MAX_TIMER_NAME_LEN - 1] = '\0';
        } else {
            my_soft_timer[id].name[0] = '\0';
        }
    }

    return (id >= 0) ? TIMER_ERR_SUCCESS : TIMER_ERR_NO_RESOURCE;
}

timer_error_e app_timer_stop(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return TIMER_ERR_INVALID_PARAM;
    }

    int id = find_timer_by_name(name);
    if (id >= 0) {
        my_soft_timer[id].state = TIMER_STATE_INACTIVE;
        return TIMER_ERR_SUCCESS;
    }
    return TIMER_ERR_NOT_FOUND;
}

bool app_timer_is_active(const char *name)
{
    int id = find_timer_by_name(name);
    return (id >= 0) ? (my_soft_timer[id].state == TIMER_STATE_ACTIVE) : false;
}

void app_timer_poll(void)
{
    for (int i = 0; i < MAX_SOFT_TIMERS; i++) {
        if (my_soft_timer[i].state == TIMER_STATE_PENDING) {
            // 执行回调
            if (my_soft_timer[i].callback) {
                my_soft_timer[i].callback(my_soft_timer[i].user_arg);
            }

            // 更新状态和 start_time
            if (my_soft_timer[i].repeat) {
                my_soft_timer[i].start_time = system_ticks; // 在 poll 中更新 start_time
                my_soft_timer[i].state      = TIMER_STATE_ACTIVE;
            } else {
                my_soft_timer[i].state = TIMER_STATE_INACTIVE;
            }
        }
    }
}

uint32_t app_timer_get_ticks(void)
{
    return system_ticks;
}
