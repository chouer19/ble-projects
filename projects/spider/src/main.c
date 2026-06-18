/*
 * spider - 串口交互控制 PCA9685 舵机（多通道 + 平滑运动）
 *
 * 接线：I2C0  SDA=P0.26  SCL=P0.27，PCA9685 地址 0x40
 *
 * UART shell 命令（vcom0, 115200，支持 Tab 补全）：
 *
 *   spider scan                     扫描 I2C 总线
 *   spider check [addr]             读 PCA9685 MODE1/MODE2 寄存器
 *   spider servo <ch> <us>          设通道脉宽（微秒）
 *   angle <ch> <deg>                设通道角度（0~180，顶层命令，无 spider 前缀）
 *   spider multi <ch:deg> ...       多通道同步设角度，如 spider multi 0:90 4:45
 *   spider all <deg>                所有已激活通道设同一角度
 *   spider move <ch> <deg> <ms>     在指定时长内匀速转到目标角度
 *   spider speed [deg_per_s]        查看/设置全局速度限制（0=瞬时）
 *   spider status                   显示各通道状态
 *   spider off <ch>                 关闭单通道
 *   spider offall                   关闭全部通道
 *   spider stand                    Kame 进入标准站姿（按标定表）
 *   spider splay [delta]            Kame 叉腿站立（前腿前/后腿后，默认 30°）
 *
 * Motion Framework 顶层命令（动作序列 + FSM，无前缀，便于串口输入）：
 *   stand / splay                   静态姿态
 *   lift <L1|R1|L2|R2>              抬腿并保持（先进 TRIPOD，静态）
 *   wave <L1|R1|L2|R2>              招手（循环；stop 恢复立正）
 *   front_down/rear_down/full_down  趴下
 *   forward/backward                行走（循环）
 *   turn_left/turn_right            弧线转向（循环）
 *   spin_left/spin_right            原地旋转（循环）
 *   nod / shake / roll              机身摆动（循环）
 *   speed [ms]                      查看/设置动作节拍 motion_step_delay_ms
 *   stop                            停止当前动作并恢复立正
 *
 * 运动模型：50 Hz tick 的插值引擎。angle/servo/multi/all 按全局速度限制
 * 平滑移动；move 按给定时长移动；首次激活某通道时瞬间到位（位置未知）。
 *
 * 上电行为：检测到 PCA9685 后自动执行 stand（标准站姿）进行复位，
 * 无需手动输入命令。
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>
#include <stdlib.h>
#include <string.h>

#include "kame_servo_cal.h"
#include "kame_pose.h"
#include "kame_motion.h"

/* 叉腿站立时髋关节相对站立位的默认偏移角（前腿向前 / 后腿向后） */
#define KAME_SPLAY_DEFAULT_DEG 30U

#define PCA9685_DEFAULT_ADDR 0x40
#define PCA9685_REG_MODE1    0x00
#define PCA9685_REG_MODE2    0x01

#define SERVO_PERIOD_USEC 20000U
#define SERVO_PULSE_MIN   500U
#define SERVO_PULSE_MAX   2500U
#define SERVO_CHANNEL_CNT 16

#define MOTION_TICK_MS    20U
/* 每度对应的脉宽（us），500~2500us 映射 0~180 度 */
#define US_PER_DEG        ((SERVO_PULSE_MAX - SERVO_PULSE_MIN) / 180U)

/* 全局速度限制（度/秒），0 = 瞬时到位 */
#define SPEED_DEFAULT_DEG_S 240U

static const struct device *i2c_bus = DEVICE_DT_GET(DT_NODELABEL(i2c0));
static const struct device *pca9685 = DEVICE_DT_GET(DT_NODELABEL(pca9685));

struct servo_chan {
	bool active;
	uint16_t cur_us;
	uint16_t tgt_us;
	uint16_t step_us;   /* 每 tick 步进量，0 = 瞬时 */
};

static struct servo_chan chans[SERVO_CHANNEL_CNT];
static uint32_t speed_deg_s = SPEED_DEFAULT_DEG_S;
static K_MUTEX_DEFINE(servo_lock);

