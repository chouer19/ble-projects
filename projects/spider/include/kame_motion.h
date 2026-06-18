/*
 * Kame 四足机器人 — Motion Framework（动作序列 + 状态机）
 *
 * 架构（不追求实时步态 / 不用 IK / 不做轨迹规划）：
 *
 *   - 每个 Motion 由若干 Step 组成；每个 Step 输出一个整机 Pose。
 *   - FSM 后台线程每隔 motion_step_delay_ms（默认 500ms）推进一个 Step：
 *       构建该 Step 的 Pose -> kame_pose_apply() 下发。
 *   - 同一时刻只运行一个 Motion；收到新 Motion 立即切换（打断当前）。
 *   - 静态动作：跑完全部 Step 后停在末态。
 *   - 循环动作：跑完后回到 Step0 反复执行，直到被切换/停止。
 *
 * 生命周期钩子（见 kame_motion.c 中 FSM 线程）：
 *   motion_enter()  进入某 Motion 时复位 step=0
 *   motion_step()   执行当前 Step 并推进
 *   motion_exit()   切换/停止时收尾
 */

#ifndef KAME_MOTION_H
#define KAME_MOTION_H

#include <stdbool.h>
#include <stdint.h>

#include "kame_pose.h"

/* ------------------------------------------------------------------ */
/* 统一 Motion 枚举                                                    */
/* ------------------------------------------------------------------ */

typedef enum {
	/* --- 静态动作 --- */
	KAME_MOTION_STAND = 0,
	KAME_MOTION_SPLAY,
	KAME_MOTION_LIFT_LEG,   /* 需腿参数 L1/R1/L2/R2 */
	KAME_MOTION_FRONT_DOWN,
	KAME_MOTION_REAR_DOWN,
	KAME_MOTION_FULL_DOWN,

	/* --- 循环动作 --- */
	KAME_MOTION_FORWARD,
	KAME_MOTION_BACKWARD,
	KAME_MOTION_TURN_LEFT,
	KAME_MOTION_TURN_RIGHT,
	KAME_MOTION_SPIN_LEFT,
	KAME_MOTION_SPIN_RIGHT,
	KAME_MOTION_NOD,
	KAME_MOTION_SHAKE,
	KAME_MOTION_ROLL,
	KAME_MOTION_WAVE,       /* 需腿参数 L1/R1/L2/R2 */

	KAME_MOTION_COUNT,
	KAME_MOTION_NONE = 0xFF,
} kame_motion_id_t;

/* motion_step_delay_ms 默认节拍 */
#define KAME_MOTION_DEFAULT_DELAY_MS 500U

/* ------------------------------------------------------------------ */
/* 控制 API                                                            */
/* ------------------------------------------------------------------ */

/** 请求执行某 Motion（打断并替换当前 Motion）。leg 仅对需要腿参数的动作有效。 */
void kame_motion_request(kame_motion_id_t id, kame_leg_id_t leg);

/** 停止当前 Motion（停在当前姿态，不再推进）。 */
void kame_motion_stop(void);

/** 设置/读取全局动作节拍 motion_step_delay_ms。 */
void kame_motion_set_delay(uint32_t ms);
uint32_t kame_motion_get_delay(void);

/* ------------------------------------------------------------------ */
/* 查询（供 shell 命令解析使用）                                       */
/* ------------------------------------------------------------------ */

/** Motion 名称（小写，如 "lift"）。 */
const char *kame_motion_name(kame_motion_id_t id);

/** 由名称解析 Motion id；失败返回 KAME_MOTION_NONE。 */
kame_motion_id_t kame_motion_from_name(const char *name);

/** 该 Motion 是否需要腿参数（lift / wave）。 */
bool kame_motion_needs_leg(kame_motion_id_t id);

/** 该 Motion 是否为循环动作。 */
bool kame_motion_is_cyclic(kame_motion_id_t id);

#endif /* KAME_MOTION_H */
