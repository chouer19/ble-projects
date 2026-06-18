/*
 * Kame 四足机器人 — Motion Framework 实现
 *
 * 步序表（motions[]）+ FSM 后台线程。每个 Motion 是一组 Step，
 * 每个 Step 由 build() 输出一个整机 Pose，线程按 motion_step_delay_ms 节拍推进。
 *
 * 步态说明（无 IK / 无轨迹规划，离散对角步态，参数可在硬件上微调）：
 *   对角腿配对：A = {L1, R2}，B = {R1, L2}
 *   4 拍循环：
 *     拍0  抬起 A，A 髋摆到「前」，B 落地，B 髋在「后」
 *     拍1  A 落地于前
 *     拍2  抬起 B，B 髋摆到「前」，A 落地，A 髋在「后」
 *     拍3  B 落地于前
 *   落地腿由「前」拖向「后」即为驱动行程，推动机身前进。
 *   每条腿一个方向系数 dir（+1/-1/0）即可派生 前进/后退/转向/原地旋转。
 *   外张偏置：前腿恒偏机身前、后腿恒偏机身后（GAIT_HIP_BIAS_DEG），摆动叠加其上，
 *   使同侧前后腿摆动窗口分处中线两侧、永不相向靠拢，避免前后腿互撞损坏舵机。
 */

#include <zephyr/kernel.h>
#include <ctype.h>

#include "kame_motion.h"
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

/* ---- 步态可调参数 ---- */
#define GAIT_HIP_DEG       20 /* 髋前后摆幅（单侧；以外张偏置为中心前后各摆该角） */
#define GAIT_HIP_BIAS_DEG  25 /* 外张偏置：前腿恒偏机身前、后腿恒偏机身后，
				 * 使同侧前后腿摆动窗口分处中线两侧，永不相向靠拢相撞。
				 * 需满足 BIAS > HIP_DEG，保证每腿始终停在中位自己一侧。 */
#define GAIT_LIFT_DEG  40 /* 摆动腿抬起量 */
#define ROLL_DEG       25 /* 横滚幅度 */
#define NOD_DEG        30 /* 点头(前腿膝)幅度 */
/* 摇头(前腿髋)幅度：前腿后摆时会靠近同侧后腿，靠拢量≈本值；
 * 实测碰撞阈值约 35°(左)/40°(右)，取 20° 留 ~15° 余量。 */
#define SHAKE_DEG      20 /* 摇头(前腿髋)幅度 */

/* ------------------------------------------------------------------ */
/* Step 构建：静态动作                                                 */
/* ------------------------------------------------------------------ */

static void step_stand(kame_leg_id_t leg, uint8_t step, kame_pose_t *p)
{
	ARG_UNUSED(leg); ARG_UNUSED(step);
	kame_pose_stand(p);
}

static void step_splay(kame_leg_id_t leg, uint8_t step, kame_pose_t *p)
{
	ARG_UNUSED(leg); ARG_UNUSED(step);
	kame_pose_splay(KAME_SPLAY_DELTA_DEG, p);
}

static void step_front_down(kame_leg_id_t leg, uint8_t step, kame_pose_t *p)
{
	ARG_UNUSED(leg); ARG_UNUSED(step);
	kame_pose_front_down(p);
}

static void step_rear_down(kame_leg_id_t leg, uint8_t step, kame_pose_t *p)
{
	ARG_UNUSED(leg); ARG_UNUSED(step);
	kame_pose_rear_down(p);
}

static void step_full_down(kame_leg_id_t leg, uint8_t step, kame_pose_t *p)
{
	ARG_UNUSED(leg); ARG_UNUSED(step);
	kame_pose_full_down(p);
}

/* ------------------------------------------------------------------ */
/* Step 构建：单腿动作（先进 TRIPOD，禁止直接抬腿）                    */
/* ------------------------------------------------------------------ */

/*
 * LIFT_LEG（静态动作，2 步，停在末态）：
 *   Step0 先进三脚架（支撑腿调整髋/膝形成稳定支撑，禁止直接抬腿）
 *   Step1 抬起目标腿，保持。
 * 跑完停在「抬腿」末态；结束请用 stop（恢复站立）或切换其他动作。
 */
static void step_lift_leg(kame_leg_id_t leg, uint8_t step, kame_pose_t *p)
{
	if (step == 0) {
		kame_pose_tripod(leg, p); /* Step0 稳定三脚架 */
	} else {
		kame_pose_lift(leg, p);   /* Step1 抬腿并保持 */
	}
}