static uint32_t deg_to_us(uint32_t deg)
{
	return SERVO_PULSE_MIN +
	       (SERVO_PULSE_MAX - SERVO_PULSE_MIN) * deg / 180U;
}

static uint32_t us_to_deg(uint32_t us)
{
	if (us <= SERVO_PULSE_MIN) {
		return 0;
	}
	return (us - SERVO_PULSE_MIN) * 180U /
	       (SERVO_PULSE_MAX - SERVO_PULSE_MIN);
}

/* 全局速度 -> 每 tick 步进脉宽 */
static uint16_t speed_to_step(void)
{
	if (speed_deg_s == 0) {
		return 0;
	}
	uint32_t step = speed_deg_s * US_PER_DEG * MOTION_TICK_MS / 1000U;

	return MAX(step, 1U);
}

static int pwm_write(uint32_t ch, uint32_t pulse_us)
{
	return pwm_set(pca9685, ch, PWM_USEC(SERVO_PERIOD_USEC),
		       PWM_USEC(pulse_us), 0);
}

/*
 * 静默下发核心：设置通道目标。step_us: 0=瞬时，否则每 tick 步进量。
 * 首次激活的通道直接跳到目标（当前位置未知，慢速插值无意义）。
 * 不做日志输出，供 shell 命令与 Motion 框架共用。
 */
static int servo_apply_us(uint32_t ch, uint32_t pulse_us, uint16_t step_us)
{
	int ret = 0;

	if (!device_is_ready(pca9685)) {
		return -ENODEV;
	}
	if (ch >= SERVO_CHANNEL_CNT) {
		return -EINVAL;
	}

	pulse_us = CLAMP(pulse_us, SERVO_PULSE_MIN, SERVO_PULSE_MAX);

	k_mutex_lock(&servo_lock, K_FOREVER);
	struct servo_chan *c = &chans[ch];

	if (!c->active) {
		ret = pwm_write(ch, pulse_us);
		if (ret == 0) {
			c->active = true;
			c->cur_us = pulse_us;
			c->tgt_us = pulse_us;
			c->step_us = 0;
		}
	} else {
		c->tgt_us = pulse_us;
		c->step_us = step_us;
	}
	k_mutex_unlock(&servo_lock);

	return ret;
}

/*
 * 设置通道目标（带 shell 日志）。step_us: 0=瞬时，否则每 tick 步进量。
 */
static int servo_set_target(const struct shell *sh, uint32_t ch,
			    uint32_t pulse_us, uint16_t step_us)
{
	/* 手动命令优先：停止正在运行的 Motion，避免循环动作覆盖 */
	kame_motion_stop();

	if (!device_is_ready(pca9685)) {
		shell_error(sh, "PCA9685 driver not ready（先 spider scan 排查，"
				"接好线后按 RESET）");
		return -ENODEV;
	}
	if (ch >= SERVO_CHANNEL_CNT) {
		shell_error(sh, "channel %u out of range (0~15)", ch);
		return -EINVAL;
	}

	pulse_us = CLAMP(pulse_us, SERVO_PULSE_MIN, SERVO_PULSE_MAX);

	int ret = servo_apply_us(ch, pulse_us, step_us);

	if (ret != 0) {
		shell_error(sh, "pwm_set failed: %d", ret);
	} else {
		shell_print(sh, "ch%u -> %u us (%u deg)%s", ch, pulse_us,
			    us_to_deg(pulse_us),
			    step_us ? "" : " [instant]");
	}
	return ret;
}

/*
 * Motion / Pose 框架的舵机后端钩子（声明见 kame_pose.h）。
 * 由 servo_id 经标定表映射到通道，按全局速度平滑移动；不写死通道号。
 */
int kame_servo_apply(kame_servo_id_t id, uint8_t deg)
{
	const struct kame_servo_cal *s = kame_servo_get(id);

	if (s == NULL || s->ch == KAME_SERVO_CH_UNASSIGNED) {
		return -EINVAL;
	}

	uint8_t clamped = kame_servo_clamp(id, deg);

	return servo_apply_us(s->ch, deg_to_us(clamped), speed_to_step());
}

