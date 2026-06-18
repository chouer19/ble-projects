/*
 * Kame 四足机器人 — Pose（静态姿态）实现
 *
 * 全部姿态都以 kame_servo_cal 中的标定角为基准，通过
 * kame_servo_hip_forward_deg()/kame_servo_knee_up_deg() 做定向偏移，
 * 因此不依赖任何 PCA9685 通道号，方向差异由标定标志位自动处理。
 */

#include <ctype.h>
#include <string.h>

#include "kame_pose.h"

/* ASCII 大小写不敏感比较（避免依赖 strcasecmp 的 libc 实现差异） */
static bool ci_equal(const char *a, const char *b)
{
	if (a == NULL || b == NULL) {
		return false;
	}
	while (*a != '\0' && *b != '\0') {
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
			return false;
		}
		a++;
		b++;
	}
	return *a == *b;
}

/* ------------------------------------------------------------------ */
/* 腿映射表                                                            */
/* ------------------------------------------------------------------ */

const struct kame_leg_map kame_legs[KAME_LEG_COUNT] = {
	[KAME_LEG_L1] = {"L1", KAME_SERVO_L1_HIP, KAME_SERVO_L1_KNEE, true,  true},
	[KAME_LEG_R1] = {"R1", KAME_SERVO_R1_HIP, KAME_SERVO_R1_KNEE, true,  false},
	[KAME_LEG_L2] = {"L2", KAME_SERVO_L2_HIP, KAME_SERVO_L2_KNEE, false, true},
	[KAME_LEG_R2] = {"R2", KAME_SERVO_R2_HIP, KAME_SERVO_R2_KNEE, false, false},
};

const struct kame_leg_map *kame_leg_get(kame_leg_id_t leg)
{
	if ((unsigned int)leg >= KAME_LEG_COUNT) {
		return NULL;
	}
	return &kame_legs[leg];
}

kame_leg_id_t kame_leg_from_name(const char *s)
{
	if (s == NULL) {
		return KAME_LEG_NONE;
	}
	for (int i = 0; i < KAME_LEG_COUNT; i++) {
		if (ci_equal(s, kame_legs[i].name)) {
			return (kame_leg_id_t)i;
		}
	}
	return KAME_LEG_NONE;
}

/* ------------------------------------------------------------------ */
/* 基础姿态                                                            */
/* ------------------------------------------------------------------ */

void kame_pose_stand(kame_pose_t *out)
{
	for (kame_servo_id_t id = 0; id < KAME_SERVO_COUNT; id++) {
		out->deg[id] = kame_servos[id].stand_deg;
	}
}

void kame_pose_splay(uint8_t delta, kame_pose_t *out)
{
	for (kame_servo_id_t id = 0; id < KAME_SERVO_COUNT; id++) {
		out->deg[id] = kame_servo_splay_deg(id, delta);
	}
}

/*
 * 趴下姿态：把指定腿的膝关节向最高方向收起 KAME_DOWN_KNEE_DEG，
 * 使对应身体部分落下（前/后/全部）。髋关节保持站立角。
 */
static void pose_down(bool front, bool rear, kame_pose_t *out)
{
	kame_pose_stand(out);

	for (int i = 0; i < KAME_LEG_COUNT; i++) {
		const struct kame_leg_map *L = &kame_legs[i];
		bool sel = (L->is_front && front) || (!L->is_front && rear);

		if (sel) {
			out->deg[L->knee] =
				kame_servo_knee_up_deg(L->knee, KAME_DOWN_KNEE_DEG);
		}
	}
}

void kame_pose_front_down(kame_pose_t *out) { pose_down(true, false, out); }
void kame_pose_rear_down(kame_pose_t *out)  { pose_down(false, true, out); }
void kame_pose_full_down(kame_pose_t *out)  { pose_down(true, true, out); }

/* ------------------------------------------------------------------ */
/* 三脚架 / 抬腿                                                       */
/* ------------------------------------------------------------------ */

