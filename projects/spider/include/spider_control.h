/*
 * spider 统一命令分发（Shell 与 BLE 汇合点）
 */

#ifndef SPIDER_CONTROL_H
#define SPIDER_CONTROL_H

#include <stddef.h>
#include <stdint.h>

#include "kame_motion.h"

int spider_control_motion(kame_motion_id_t id, kame_leg_id_t leg);
int spider_control_stop(void);

int spider_control_set_motion_delay_ms(uint32_t ms);
uint32_t spider_control_get_motion_delay_ms(void);

int spider_control_set_servo_speed_deg_s(uint32_t deg_s);
uint32_t spider_control_get_servo_speed_deg_s(void);

/** 解析并执行一帧 BLE 载荷；非法帧静默丢弃，成功返回 0 */
int spider_control_ble_frame(const uint8_t *data, size_t len);

#endif /* SPIDER_CONTROL_H */
