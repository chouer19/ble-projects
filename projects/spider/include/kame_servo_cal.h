/*
 * Kame 四足机器人 — 8 路舵机标定参数
 *
 * 机器可读的配置源；说明文档见 docs/kame-servo-calibration.md
 * PCA9685 通道映射见 kame_servo_cal.c 中各条目的 ch 字段。
 */

#ifndef KAME_SERVO_CAL_H
#define KAME_SERVO_CAL_H

#include <stdint.h>

#define KAME_SERVO_COUNT 8U

/** 未分配 PCA9685 通道时的占位值 */
#define KAME_SERVO_CH_UNASSIGNED 0xFFU

/**
 * 腿关节：0° 端点语义（R1 与其余三条腿相反）
 * 髋关节：角度增大时的行进方向（R 侧向前，L 侧向后）
 * 关节类型 / 腿位：供姿态生成（站立、叉腿）按前后腿、髋膝区分处理
 */
#define KAME_SERVO_F_KNEE_0_BELLY     (1U << 0)
#define KAME_SERVO_F_HIP_INC_BACKWARD (1U << 1)
#define KAME_SERVO_F_HIP             (1U << 2) /**< 置位=髋关节，否则为腿(膝)关节 */
#define KAME_SERVO_F_FRONT           (1U << 3) /**< 置位=前腿(R1/L1)，否则后腿(R2/L2) */

typedef enum {
	KAME_SERVO_R1_KNEE = 0,
	KAME_SERVO_R1_HIP,
	KAME_SERVO_R2_HIP,
	KAME_SERVO_R2_KNEE,
	KAME_SERVO_L1_HIP,
	KAME_SERVO_L1_KNEE,
	KAME_SERVO_L2_HIP,
	KAME_SERVO_L2_KNEE,
} kame_servo_id_t;

struct kame_servo_cal {
	const char *label; /**< 日志/调试用，如 "R1/knee" */
	uint8_t ch;        /**< PCA9685 通道 0~15 */
	uint8_t min_deg;
	uint8_t max_deg;
	uint8_t stand_deg;
	uint8_t flags;
};

extern const struct kame_servo_cal kame_servos[KAME_SERVO_COUNT];

/** 按 id 取标定条目，id 非法时返回 NULL */
const struct kame_servo_cal *kame_servo_get(kame_servo_id_t id);

/** 将角度钳位到该舵机有效范围 */
uint8_t kame_servo_clamp(kame_servo_id_t id, uint8_t deg);

/**
 * 叉腿站立姿态角度：腿关节保持站立标定值不变；
 * 髋关节在站立值基础上，前腿(R1/L1)向前、后腿(R2/L2)向后偏移 delta_deg，
 * 自动按各舵机方向（KAME_SERVO_F_HIP_INC_BACKWARD）取号并钳位到有效范围。
 */
uint8_t kame_servo_splay_deg(kame_servo_id_t id, uint8_t delta_deg);

/**
 * 髋关节定向偏移：以站立标定角为基准，向「爪子向前」方向偏移 delta_forward 度
 * （可为负，负值即向后）。按各髋舵机的角度方向（KAME_SERVO_F_HIP_INC_BACKWARD）
 * 自动取号并钳位。传入非髋关节（腿关节）时原样返回其站立标定角。
 */
uint8_t kame_servo_hip_forward_deg(kame_servo_id_t id, int delta_forward);

/**
 * 腿(膝)关节定向偏移：以站立标定角为基准，向「抬爪最高」方向偏移 delta_up 度
 * （可为负，负值即向收爪/腹下方向）。R1 膝(0°=最高)与其余三腿(180°=最高)方向相反，
 * 由 KAME_SERVO_F_KNEE_0_BELLY 自动取号并钳位。传入非腿关节（髋）时原样返回站立角。
 */
uint8_t kame_servo_knee_up_deg(kame_servo_id_t id, int delta_up);

/**
 * 腿(膝)关节贴近「最高点」：以该膝舵机能抬到的最高物理端为基准，
 * 向下回落 delta_below 度（delta_below>=0）。最高端按 KAME_SERVO_F_KNEE_0_BELLY
 * 自动判定：0°=腹下的腿(R2/L1)最高端=max_deg，0°=最高的腿(R1/L2)最高端=min_deg。
 * 结果自动钳位到有效范围。传入非腿关节（髋）时原样返回站立角。
 */
uint8_t kame_servo_knee_top_deg(kame_servo_id_t id, int delta_below);

#endif /* KAME_SERVO_CAL_H */