/*
 * 三脚架：要抬 lift_leg，需把重心移到其余三条腿构成的支撑三角内（远离 lift_leg）。
 *
 * 主手段——调整三条支撑腿的【髋关节】平移机身重心：
 *   四脚着地时旋转髋关节会让机身相对脚掌前后平移。
 *   - 抬前腿(L1/R1)：三条支撑腿髋向「前」转 -> 机身后移 -> 重心落入后侧支撑三角；
 *   - 抬后腿(L2/R2)：三条支撑腿髋向「后」转 -> 机身前移 -> 重心落入前侧支撑三角。
 *   KAME_TRIPOD_HIP_DEG 为可调平移量。
 *
 * 辅助手段——【膝关节】做左右侧倾，把重心移离抬起腿：
 *   对侧两条支撑腿压腿(伸展)抬高该侧机身；同侧唯一支撑腿略收腿降低该侧。
 *   二者配合把重心推入 {R1,L2,R2} 等支撑三角内。KAME_TRIPOD_LEAN_DEG 为倾斜量。
 *
 * 目标腿 lift_leg 本身保持站立角（尚未抬起），抬腿在 kame_pose_lift() 中进行。
 */
void kame_pose_tripod(kame_leg_id_t lift_leg, kame_pose_t *out)
{
	kame_pose_stand(out);

	const struct kame_leg_map *X = kame_leg_get(lift_leg);

	if (X == NULL) {
		return; /* 非法腿：退化为标准站姿 */
	}

	/* 抬前腿 -> 机身后移(支撑腿髋向前)；抬后腿 -> 机身前移(支撑腿髋向后) */
	int hip_fwd = X->is_front ? KAME_TRIPOD_HIP_DEG : -KAME_TRIPOD_HIP_DEG;

	for (int i = 0; i < KAME_LEG_COUNT; i++) {
		if (i == (int)lift_leg) {
			continue; /* 目标腿保持站立角，下一步再抬 */
		}

		const struct kame_leg_map *L = &kame_legs[i];

		/* 主：调整支撑腿髋关节平移重心，让其余三腿更稳定 */
		out->deg[L->hip] = kame_servo_hip_forward_deg(L->hip, hip_fwd);

		/* 辅：对侧压腿抬高、同侧略收腿，横向把重心推离抬起腿 */
		if (L->is_left != X->is_left) {
			out->deg[L->knee] =
				kame_servo_knee_up_deg(L->knee, -KAME_TRIPOD_LEAN_DEG);
		} else {
			out->deg[L->knee] =
				kame_servo_knee_up_deg(L->knee, KAME_TRIPOD_LEAN_DEG);
		}
	}
}

void kame_pose_lift(kame_leg_id_t lift_leg, kame_pose_t *out)
{
	kame_pose_tripod(lift_leg, out);

	const struct kame_leg_map *X = kame_leg_get(lift_leg);

	if (X == NULL) {
		return;
	}
	out->deg[X->knee] = kame_servo_knee_up_deg(X->knee, KAME_LIFT_KNEE_DEG);
}

/* ------------------------------------------------------------------ */
/* 分发 / 下发                                                         */
/* ------------------------------------------------------------------ */

void kame_pose_build(kame_pose_id_t id, kame_pose_t *out)
{
	switch (id) {
	case KAME_POSE_STAND:      kame_pose_stand(out); break;
	case KAME_POSE_SPLAY:      kame_pose_splay(KAME_SPLAY_DELTA_DEG, out); break;
	case KAME_POSE_FRONT_DOWN: kame_pose_front_down(out); break;
	case KAME_POSE_REAR_DOWN:  kame_pose_rear_down(out); break;
	case KAME_POSE_FULL_DOWN:  kame_pose_full_down(out); break;
	default:                   kame_pose_stand(out); break;
	}
}

void kame_pose_apply(const kame_pose_t *p)
{
	for (kame_servo_id_t id = 0; id < KAME_SERVO_COUNT; id++) {
		if (p->deg[id] == KAME_POSE_KEEP) {
			continue;
		}
		kame_servo_apply(id, p->deg[id]);
	}
}