static int servo_off(const struct shell *sh, uint32_t ch)
{
	int ret;

	kame_motion_stop();

	if (ch >= SERVO_CHANNEL_CNT) {
		shell_error(sh, "channel %u out of range (0~15)", ch);
		return -EINVAL;
	}

	k_mutex_lock(&servo_lock, K_FOREVER);
	ret = pwm_write(ch, 0);
	chans[ch].active = false;
	k_mutex_unlock(&servo_lock);

	if (ret != 0) {
		shell_error(sh, "pwm_set failed: %d", ret);
	} else {
		shell_print(sh, "ch%u off", ch);
	}
	return ret;
}

/* 50 Hz 插值引擎：把各通道 cur_us 逐步推向 tgt_us */
static void motion_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	for (;;) {
		k_mutex_lock(&servo_lock, K_FOREVER);
		for (uint32_t ch = 0; ch < SERVO_CHANNEL_CNT; ch++) {
			struct servo_chan *sc = &chans[ch];

			if (!sc->active || sc->cur_us == sc->tgt_us) {
				continue;
			}

			uint16_t step = sc->step_us;
			uint16_t next;

			if (step == 0) {
				next = sc->tgt_us;
			} else if (sc->cur_us < sc->tgt_us) {
				next = MIN(sc->cur_us + step, sc->tgt_us);
			} else {
				next = (sc->cur_us - sc->tgt_us > step) ?
				       sc->cur_us - step : sc->tgt_us;
			}

			if (pwm_write(ch, next) == 0) {
				sc->cur_us = next;
			}
		}
		k_mutex_unlock(&servo_lock);
		k_msleep(MOTION_TICK_MS);
	}
}

K_THREAD_DEFINE(motion_tid, 2048, motion_thread, NULL, NULL, NULL, 5, 0, 0);

/* ---------------- I2C 诊断命令 ---------------- */

static bool i2c_addr_present(uint8_t addr)
{
	uint8_t dummy;

	/* 读 1 字节探测：nRF52 TWIM 不支持 0 长度传输 */
	return i2c_read(i2c_bus, &dummy, 1, addr) == 0;
}

static int cmd_scan(const struct shell *sh, size_t argc, char **argv)
{
	int found = 0;

	if (!device_is_ready(i2c_bus)) {
		shell_error(sh, "i2c0 not ready");
		return -ENODEV;
	}

	shell_print(sh, "Scanning I2C0 (SDA=P0.26 SCL=P0.27) 0x03..0x77 ...");
	shell_print(sh, "     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f");

	for (uint8_t base = 0; base <= 0x70; base += 16) {
		char line[64];
		int pos = snprintf(line, sizeof(line), "%02x:", base);

		for (uint8_t off = 0; off < 16; off++) {
			uint8_t addr = base + off;

			if (addr < 0x03 || addr > 0x77) {
				pos += snprintf(line + pos, sizeof(line) - pos, "   ");
			} else if (i2c_addr_present(addr)) {
				pos += snprintf(line + pos, sizeof(line) - pos, " %02x", addr);
				found++;
			} else {
				pos += snprintf(line + pos, sizeof(line) - pos, " --");
			}
		}
		shell_print(sh, "%s", line);
	}

	if (found == 0) {
		shell_warn(sh, "No I2C device found!");
	} else {
		shell_print(sh, "Found %d device(s).%s", found,
			    i2c_addr_present(PCA9685_DEFAULT_ADDR) ?
			    " PCA9685 @0x40 online." : "");
	}
	return 0;
}

