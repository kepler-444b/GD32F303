#ifndef _APP_PUBLIC_H_
#define _APP_PUBLIC_H_

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

} extend_all_status_t;

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

// 获取扩展状态表
extend_all_status_t *app_public_get_extend(void);

void app_display_scene_icon(uint8_t scene);
void app_display_exe_scene(uint8_t scene_id);
void app_display_resource_icon(void);
uint8_t *app_get_led_by_num(extend_all_status_t *status, uint8_t index);

#endif