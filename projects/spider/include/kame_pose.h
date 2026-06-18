/*
 * Kame 四足机器人 — Pose（静态姿态）抽象层
 *
 * 设计原则：
 *   - 不使用 IK / 不做轨迹规划；姿态由「每个舵机的目标角度」直接描述。
 *   - 动作代码禁止出现 PCA9685 通道号；一律通过 servo_id / leg_id 访问。
 *   - 单腿动作（lift / wave）执行前必须先进入对应 TRIPOD 三脚架姿态，
 *     由其余三条腿调整重心形成稳定支撑后再抬腿。
 *
 * 一个 Pose = 8 个舵机的目标角度数组（按 kame_servo_id_t 索引）。
 * 后端（main.c）通过 kame_servo_apply() 把角度平滑下发到 PCA9685。
 */

#ifndef KAME_POSE_H
#define KAME_POSE_H

#include <stdbool.h>
#include <stdint.h>

#include "kame_servo_cal.h"

/* ------------------------------------------------------------------ */
/* 腿抽象：leg_id -> (hip servo_id, knee servo_id)                      */
/* ------------------------------------------------------------------ */

typedef enum {
	KAME_LEG_L1 = 0, /* 前左 */
	KAME_LEG_R1,     /* 前右 */
	KAME_LEG_L2,     /* 后左 */
	KAME_LEG_R2,     /* 后右 */
	KAME_LEG_COUNT,
	KAME_LEG_NONE = 0xFF,
} kame_leg_id_t;

struct kame_leg_map {
	const char *name;        /* "L1"/"R1"/"L2"/"R2" */
	kame_servo_id_t hip;     /* 髋关节 servo_id */
	kame_servo_id_t knee;    /* 腿(膝)关节 servo_id */
	bool is_front;           /* true=前腿(L1/R1) */
	bool is_left;            /* true=左腿(L1/L2) */
};

extern const struct kame_leg_map kame_legs[KAME_LEG_COUNT];

/** 按 leg_id 取腿映射，非法返回 NULL */
const struct kame_leg_map *kame_leg_get(kame_leg_id_t leg);

/** 解析腿名("L1"/"R1"/"L2"/"R2"，大小写不敏感)，失败返回 KAME_LEG_NONE */
kame_leg_id_t kame_leg_from_name(const char *s);

/* ------------------------------------------------------------------ */
/* Pose 数据结构                                                       */
/* ------------------------------------------------------------------ */

/** 该舵机本步不动（保持当前角度） */
#define KAME_POSE_KEEP 0xFFU

typedef struct {
	uint8_t deg[KAME_SERVO_COUNT]; /* 按 kame_servo_id_t 索引；KEEP=不动 */
} kame_pose_t;

/* ------------------------------------------------------------------ */
/* 可调参数（在硬件上微调；不写死通道号）                              */
/* ------------------------------------------------------------------ */

#define KAME_SPLAY_DELTA_DEG   30 /* 叉腿髋偏移 */
/*
 * 三脚架：支撑腿髋关节平移机身重心离开抬起腿。
 * 注意防撞——抬某腿时其同侧另一腿会向该腿方向平移本量，二者「靠拢量」≈本值。
 * 实测同侧前后腿碰撞阈值约 35°(左)/40°(右)，故取 20° 留 ~15° 余量。
 */
#define KAME_TRIPOD_HIP_DEG    25 /* 三脚架：支撑腿髋关节调整，把机身重心移离抬起腿 */
#define KAME_TRIPOD_LEAN_DEG   25 /* 三脚架：对侧压腿+同侧收腿，横向把重心推入支撑三角 */
#define KAME_LIFT_KNEE_DEG     75 /* 抬腿：目标腿膝关节向最高方向偏移 */
#define KAME_WAVE_KNEE_DEG     50 /* 招手：目标腿膝关节自最高端向下回落的摆幅 */
#define KAME_DOWN_KNEE_DEG     45 /* 趴下：相关腿膝关节收起幅度 */

/* ------------------------------------------------------------------ */
/* 静态姿态 ID（无参数，整机姿态）                                     */
/* ------------------------------------------------------------------ */

typedef enum {
	KAME_POSE_STAND = 0,
	KAME_POSE_SPLAY,
	KAME_POSE_FRONT_DOWN,
	KAME_POSE_REAR_DOWN,
	KAME_POSE_FULL_DOWN,
	KAME_POSE_ID_COUNT,
} kame_pose_id_t;

/* ---- 整机静态姿态构建 ---- */
void kame_pose_stand(kame_pose_t *out);
void kame_pose_splay(uint8_t delta, kame_pose_t *out);
void kame_pose_front_down(kame_pose_t *out);
void kame_pose_rear_down(kame_pose_t *out);
void kame_pose_full_down(kame_pose_t *out);

/** 通过 pose_id 分发到上面对应的构建函数 */
void kame_pose_build(kame_pose_id_t id, kame_pose_t *out);

/* ---- 单腿动作复用姿态 ---- */

/**
 * 三脚架姿态：抬 lift_leg 前，其余三条腿调整髋/膝把重心移入支撑三角
 * （远离 lift_leg）。lift_leg 本身保持站立角（尚未抬起）。
 */
void kame_pose_tripod(kame_leg_id_t lift_leg, kame_pose_t *out);

/**
 * 抬腿姿态：在 kame_pose_tripod(lift_leg) 基础上，把 lift_leg 的膝关节抬起。
 */
void kame_pose_lift(kame_leg_id_t lift_leg, kame_pose_t *out);

/* ------------------------------------------------------------------ */
/* 下发                                                                */
/* ------------------------------------------------------------------ */

/** 逐舵机下发 pose（KEEP 跳过）。底层平滑移动由后端按全局速度处理。 */
void kame_pose_apply(const kame_pose_t *p);

/**
 * 舵机后端钩子，由 main.c 实现：
 * 把 servo_id 对应舵机按全局速度平滑移动到 deg（内部完成 id->通道、角度钳位）。
 */
int kame_servo_apply(kame_servo_id_t id, uint8_t deg);

#endif /* KAME_POSE_H */
