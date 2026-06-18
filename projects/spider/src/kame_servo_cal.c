#include <stdbool.h>
#include <stddef.h>

#include "kame_servo_cal.h"

const struct kame_servo_cal kame_servos[KAME_SERVO_COUNT] = {
	[KAME_SERVO_R1_KNEE] = {
		.label = "R1/knee",
		.ch = 14,
		.min_deg = 0,
		.max_deg = 180,
		.stand_deg = 140,
		.flags = KAME_SERVO_F_FRONT,
	},
	[KAME_SERVO_R1_HIP] = {
		.label = "R1/hip",
		.ch = 4,
		.min_deg = 50,
		.max_deg = 140,
		.stand_deg = 75,
		.flags = KAME_SERVO_F_HIP | KAME_SERVO_F_FRONT,
	},
	[KAME_SERVO_R2_HIP] = {
		.label = "R2/hip",
		.ch = 3,
		.min_deg = 80,
		.max_deg = 170,
		.stand_deg = 145,
		.flags = KAME_SERVO_F_HIP,
	},
	[KAME_SERVO_R2_KNEE] = {
		.label = "R2/knee",
		.ch = 13,
		.min_deg = 0,
		.max_deg = 180,
		.stand_deg = 40,
		.flags = KAME_SERVO_F_KNEE_0_BELLY,
	},
	[KAME_SERVO_L1_HIP] = {
		.label = "L1/hip",
		.ch = 1,
		.min_deg = 60,
		.max_deg = 150,
		.stand_deg = 130,
		.flags = KAME_SERVO_F_HIP_INC_BACKWARD | KAME_SERVO_F_HIP |
			 KAME_SERVO_F_FRONT,
	},
	[KAME_SERVO_L1_KNEE] = {
		.label = "L1/knee",
		.ch = 11,
		.min_deg = 0,
		.max_deg = 180,
		.stand_deg = 40,
		.flags = KAME_SERVO_F_KNEE_0_BELLY | KAME_SERVO_F_FRONT,
	},
	[KAME_SERVO_L2_HIP] = {
		.label = "L2/hip",
		.ch = 2,
		.min_deg = 50,
		.max_deg = 130,
		.stand_deg = 75,
		.flags = KAME_SERVO_F_HIP_INC_BACKWARD | KAME_SERVO_F_HIP,
	},
	[KAME_SERVO_L2_KNEE] = {
		.label = "L2/knee",
		.ch = 12,
		.min_deg = 0,
		.max_deg = 180,
		.stand_deg = 140,
		/*
		 * L2 膝舵机与 L1 镜像安装，方向与 R1 一致：0°=抬最高 / 180°=收腹下，
		 * 故不置 KNEE_0_BELLY。站立 140° 落在收腹下端，与其余三腿对称；
		 * 否则趴下/抬腿时方向相反（L2 拐错方向）。
		 */
		.flags = 0,
	},
};

const struct kame_servo_cal *kame_servo_get(kame_servo_id_t id)
{
	if ((unsigned int)id >= KAME_SERVO_COUNT) {
		return NULL;
	}

	return &kame_servos[id];
}

uint8_t kame_servo_clamp(kame_servo_id_t id, uint8_t deg)
{
	const struct kame_servo_cal *s = kame_servo_get(id);

	if (s == NULL) {
		return deg;
	}
	if (deg < s->min_deg) {
		return s->min_deg;
	}
	if (deg > s->max_deg) {
		return s->max_deg;
	}

	return deg;
}

uint8_t kame_servo_splay_deg(kame_servo_id_t id, uint8_t delta_deg)
{
	const struct kame_servo_cal *s = kame_servo_get(id);

	if (s == NULL) {
		return 0;
	}

	/* 腿(膝)关节维持站立标定，不参与叉腿 */
	if ((s->flags & KAME_SERVO_F_HIP) == 0U) {
		return s->stand_deg;
	}

	/* 前腿向前、后腿向后；再按该髋舵机的角度方向确定加减号 */
	bool move_forward = (s->flags & KAME_SERVO_F_FRONT) != 0U;
	bool inc_backward = (s->flags & KAME_SERVO_F_HIP_INC_BACKWARD) != 0U;
	bool increase = (move_forward != inc_backward);

	int target = (int)s->stand_deg + (increase ? (int)delta_deg
						    : -(int)delta_deg);

	if (target < s->min_deg) {
		target = s->min_deg;
	}
	if (target > s->max_deg) {
		target = s->max_deg;
	}

	return (uint8_t)target;
}

uint8_t kame_servo_knee_top_deg(kame_servo_id_t id, int delta_below)
{
	const struct kame_servo_cal *s = kame_servo_get(id);

	if (s == NULL) {
		return 0;
	}
	/* 仅对腿(膝)关节生效；髋关节原样返回站立角 */
	if ((s->flags & KAME_SERVO_F_HIP) != 0U) {
		return s->stand_deg;
	}

	if (delta_below < 0) {
		delta_below = 0;
	}

	/* 最高端：0°=腹下(KNEE_0_BELLY)的腿在 max_deg；R1/L2(0°=最高)在 min_deg。
	 * delta_below 表示自最高端向下(收爪)回落的角度。 */
	bool up_increase = (s->flags & KAME_SERVO_F_KNEE_0_BELLY) != 0U;
	int target = up_increase ? (int)s->max_deg - delta_below
				 : (int)s->min_deg + delta_below;

	if (target < s->min_deg) {
		target = s->min_deg;
	}
	if (target > s->max_deg) {
		target = s->max_deg;
	}

	return (uint8_t)target;
}

uint8_t kame_servo_hip_forward_deg(kame_servo_id_t id, int delta_forward)
{
	const struct kame_servo_cal *s = kame_servo_get(id);

	if (s == NULL) {
		return 0;
	}
	/* 仅对髋关节生效；腿关节原样返回站立角 */
	if ((s->flags & KAME_SERVO_F_HIP) == 0U) {
		return s->stand_deg;
	}

	/* 向前：R 侧角度增大、L 侧角度减小（由 INC_BACKWARD 标志区分） */
	bool inc_backward = (s->flags & KAME_SERVO_F_HIP_INC_BACKWARD) != 0U;
	int signed_delta = inc_backward ? -delta_forward : delta_forward;
	int target = (int)s->stand_deg + signed_delta;

	if (target < s->min_deg) {
		target = s->min_deg;
	}
	if (target > s->max_deg) {
		target = s->max_deg;
	}

	return (uint8_t)target;
}

uint8_t kame_servo_knee_up_deg(kame_servo_id_t id, int delta_up)
{
	const struct kame_servo_cal *s = kame_servo_get(id);

	if (s == NULL) {
		return 0;
	}
	/* 仅对腿(膝)关节生效；髋关节原样返回站立角 */
	if ((s->flags & KAME_SERVO_F_HIP) != 0U) {
		return s->stand_deg;
	}

	/* 抬高方向：0°=腹下(KNEE_0_BELLY) 的腿向 180° 抬；R1(0°=最高) 向 0° 抬 */
	bool up_increase = (s->flags & KAME_SERVO_F_KNEE_0_BELLY) != 0U;
	int signed_delta = up_increase ? delta_up : -delta_up;
	int target = (int)s->stand_deg + signed_delta;

	if (target < s->min_deg) {
		target = s->min_deg;
	}
	if (target > s->max_deg) {
		target = s->max_deg;
	}

	return (uint8_t)target;
}
