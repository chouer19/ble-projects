/*
 * spider 舵机后端：PCA9685 插值引擎 + 全局转速
 */

#ifndef SPIDER_SERVO_H
#define SPIDER_SERVO_H

#include <stdbool.h>
#include <stdint.h>

#include "kame_pose.h"

#define SPIDER_SERVO_CHANNEL_CNT       16U
#define SPIDER_SERVO_SPEED_DEFAULT_DEG_S 240U

struct spider_servo_chan_state {
	bool active;
	uint16_t cur_us;
	uint16_t tgt_us;
	bool moving;
};

uint32_t spider_servo_get_speed_deg_s(void);
void spider_servo_set_speed_deg_s(uint32_t deg_s);
uint16_t spider_servo_speed_to_step(void);

uint32_t spider_servo_deg_to_us(uint32_t deg);
uint32_t spider_servo_us_to_deg(uint32_t us);

int spider_servo_apply_us(uint32_t ch, uint32_t pulse_us, uint16_t step_us);

/** 手动命令路径：先 kame_motion_stop()，再设置目标 */
int spider_servo_set_target(uint32_t ch, uint32_t pulse_us, uint16_t step_us);

int spider_servo_off(uint32_t ch);

bool spider_servo_is_ready(void);
bool spider_servo_get_chan(uint32_t ch, struct spider_servo_chan_state *out);

#endif /* SPIDER_SERVO_H */
