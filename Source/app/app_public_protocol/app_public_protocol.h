#ifndef _APP_PANEL_PROTOCOL_H_
#define _APP_PANEL_PROTOCOL_H_
#include <stdint.h>
#include <stdbool.h>
#include "../Source/app/app_public_config/app_public_config.h"
#include "../Source/cjson/cJSON.h"

typedef enum {
    // 按键功能
    ALL_CLOSE     = 0x00, // 总关
    ALL_ON_OFF    = 0x01, // 总开关
    CLEAN_ROOM    = 0x02, // 清理模式
    DND_MODE      = 0x03, // 勿扰模式
    LATER_MODE    = 0x04, // 请稍后
    CHECK_OUT     = 0x05, // 退房
    SOS_MODE      = 0x06, // 紧急呼叫
    SERVICE       = 0x07, // 请求服务
    CURTAIN_OPEN  = 0x10, // 窗帘开
    CURTAIN_CLOSE = 0x11, // 窗帘关
    SCENE_MODE    = 0x0D, // 场景模式
    LIGHT_MODE    = 0x0E, // 灯控模式
    NIGHT_LIGHT   = 0x0F, // 夜灯模式
} panel_type_e;

/* *************************************************************************************************************** */

// 面板上报数据帧相关宏定义
#define PANEL_FRAME_RX_HEAD     0xAA // 固定帧头
#define PANEL_FRAME_RX_DATA_LEN 6    // 面板类上报有效数据长度固定为6
#define PANEL_FRAME_RX_MAX_LEN  24

// 主机下发到面板的数据帧相关宏定义
#define PANEL_FRAME_TX_HEAD_1 0xFF // 固定帧头
#define PANEL_FRAME_TX_HEAD_2 0xAA // 固定帧头
#define PANEL_FRAME_TX_TYPE   0x01 // 报文类型:暂固定为 0x01

// 面板上报的信息
typedef struct
{
    uint8_t src_addr; // 面板地址
    uint8_t level;    // 触发类型
    uint8_t status;   // 面板状态(上报面板状态)
    uint8_t key_num;  // 按键编号(上报按键编号)
    bool key_status;  // 按键状态(上报按键状态)
    uint8_t reserve_1;
    uint8_t reserve_2;
} panel_src_info_t;

// 子帧设备的状态信息
typedef struct {
    uint8_t addr[8];
    uint8_t bl;
    uint8_t reserve1;
    uint8_t reserve2;
} panel_sub_status_t;

// 所有面板设备的状态表
typedef struct
{
    panel_sub_status_t sub_frame[PANEL_DEV_MAX / 8];
} panel_full_status_t;

// 面板发送帧
typedef struct {
    uint8_t fh_1;
    uint8_t fh_2;
    uint8_t length;
    uint8_t type;
    panel_full_status_t full_frame;
    uint8_t crc;
    uint8_t ft_1;
    uint8_t ft_2;
} panel_tx_buf_t;

/* *************************************************************************************************************** */

// 主机下发到扩展设备的数据帧相关宏定义
#define EXTEND_FRAME_TX_HEAD 0xFB
#define EXTEND_FRAME_TX_TYPE 0x01

// 扩展设备的状态表
typedef struct
{
    uint8_t relay_sel_1[6];
    uint8_t relay_sel_2[3];
    uint8_t tg_value[4]; // 4路可控硅调光

    uint8_t led_sel_1[4];  // 4 路LED PWM调光(1~4)
    uint8_t led_sel_2[12]; // 12路LED PWM调光(5~16)
    uint8_t led_sel_3[8];  // 8 路LED PWM调光(17~24)
    uint8_t led_sel_4[8];  // 8 路LED PWM调光(25~32)

    uint8_t air_dev[3]; // 空调模块

} extend_status_t;

// 主机发送给扩展的数据帧 https://docs.qq.com/doc/DZGNSRE5lblJVcmVU
typedef struct
{
    uint8_t fh;
    uint8_t type;
    uint8_t relay_sel_1[6];
    uint8_t tg_value[4];
    uint8_t crc_1;
    uint8_t led_sel_1[4];
    uint8_t reserve;
    uint8_t crc_2;
    uint8_t led_sel_2[12];
    uint8_t crc_3;
    uint8_t led_sel_3[8];
    uint8_t air_dev[3];
    uint8_t relay_sel_2[3];
    uint8_t led_sel_4[8];

} extend_tx_buf_t;

void app_public_protocol_init(void);

#endif