#ifndef _APP_PANEL_PROTOCOL_H_
#define _APP_PANEL_PROTOCOL_H_
#include <stdint.h>
#include <stdbool.h>
#include "../Source/app/app_public_config/app_public_config.h"
#include "../Source/cjson/cJSON.h"
#include "../Source/app/app_public_protocol/app_public.h"

/* *************************************************************************************************************** */

// 面板上报数据帧相关宏定义
#define PANEL_FRAME_RX_HEAD     0xAA // 固定帧头
#define PANEL_FRAME_RX_DATA_LEN 6    // 面板类上报有效数据长度固定为6
#define PANEL_FRAME_RX_MAX_LEN  24

// 主机下发到面板的数据帧相关宏定义
#define PANEL_FRAME_TX_HEAD_1 0xFF // 固定帧头
#define PANEL_FRAME_TX_HEAD_2 0xAA // 固定帧头
#define PANEL_FRAME_TX_TYPE   0x01 // 报文类型:暂固定为 0x01

#define SET_EXTEND_MAX        136 // 设置扩展状态数据长度

typedef enum {
    KEY  = 0x01, // 按键数据
    KNOB = 0x02, // 旋钮数据
} panel_type;

// 面板上报的信息
typedef struct
{
    uint8_t type;     // 报文类型(按键:0x01,旋钮:0x02)
    uint8_t src_addr; // 面板地址
    uint8_t level;    // 触发类型
    uint8_t status;   // 面板状态(上报面板状态)
    uint8_t key_num;  // 按键编号(上报按键编号)
    bool key_status;  // 按键状态(上报按键状态)
    uint8_t reserve;  // 保留值(旋钮值)
    uint8_t reserve_1;
    uint8_t reserve_2;
} panel_src_info_t;

typedef struct {
    uint8_t status;
    uint8_t reserve;
} panel_sub_idx;

// 子帧设备的状态信息
typedef struct {
    panel_sub_idx idx[8];
    uint8_t bl;
    uint8_t reserve1;
    uint8_t reserve2;
} panel_sub_status_t;

// 所有面板设备的状态表
typedef struct
{
    panel_sub_status_t sub_frame[PANEL_DEV_MAX / 8];
} panel_full_status_t;

typedef struct {

    uint8_t key_status[PANEL_DEV_MAX];  // 32个面板状态
    uint8_t key_reserve[PANEL_DEV_MAX]; // 32个面板保留
} panel_all_status_t;

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

void app_public_protocol_init(void);
void app_public_scene_cfg_parse(const char *str);
void app_public_timer_task_cfg_parse(const char *str);
void app_public_bind_scene_cfg_parse(const char *str);
void app_public_bind_group_cfg_parse(const char *str);
void app_puublic_set_extend(const char *str);
void app_public_exe_scene(const char *str);
void app_public_del_cfg(const char *str);

#endif