/*
 * WAVE（循环动作）：目标腿膝关节贴近最高点往复摆动招手。
 * 支撑三腿始终保持 TRIPOD 稳定姿态；目标腿髋关节保持站立角不动，
 * 仅膝关节在各自「最高端」附近上下摆动，幅度 KAME_WAVE_KNEE_DEG：
 *   Step0 贴到最高端 / Step1 回落 KAME_WAVE_KNEE_DEG（循环）。
 * 例如 L1 膝(0~180°，180°最高)即在 (180-KAME_WAVE_KNEE_DEG)~180° 之间摆动。
 * 停止(stop)时直接恢复立正姿势。
 */
static void step_wave(kame_leg_id_t leg, uint8_t step, kame_pose_t *p)
{
	const struct kame_leg_map *X = kame_leg_get(leg);

	kame_pose_tripod(leg, p); /* 支撑三腿稳定；目标腿髋/膝暂为站立角 */

	int down = (step % 2U) == 0U
		   ? 0                   /* 贴到最高端 */
		   : KAME_WAVE_KNEE_DEG; /* 自最高端回落 */

	if (X != NULL) {
		p->deg[X->knee] = kame_servo_knee_top_deg(X->knee, down);
	}
}

/* ------------------------------------------------------------------ */
/* Step 构建：循环步态                                                 */
/* ------------------------------------------------------------------ */

/* 对角配对：A = {L1, R2} */
static inline bool leg_in_pair_a(int leg)
{
	return (leg == KAME_LEG_L1) || (leg == KAME_LEG_R2);
}

/*
 * 通用步态：dir[leg] 为各腿前进方向系数（+1 向前 / -1 向后 / 0 原地）。
 * 4 拍循环，A、B 两对交替抬腿—摆动—落地—驱动。
 */
static void gait_fill(kame_pose_t *p, uint8_t step, const int8_t dir[KAME_LEG_COUNT])
{
	kame_pose_stand(p);

	uint8_t ph = step % 4U;
	bool a_forward = (ph == 0U) || (ph == 1U);
	bool a_lift = (ph == 0U);
	bool b_lift = (ph == 2U);

	for (int i = 0; i < KAME_LEG_COUNT; i++) {
		const struct kame_leg_map *L = &kame_legs[i];
		bool in_a = leg_in_pair_a(i);
		bool forward = in_a ? a_forward : !a_forward;
		bool lift = in_a ? a_lift : b_lift;

		/* 外张偏置：前腿恒偏机身前、后腿恒偏机身后；摆动叠加其上。
		 * BIAS>HIP_DEG 保证前腿不越过中位向后、后腿不越过中位向前，
		 * 同侧前后腿摆动窗口分处中线两侧，避免相向靠拢相撞。 */
		int home = L->is_front ? GAIT_HIP_BIAS_DEG : -GAIT_HIP_BIAS_DEG;
		int swing = (forward ? GAIT_HIP_DEG : -GAIT_HIP_DEG) * dir[i];
		int hip_fwd = home + swing;

		p->deg[L->hip] = kame_servo_hip_forward_deg(L->hip, hip_fwd);
		if (lift) {
			p->deg[L->knee] =
				kame_servo_knee_up_deg(L->knee, GAIT_LIFT_DEG);
		}
	}
}

/* 各腿顺序：L1, R1, L2, R2 */
static const int8_t DIR_FORWARD[KAME_LEG_COUNT]    = { +1, +1, +1, +1 };
static const int8_t DIR_BACKWARD[KAME_LEG_COUNT]   = { -1, -1, -1, -1 };
static const int8_t DIR_SPIN_LEFT[KAME_LEG_COUNT]  = { -1, +1, -1, +1 }; /* 右侧向前/左侧向后 -> 左转 */
static const int8_t DIR_SPIN_RIGHT[KAME_LEG_COUNT] = { +1, -1, +1, -1 };
static const int8_t DIR_TURN_LEFT[KAME_LEG_COUNT]  = {  0, +1,  0, +1 }; /* 左侧原地、右侧前进 -> 弧线左转 */
static const int8_t DIR_TURN_RIGHT[KAME_LEG_COUNT] = { +1,  0, +1,  0 };

static void step_forward(kame_leg_id_t leg, uint8_t step, kame_pose_t *p)
{ ARG_UNUSED(leg); gait_fill(p, step, DIR_FORWARD); }
static void step_backward(kame_leg_id_t leg, uint8_t step, kame_pose_t *p)
{ ARG_UNUSED(leg); gait_fill(p, step, DIR_BACKWARD); }
static void step_turn_left(kame_leg_id_t leg, uint8_t step, kame_pose_t *p)
{ ARG_UNUSED(leg); gait_fill(p, step, DIR_TURN_LEFT); }
static void step_turn_right(kame_leg_id_t leg, uint8_t step, kame_pose_t *p)
{ ARG_UNUSED(leg); gait_fill(p, step, DIR_TURN_RIGHT); }
static void step_spin_left(kame_leg_id_t leg, uint8_t step, kame_pose_t *p)
{ ARG_UNUSED(leg); gait_fill(p, step, DIR_SPIN_LEFT); }
static void step_spin_right(kame_leg_id_t leg, uint8_t step, kame_pose_t *p)
{ ARG_UNUSED(leg); gait_fill(p, step, DIR_SPIN_RIGHT); }