static int cmd_check(const struct shell *sh, size_t argc, char **argv)
{
	uint8_t addr = PCA9685_DEFAULT_ADDR;
	uint8_t mode1, mode2, reg;
	int ret;

	if (argc > 1) {
		addr = (uint8_t)strtoul(argv[1], NULL, 0);
	}

	reg = PCA9685_REG_MODE1;
	ret = i2c_write_read(i2c_bus, addr, &reg, 1, &mode1, 1);
	if (ret != 0) {
		shell_error(sh, "Read MODE1 @0x%02x failed: %d", addr, ret);
		return ret;
	}
	reg = PCA9685_REG_MODE2;
	ret = i2c_write_read(i2c_bus, addr, &reg, 1, &mode2, 1);
	if (ret != 0) {
		shell_error(sh, "Read MODE2 @0x%02x failed: %d", addr, ret);
		return ret;
	}

	shell_print(sh, "PCA9685 @0x%02x: MODE1=0x%02x MODE2=0x%02x", addr, mode1, mode2);
	shell_print(sh, "  AI=%d SLEEP=%d RESTART=%d | OUTDRV=%s",
		    !!(mode1 & BIT(5)), !!(mode1 & BIT(4)), !!(mode1 & BIT(7)),
		    (mode2 & BIT(2)) ? "totem-pole" : "open-drain");
	return 0;
}

/* ---------------- 舵机命令 ---------------- */

static int cmd_servo(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t ch = strtoul(argv[1], NULL, 0);
	uint32_t us = strtoul(argv[2], NULL, 0);

	return servo_set_target(sh, ch, us, speed_to_step());
}

static int cmd_angle(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t ch = strtoul(argv[1], NULL, 0);
	uint32_t deg = strtoul(argv[2], NULL, 0);

	if (deg > 180) {
		shell_error(sh, "angle %u out of range (0~180)", deg);
		return -EINVAL;
	}
	return servo_set_target(sh, ch, deg_to_us(deg), speed_to_step());
}

/* spider multi 0:90 4:45 ... 多通道同一 tick 同步开始移动 */
static int cmd_multi(const struct shell *sh, size_t argc, char **argv)
{
	int ret = 0;

	for (size_t i = 1; i < argc; i++) {
		char *colon = strchr(argv[i], ':');

		if (colon == NULL) {
			shell_error(sh, "bad arg '%s' (expect <ch>:<deg>)", argv[i]);
			return -EINVAL;
		}

		uint32_t ch = strtoul(argv[i], NULL, 0);
		uint32_t deg = strtoul(colon + 1, NULL, 0);

		if (deg > 180) {
			shell_error(sh, "angle %u out of range (0~180)", deg);
			return -EINVAL;
		}

		ret = servo_set_target(sh, ch, deg_to_us(deg), speed_to_step());
		if (ret != 0) {
			break;
		}
	}
	return ret;
}

/* 所有已激活通道转到同一角度 */
static int cmd_all(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t deg = strtoul(argv[1], NULL, 0);
	int count = 0;

	if (deg > 180) {
		shell_error(sh, "angle %u out of range (0~180)", deg);
		return -EINVAL;
	}

	for (uint32_t ch = 0; ch < SERVO_CHANNEL_CNT; ch++) {
		if (chans[ch].active) {
			servo_set_target(sh, ch, deg_to_us(deg), speed_to_step());
			count++;
		}
	}
	if (count == 0) {
		shell_warn(sh, "no active channel yet (use angle/servo first)");
	}
	return 0;
}

/* spider move <ch> <deg> <ms>：在 ms 时长内匀速到位 */
static int cmd_move(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t ch = strtoul(argv[1], NULL, 0);
	uint32_t deg = strtoul(argv[2], NULL, 0);
	uint32_t ms = strtoul(argv[3], NULL, 0);

	if (deg > 180) {
		shell_error(sh, "angle %u out of range (0~180)", deg);
		return -EINVAL;
	}
	if (ch >= SERVO_CHANNEL_CNT) {
		shell_error(sh, "channel %u out of range (0~15)", ch);
		return -EINVAL;
	}

	uint32_t tgt = deg_to_us(deg);
	uint16_t step = 0;

	if (ms >= MOTION_TICK_MS && chans[ch].active) {
		uint32_t ticks = ms / MOTION_TICK_MS;
		uint32_t dist = (chans[ch].cur_us > tgt) ?
				chans[ch].cur_us - tgt : tgt - chans[ch].cur_us;

		step = MAX(DIV_ROUND_UP(dist, ticks), 1U);
	}
	return servo_set_target(sh, ch, tgt, step);
}

