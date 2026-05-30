#ifndef _EVENTBUS_H_
#define _EVENTBUS_H_

// 事件类型定义(根据需求修改)
typedef enum {

    EVENT_MQTT_RECV_MSG,
    EVENT_USART2_RECV_MSG,
    // EVENT_USART0_RECV_MSG,
    EVENT_USART0_SET, // 上位机配置场景绑定
    EVENT_USART0_CFG, // 上位机配置设备信息

    EVENT_USER_LINK_ON, // 手动联网

    EVENT_PHY_LINK_ON,  // 网线已经插上
    EVENT_PHY_LINK_OFF, // 网线已经拔出

    EVENT_NETWOR_ON,  // 网络已经连接
    EVENT_NETWOR_OFF, // 网络已经断开

    EVENT_STANDARD_TIME, // 校准时间

    EVENT_MQTT_CONNECT,

    EVENT_REPORT_TEMP,     // 上报主板温度 
    EVENT_REPORT_GET_TIME, // 上报获取云端时间

    MQTT_DEL_BIND_CFG,  // 删除绑定信息
    MQTT_DEL_SCENE_CFG, // 删除场景信息

    MQTT_SWITCH_LED_MSG,

    // 添加更多事件类型...
    EVENT_COUNT // 自动计算事件数量
} event_type_e;

// 事件回调函数类型(带事件类型和参数)
typedef void (*EventHandler)(event_type_e event, void *params);

// 事件结构体(包含事件类型和参数)
typedef struct
{
    event_type_e type;
    void *params;
} event_t;

// 初始化事件总线
void app_eventbus_init(void);

// 订阅所有事件(只需调用一次)
void app_eventbus_subscribe(EventHandler handler);

// 发布事件(带参数)
void app_eventbus_publish(event_type_e event, void *params);

// 处理事件(在主循环中调用)
void app_eventbus_poll(void);
#endif