/* ------------------------------------------------------------------ */
/* Step 构建：机身姿态摆动（循环）                                     */
/* ------------------------------------------------------------------ */

/* 横滚：sign>0 左侧膝抬/右侧膝压；sign<0 相反 */
static void roll_fill(kame_pose_t *p, int sign)
{
	kame_pose_stand(p);
	for (int i = 0; i < KAME_LEG_COUNT; i++) {
		const struct kame_leg_map *L = &kame_legs[i];
		int d = (L->is_left ? +ROLL_DEG : -ROLL_DEG) * sign;

		p->deg[L->knee] = kame_servo_knee_up_deg(L->knee, d);
	}
}

static void step_roll(kame_leg_id_t leg, uint8_t step, kame_pose_t *p)
{
	ARG_UNUSED(leg);
	roll_fill(p, (step % 2U) == 0U ? +1 : -1);
}

/* 点头：前腿膝关节上下往复 */
static void step_nod(kame_leg_id_t leg, uint8_t step, kame_pose_t *p)
{
	ARG_UNUSED(leg);
	kame_pose_stand(p);

	int d = (step % 2U) == 0U ? +NOD_DEG : -NOD_DEG;

	for (int i = 0; i < KAME_LEG_COUNT; i++) {
		const struct kame_leg_map *L = &kame_legs[i];

		if (L->is_front) {
			p->deg[L->knee] = kame_servo_knee_up_deg(L->knee, d);
		}
	}
}

/* 摇头：两前腿髋关节反向往复（机身前部偏摆） */
static void step_shake(kame_leg_id_t leg, uint8_t step, kame_pose_t *p)
{
	ARG_UNUSED(leg);
	kame_pose_stand(p);

	int sign = (step % 2U) == 0U ? +1 : -1;

	for (int i = 0; i < KAME_LEG_COUNT; i++) {
		const struct kame_leg_map *L = &kame_legs[i];

		if (L->is_front) {
			int d = (L->is_left ? +SHAKE_DEG : -SHAKE_DEG) * sign;

			p->deg[L->hip] = kame_servo_hip_forward_deg(L->hip, d);
		}
	}
}

/* ------------------------------------------------------------------ */
/* Motion 步序表                                                       */
/* ------------------------------------------------------------------ */

typedef void (*kame_step_fn)(kame_leg_id_t leg, uint8_t step, kame_pose_t *out);

typedef struct {
	const char *name;
	bool cyclic;
	bool needs_leg;
	uint8_t steps;
	kame_step_fn build;
} kame_motion_def_t;

static const kame_motion_def_t motions[KAME_MOTION_COUNT] = {
	[KAME_MOTION_STAND]      = {"stand",      false, false, 1, step_stand},
	[KAME_MOTION_SPLAY]      = {"splay",      false, false, 1, step_splay},
	[KAME_MOTION_LIFT_LEG]   = {"lift",       false, true,  2, step_lift_leg},
	[KAME_MOTION_FRONT_DOWN] = {"front_down", false, false, 1, step_front_down},
	[KAME_MOTION_REAR_DOWN]  = {"rear_down",  false, false, 1, step_rear_down},
	[KAME_MOTION_FULL_DOWN]  = {"full_down",  false, false, 1, step_full_down},

	[KAME_MOTION_FORWARD]    = {"forward",    true,  false, 4, step_forward},
	[KAME_MOTION_BACKWARD]   = {"backward",   true,  false, 4, step_backward},
	[KAME_MOTION_TURN_LEFT]  = {"turn_left",  true,  false, 4, step_turn_left},
	[KAME_MOTION_TURN_RIGHT] = {"turn_right", true,  false, 4, step_turn_right},
	[KAME_MOTION_SPIN_LEFT]  = {"spin_left",  true,  false, 4, step_spin_left},
	[KAME_MOTION_SPIN_RIGHT] = {"spin_right", true,  false, 4, step_spin_right},
	[KAME_MOTION_NOD]        = {"nod",        true,  false, 2, step_nod},
	[KAME_MOTION_SHAKE]      = {"shake",      true,  false, 2, step_shake},
	[KAME_MOTION_ROLL]       = {"roll",       true,  false, 2, step_roll},
	[KAME_MOTION_WAVE]       = {"wave",       true,  true,  2, step_wave},
};

/* ------------------------------------------------------------------ */
/* FSM 状态 + 后台线程                                                 */
/* ------------------------------------------------------------------ */