static int cmd_speed(const struct shell *sh, size_t argc, char **argv)
{
	if (argc > 1) {
		speed_deg_s = strtoul(argv[1], NULL, 0);
	}
	if (speed_deg_s == 0) {
		shell_print(sh, "speed: instant");
	} else {
		shell_print(sh, "speed: %u deg/s", speed_deg_s);
	}
	return 0;
}

static int cmd_status(const struct shell *sh, size_t argc, char **argv)
{
	int count = 0;

	shell_print(sh, "PCA9685 driver: %s | speed: %u deg/s%s",
		    device_is_ready(pca9685) ? "ready" : "NOT READY",
		    speed_deg_s, speed_deg_s == 0 ? " (instant)" : "");

	for (uint32_t ch = 0; ch < SERVO_CHANNEL_CNT; ch++) {
		struct servo_chan *c = &chans[ch];

		if (!c->active) {
			continue;
		}
		shell_print(sh, "  ch%-2u cur=%4u us (%3u deg)  tgt=%4u us (%3u deg)%s",
			    ch, c->cur_us, us_to_deg(c->cur_us),
			    c->tgt_us, us_to_deg(c->tgt_us),
			    c->cur_us == c->tgt_us ? "" : "  [moving]");
		count++;
	}
	if (count == 0) {
		shell_print(sh, "  (no active channel)");
	}
	return 0;
}

static int cmd_off(const struct shell *sh, size_t argc, char **argv)
{
	return servo_off(sh, strtoul(argv[1], NULL, 0));
}

static int cmd_offall(const struct shell *sh, size_t argc, char **argv)
{
	for (uint32_t ch = 0; ch < SERVO_CHANNEL_CNT; ch++) {
		if (chans[ch].active) {
			servo_off(sh, ch);
		}
	}
	return 0;
}

/* ---------------- Kame 整机姿态命令 ---------------- */

/*
 * 让 8 个舵机进入某姿态：splay=false 为标准站姿，splay=true 为叉腿站立。
 * 所有舵机同一 tick 同步开始移动，按全局速度限制平滑到位。
 */
static int kame_apply_pose(const struct shell *sh, bool splay, uint8_t delta)
{
	if (!device_is_ready(pca9685)) {
		shell_error(sh, "PCA9685 driver not ready（先 spider scan 排查，"
				"接好线后按 RESET）");
		return -ENODEV;
	}

	uint16_t step = speed_to_step();

	for (size_t i = 0; i < KAME_SERVO_COUNT; i++) {
		const struct kame_servo_cal *s = &kame_servos[i];
		uint8_t deg = splay ? kame_servo_splay_deg((kame_servo_id_t)i, delta)
				    : s->stand_deg;

		servo_set_target(sh, s->ch, deg_to_us(deg), step);
	}
	return 0;
}

static int cmd_stand(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "Kame -> 标准站姿");
	return kame_apply_pose(sh, false, 0);
}

static int cmd_splay(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t delta = KAME_SPLAY_DEFAULT_DEG;

	if (argc > 1) {
		delta = strtoul(argv[1], NULL, 0);
		if (delta > 90) {
			shell_error(sh, "delta %u out of range (0~90)", delta);
			return -EINVAL;
		}
	}

	shell_print(sh, "Kame -> 叉腿站立（前腿前 / 后腿后，髋 ±%u°）", delta);
	return kame_apply_pose(sh, true, (uint8_t)delta);
}

/* ---------------- Motion Framework 命令 ---------------- */

/* speed [ms]：查看/设置全局动作节拍 motion_step_delay_ms */
static int cmd_motion_speed(const struct shell *sh, size_t argc, char **argv)
{
	if (argc > 1) {
		uint32_t ms = strtoul(argv[1], NULL, 0);

		if (ms == 0) {
			shell_error(sh, "delay must be > 0 ms");
			return -EINVAL;
		}
		kame_motion_set_delay(ms);
	}
	shell_print(sh, "motion_step_delay_ms = %u", kame_motion_get_delay());
	return 0;
}

