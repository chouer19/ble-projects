/*
 * spider - 串口交互控制 PCA9685 舵机（多通道 + 平滑运动）+ BLE 遥控
 *
 * I2C 总线与引脚由板级 overlay 定义（DK: i2c0 P0.26/P0.27；XIAO: i2c1 D4/D5）。
 * PCA9685 地址 0x40。
 *
 * Shell 命令（DK: J-Link vcom 115200；XIAO: USB CDC，支持 Tab 补全）：
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
 * BLE：广播名 SpiderBod，GATT cmd 写入等价于上述动作/速度命令。
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>
#include <stdlib.h>
#include <string.h>

#include "ble_remote.h"
#include "kame_servo_cal.h"
#include "kame_pose.h"
#include "kame_motion.h"
#include "spider_control.h"
#include "spider_servo.h"

#define KAME_SPLAY_DEFAULT_DEG 30U

#define PCA9685_DEFAULT_ADDR 0x40
#define PCA9685_REG_MODE1    0x00
#define PCA9685_REG_MODE2    0x01

#define MOTION_TICK_MS 20U

static const struct device *i2c_bus =
	DEVICE_DT_GET(DT_BUS(DT_NODELABEL(pca9685)));

static int servo_set_target_shell(const struct shell *sh, uint32_t ch,
				  uint32_t pulse_us, uint16_t step_us)
{
	if (!spider_servo_is_ready()) {
		shell_error(sh, "PCA9685 driver not ready（先 spider scan 排查，"
				"接好线后按 RESET）");
		return -ENODEV;
	}
	if (ch >= SPIDER_SERVO_CHANNEL_CNT) {
		shell_error(sh, "channel %u out of range (0~15)", ch);
		return -EINVAL;
	}

	int ret = spider_servo_set_target(ch, pulse_us, step_us);

	if (ret != 0) {
		shell_error(sh, "pwm_set failed: %d", ret);
	} else {
		shell_print(sh, "ch%u -> %u us (%u deg)%s", ch, pulse_us,
			    spider_servo_us_to_deg(pulse_us),
			    step_us ? "" : " [instant]");
	}
	return ret;
}

static int servo_off_shell(const struct shell *sh, uint32_t ch)
{
	int ret;

	if (ch >= SPIDER_SERVO_CHANNEL_CNT) {
		shell_error(sh, "channel %u out of range (0~15)", ch);
		return -EINVAL;
	}

	ret = spider_servo_off(ch);
	if (ret != 0) {
		shell_error(sh, "pwm_set failed: %d", ret);
	} else {
		shell_print(sh, "ch%u off", ch);
	}
	return ret;
}

/* ---------------- I2C 诊断命令 ---------------- */

static bool i2c_addr_present(uint8_t addr)
{
	uint8_t dummy;

	return i2c_read(i2c_bus, &dummy, 1, addr) == 0;
}

static int cmd_scan(const struct shell *sh, size_t argc, char **argv)
{
	int found = 0;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!device_is_ready(i2c_bus)) {
		shell_error(sh, "I2C bus not ready");
		return -ENODEV;
	}

	shell_print(sh, "Scanning I2C (%s) 0x03..0x77 ...", i2c_bus->name);
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

	return servo_set_target_shell(sh, ch, us, spider_servo_speed_to_step());
}

static int cmd_angle(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t ch = strtoul(argv[1], NULL, 0);
	uint32_t deg = strtoul(argv[2], NULL, 0);

	if (deg > 180) {
		shell_error(sh, "angle %u out of range (0~180)", deg);
		return -EINVAL;
	}
	return servo_set_target_shell(sh, ch, spider_servo_deg_to_us(deg),
				      spider_servo_speed_to_step());
}

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

		ret = servo_set_target_shell(sh, ch, spider_servo_deg_to_us(deg),
					     spider_servo_speed_to_step());
		if (ret != 0) {
			break;
		}
	}
	return ret;
}

static int cmd_all(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t deg = strtoul(argv[1], NULL, 0);
	int count = 0;
	struct spider_servo_chan_state st;

	if (deg > 180) {
		shell_error(sh, "angle %u out of range (0~180)", deg);
		return -EINVAL;
	}

	for (uint32_t ch = 0; ch < SPIDER_SERVO_CHANNEL_CNT; ch++) {
		if (spider_servo_get_chan(ch, &st) && st.active) {
			servo_set_target_shell(sh, ch, spider_servo_deg_to_us(deg),
					       spider_servo_speed_to_step());
			count++;
		}
	}
	if (count == 0) {
		shell_warn(sh, "no active channel yet (use angle/servo first)");
	}
	return 0;
}

static int cmd_move(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t ch = strtoul(argv[1], NULL, 0);
	uint32_t deg = strtoul(argv[2], NULL, 0);
	uint32_t ms = strtoul(argv[3], NULL, 0);
	struct spider_servo_chan_state st;

	if (deg > 180) {
		shell_error(sh, "angle %u out of range (0~180)", deg);
		return -EINVAL;
	}
	if (ch >= SPIDER_SERVO_CHANNEL_CNT) {
		shell_error(sh, "channel %u out of range (0~15)", ch);
		return -EINVAL;
	}

	uint32_t tgt = spider_servo_deg_to_us(deg);
	uint16_t step = 0;

	if (ms >= MOTION_TICK_MS && spider_servo_get_chan(ch, &st) && st.active) {
		uint32_t ticks = ms / MOTION_TICK_MS;
		uint32_t dist = (st.cur_us > tgt) ? st.cur_us - tgt : tgt - st.cur_us;

		step = MAX(DIV_ROUND_UP(dist, ticks), 1U);
	}
	return servo_set_target_shell(sh, ch, tgt, step);
}