static uint32_t motion_step_delay_ms = KAME_MOTION_DEFAULT_DELAY_MS;

static struct {
	kame_motion_id_t id;
	kame_leg_id_t leg;
	uint8_t step;
	uint32_t generation; /* 每次请求/停止递增，用于打断当前 Motion */
	bool active;
} st = {
	.id = KAME_MOTION_NONE,
	.leg = KAME_LEG_NONE,
	.active = false,
};

static K_MUTEX_DEFINE(st_mtx);
static K_SEM_DEFINE(motion_wake, 0, 1);

/* motion_enter：复位到 Step0 并唤醒线程 */
void kame_motion_request(kame_motion_id_t id, kame_leg_id_t leg)
{
	if ((unsigned int)id >= KAME_MOTION_COUNT) {
		return;
	}

	k_mutex_lock(&st_mtx, K_FOREVER);
	st.id = id;
	st.leg = leg;
	st.step = 0;
	st.active = true;
	st.generation++;
	k_mutex_unlock(&st_mtx);

	k_sem_give(&motion_wake);
}

/* motion_exit：停止推进，停在当前姿态 */
void kame_motion_stop(void)
{
	k_mutex_lock(&st_mtx, K_FOREVER);
	st.active = false;
	st.generation++;
	k_mutex_unlock(&st_mtx);

	k_sem_give(&motion_wake);
}

void kame_motion_set_delay(uint32_t ms)
{
	k_mutex_lock(&st_mtx, K_FOREVER);
	motion_step_delay_ms = ms;
	k_mutex_unlock(&st_mtx);
}

uint32_t kame_motion_get_delay(void)
{
	uint32_t ms;

	k_mutex_lock(&st_mtx, K_FOREVER);
	ms = motion_step_delay_ms;
	k_mutex_unlock(&st_mtx);
	return ms;
}

static void motion_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	for (;;) {
		k_mutex_lock(&st_mtx, K_FOREVER);
		bool active = st.active;
		uint32_t gen = st.generation;
		kame_motion_id_t id = st.id;
		kame_leg_id_t leg = st.leg;
		uint8_t step = st.step;
		uint32_t delay = motion_step_delay_ms;
		k_mutex_unlock(&st_mtx);

		/* 空闲：阻塞等待下一个 Motion 请求 */
		if (!active || (unsigned int)id >= KAME_MOTION_COUNT) {
			k_sem_take(&motion_wake, K_FOREVER);
			continue;
		}

		const kame_motion_def_t *m = &motions[id];

		/* 序列结束处理：循环动作回到 Step0；静态动作停在末态 */
		if (step >= m->steps) {
			if (m->cyclic) {
				step = 0;
			} else {
				k_mutex_lock(&st_mtx, K_FOREVER);
				if (st.generation == gen) {
					st.active = false;
				}
				k_mutex_unlock(&st_mtx);
				continue;
			}
		}

		/* motion_step：构建当前 Step 的 Pose 并下发 */
		kame_pose_t pose;

		m->build(leg, step, &pose);
		kame_pose_apply(&pose);

		/* 推进 step（若期间未被新请求打断） */
		k_mutex_lock(&st_mtx, K_FOREVER);
		if (st.generation == gen) {
			st.step = step + 1U;
		}
		k_mutex_unlock(&st_mtx);

		/* 等待一个节拍；收到新请求/停止则提前唤醒并重新评估状态 */
		k_sem_take(&motion_wake, K_MSEC(delay));
	}
}

K_THREAD_DEFINE(kame_motion_tid, 1536, motion_thread, NULL, NULL, NULL, 6, 0, 0);

/* ------------------------------------------------------------------ */
/* 查询                                                                */
/* ------------------------------------------------------------------ */

const char *kame_motion_name(kame_motion_id_t id)
{
	if ((unsigned int)id >= KAME_MOTION_COUNT) {
		return "?";
	}
	return motions[id].name;
}

kame_motion_id_t kame_motion_from_name(const char *name)
{
	if (name == NULL) {
		return KAME_MOTION_NONE;
	}
	for (int i = 0; i < KAME_MOTION_COUNT; i++) {
		if (ci_equal(name, motions[i].name)) {
			return (kame_motion_id_t)i;
		}
	}
	return KAME_MOTION_NONE;
}

bool kame_motion_needs_leg(kame_motion_id_t id)
{
	if ((unsigned int)id >= KAME_MOTION_COUNT) {
		return false;
	}
	return motions[id].needs_leg;
}

bool kame_motion_is_cyclic(kame_motion_id_t id)
{
	if ((unsigned int)id >= KAME_MOTION_COUNT) {
		return false;
	}
	return motions[id].cyclic;
}