/* stop：停止当前动作并直接恢复立正(STAND) 姿势 */
static int cmd_motion_stop(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc); ARG_UNUSED(argv);
	kame_motion_request(KAME_MOTION_STAND, KAME_LEG_NONE);
	shell_print(sh, "stop -> stand");
	return 0;
}

/*
 * 通用动作处理：argv[0] 为动作名（顶层命令关键字），
 * 需腿参数的动作（lift / wave）从 argv[1] 取 L1/R1/L2/R2。
 */
static int cmd_motion_run(const struct shell *sh, size_t argc, char **argv)
{
	kame_motion_id_t id = kame_motion_from_name(argv[0]);

	if (id == KAME_MOTION_NONE) {
		shell_error(sh, "unknown motion '%s'", argv[0]);
		return -EINVAL;
	}

	kame_leg_id_t leg = KAME_LEG_NONE;

	if (kame_motion_needs_leg(id)) {
		if (argc < 2) {
			shell_error(sh, "'%s' needs leg arg: L1/R1/L2/R2", argv[0]);
			return -EINVAL;
		}
		leg = kame_leg_from_name(argv[1]);
		if (leg == KAME_LEG_NONE) {
			shell_error(sh, "bad leg '%s' (expect L1/R1/L2/R2)", argv[1]);
			return -EINVAL;
		}
	}

	kame_motion_request(id, leg);

	if (leg != KAME_LEG_NONE) {
		shell_print(sh, "motion -> %s %s%s", argv[0],
			    kame_legs[leg].name,
			    kame_motion_is_cyclic(id) ? " (cyclic)" : "");
	} else {
		shell_print(sh, "motion -> %s%s", argv[0],
			    kame_motion_is_cyclic(id) ? " (cyclic)" : "");
	}
	return 0;
}

/*
 * 顶层动作命令（精简，无前缀，便于串口直接输入）。
 * 静态动作与循环动作统一走 cmd_motion_run；speed/stop 单独处理。
 * lift / wave 需要腿参数(可选项 1，由 cmd_motion_run 校验)。
 */
SHELL_CMD_ARG_REGISTER(speed, NULL,
	"动作节拍(ms)\nUsage: speed [ms]", cmd_motion_speed, 1, 1);
SHELL_CMD_ARG_REGISTER(stop, NULL,
	"停止并恢复立正\nUsage: stop", cmd_motion_stop, 1, 0);
SHELL_CMD_ARG_REGISTER(stand, NULL, "站立", cmd_motion_run, 1, 0);
SHELL_CMD_ARG_REGISTER(splay, NULL, "叉腿站立", cmd_motion_run, 1, 0);
SHELL_CMD_ARG_REGISTER(lift, NULL,
	"抬腿并保持(先进三脚架)\nUsage: lift <L1|R1|L2|R2>",
	cmd_motion_run, 1, 1);
SHELL_CMD_ARG_REGISTER(front_down, NULL, "前身趴下", cmd_motion_run, 1, 0);
SHELL_CMD_ARG_REGISTER(rear_down, NULL, "后身趴下", cmd_motion_run, 1, 0);
SHELL_CMD_ARG_REGISTER(full_down, NULL, "整体趴下", cmd_motion_run, 1, 0);
SHELL_CMD_ARG_REGISTER(forward, NULL, "前进(循环)", cmd_motion_run, 1, 0);
SHELL_CMD_ARG_REGISTER(backward, NULL, "后退(循环)", cmd_motion_run, 1, 0);
SHELL_CMD_ARG_REGISTER(turn_left, NULL, "弧线左转(循环)", cmd_motion_run, 1, 0);
SHELL_CMD_ARG_REGISTER(turn_right, NULL, "弧线右转(循环)", cmd_motion_run, 1, 0);
SHELL_CMD_ARG_REGISTER(spin_left, NULL, "原地左旋(循环)", cmd_motion_run, 1, 0);
SHELL_CMD_ARG_REGISTER(spin_right, NULL, "原地右旋(循环)", cmd_motion_run, 1, 0);
SHELL_CMD_ARG_REGISTER(nod, NULL, "点头(循环)", cmd_motion_run, 1, 0);
SHELL_CMD_ARG_REGISTER(shake, NULL, "摇头(循环)", cmd_motion_run, 1, 0);
SHELL_CMD_ARG_REGISTER(roll, NULL, "横滚摆动(循环)", cmd_motion_run, 1, 0);
SHELL_CMD_ARG_REGISTER(wave, NULL,
	"招手(循环,先进三脚架; stop 恢复立正)\nUsage: wave <L1|R1|L2|R2>",
	cmd_motion_run, 1, 1);