static int cmd_speed(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t speed = spider_control_get_servo_speed_deg_s();

	if (argc > 1) {
		speed = strtoul(argv[1], NULL, 0);
		spider_control_set_servo_speed_deg_s(speed);
	}
	if (speed == 0) {
		shell_print(sh, "speed: instant");
	} else {
		shell_print(sh, "speed: %u deg/s", speed);
	}
	return 0;
}

static int cmd_status(const struct shell *sh, size_t argc, char **argv)
{
	int count = 0;
	uint32_t speed = spider_control_get_servo_speed_deg_s();

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "PCA9685 driver: %s | speed: %u deg/s%s",
		    spider_servo_is_ready() ? "ready" : "NOT READY",
		    speed, speed == 0 ? " (instant)" : "");

	for (uint32_t ch = 0; ch < SPIDER_SERVO_CHANNEL_CNT; ch++) {
		struct spider_servo_chan_state c;

		if (!spider_servo_get_chan(ch, &c) || !c.active) {
			continue;
		}
		shell_print(sh, "  ch%-2u cur=%4u us (%3u deg)  tgt=%4u us (%3u deg)%s",
			    ch, c.cur_us, spider_servo_us_to_deg(c.cur_us),
			    c.tgt_us, spider_servo_us_to_deg(c.tgt_us),
			    c.moving ? "  [moving]" : "");
		count++;
	}
	if (count == 0) {
		shell_print(sh, "  (no active channel)");
	}
	return 0;
}

static int cmd_off(const struct shell *sh, size_t argc, char **argv)
{
	return servo_off_shell(sh, strtoul(argv[1], NULL, 0));
}

static int cmd_offall(const struct shell *sh, size_t argc, char **argv)
{
	struct spider_servo_chan_state st;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	for (uint32_t ch = 0; ch < SPIDER_SERVO_CHANNEL_CNT; ch++) {
		if (spider_servo_get_chan(ch, &st) && st.active) {
			servo_off_shell(sh, ch);
		}
	}
	return 0;
}

/* ---------------- Kame 整机姿态命令 ---------------- */

static int kame_apply_pose(const struct shell *sh, bool splay, uint8_t delta)
{
	if (!spider_servo_is_ready()) {
		shell_error(sh, "PCA9685 driver not ready（先 spider scan 排查，"
				"接好线后按 RESET）");
		return -ENODEV;
	}

	uint16_t step = spider_servo_speed_to_step();

	for (size_t i = 0; i < KAME_SERVO_COUNT; i++) {
		const struct kame_servo_cal *s = &kame_servos[i];
		uint8_t deg = splay ? kame_servo_splay_deg((kame_servo_id_t)i, delta)
				    : s->stand_deg;

		servo_set_target_shell(sh, s->ch, spider_servo_deg_to_us(deg), step);
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

static int cmd_motion_speed(const struct shell *sh, size_t argc, char **argv)
{
	if (argc > 1) {
		uint32_t ms = strtoul(argv[1], NULL, 0);

		if (ms == 0) {
			shell_error(sh, "delay must be > 0 ms");
			return -EINVAL;
		}
		spider_control_set_motion_delay_ms(ms);
	}
	shell_print(sh, "motion_step_delay_ms = %u",
		    spider_control_get_motion_delay_ms());
	return 0;
}

static int cmd_motion_stop(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	spider_control_stop();
	shell_print(sh, "stop -> stand");
	return 0;
}

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

	spider_control_motion(id, leg);

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

SHELL_CMD_ARG_REGISTER(angle, NULL,
	"设通道角度\nUsage: angle <ch 0-15> <deg 0-180>",
	cmd_angle, 3, 0);

SHELL_STATIC_SUBCMD_SET_CREATE(spider_cmds,
	SHELL_CMD_ARG(scan, NULL,
		"扫描 PCA9685 所在 I2C 总线\nUsage: spider scan",
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
		cmd_multi, 2, SPIDER_SERVO_CHANNEL_CNT - 1),
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
	printk("I2C bus %s, expecting PCA9685 @0x40\n", i2c_bus->name);

	if (!device_is_ready(i2c_bus)) {
		printk("ERROR: I2C bus not ready!\n");
		return 0;
	}

	if (spider_servo_is_ready()) {
		printk("PCA9685 driver ready.\n");
		spider_control_motion(KAME_MOTION_STAND, KAME_LEG_NONE);
		printk("Boot reset: Kame -> 标准站姿 (stand).\n");
	} else {
		printk("WARN: PCA9685 not detected at boot. "
		       "Use 'spider scan' to debug wiring.\n");
	}

	if (spider_ble_init() != 0) {
		printk("WARN: BLE init failed.\n");
	}

	printk("Type 'spider help' or press Tab to list commands.\n");
	return 0;
}