/* 顶层舵机角度命令（无 spider 前缀，便于串口直接输入） */
SHELL_CMD_ARG_REGISTER(angle, NULL,
	"设通道角度\nUsage: angle <ch 0-15> <deg 0-180>",
	cmd_angle, 3, 0);

SHELL_STATIC_SUBCMD_SET_CREATE(spider_cmds,
	SHELL_CMD_ARG(scan, NULL,
		"扫描 I2C0 总线\nUsage: spider scan",
		cmd_scan, 1, 0),
	SHELL_CMD_ARG(check, NULL,
		"读 PCA9685 MODE 寄存器\nUsage: spider check [addr]",
		cmd_check, 1, 1),
	SHELL_CMD_ARG(servo, NULL,
		"设通道脉宽(us)\nUsage: spider servo <ch 0-15> <pulse_us>",
		cmd_servo, 3, 0),
	SHELL_CMD_ARG(multi, NULL,
		"多通道同步设角度\nUsage: spider multi <ch:deg> [<ch:deg> ...]\n"
		"e.g. spider multi 0:90 4:45",
		cmd_multi, 2, SERVO_CHANNEL_CNT - 1),
	SHELL_CMD_ARG(all, NULL,
		"所有已激活通道设同一角度\nUsage: spider all <deg 0-180>",
		cmd_all, 2, 0),
	SHELL_CMD_ARG(move, NULL,
		"指定时长匀速转动\nUsage: spider move <ch> <deg> <time_ms>",
		cmd_move, 4, 0),
	SHELL_CMD_ARG(speed, NULL,
		"查看/设置速度限制(度/秒, 0=瞬时)\nUsage: spider speed [deg_per_s]",
		cmd_speed, 1, 1),
	SHELL_CMD_ARG(status, NULL,
		"显示各通道状态\nUsage: spider status",
		cmd_status, 1, 0),
	SHELL_CMD_ARG(off, NULL,
		"关闭单通道\nUsage: spider off <ch 0-15>",
		cmd_off, 2, 0),
	SHELL_CMD_ARG(offall, NULL,
		"关闭全部通道\nUsage: spider offall",
		cmd_offall, 1, 0),
	SHELL_CMD_ARG(stand, NULL,
		"进入标准站姿(按标定表)\nUsage: spider stand",
		cmd_stand, 1, 0),
	SHELL_CMD_ARG(splay, NULL,
		"叉腿站立(前腿前/后腿后)\nUsage: spider splay [delta 0-90, 默认30]",
		cmd_splay, 1, 1),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(spider, &spider_cmds, "PCA9685 舵机控制命令", NULL);

int main(void)
{
	printk("\n*** spider: PCA9685 interactive servo console ***\n");
	printk("I2C0: SDA=P0.26 SCL=P0.27, expecting PCA9685 @0x40\n");

	if (!device_is_ready(i2c_bus)) {
		printk("ERROR: i2c0 not ready!\n");
		return 0;
	}

	if (device_is_ready(pca9685)) {
		printk("PCA9685 driver ready.\n");
		/* 上电自动复位：进入标准站姿（等价于串口 stand 命令）。
		 * 各通道首次激活时瞬时到位，从而把机器人复位到已知姿态。 */
		kame_motion_request(KAME_MOTION_STAND, KAME_LEG_NONE);
		printk("Boot reset: Kame -> 标准站姿 (stand).\n");
	} else {
		printk("WARN: PCA9685 not detected at boot. "
		       "Use 'spider scan' to debug wiring.\n");
	}

	printk("Type 'spider help' or press Tab to list commands.\n");
	return 0;
}
