/*
 * spider 舵机后端：50 Hz 插值 + PCA9685 下发
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>

#include "kame_motion.h"
#include "spider_servo.h"

#define SERVO_PERIOD_USEC 20000U
#define SERVO_PULSE_MIN   500U
#define SERVO_PULSE_MAX   2500U
#define MOTION_TICK_MS    20U
#define US_PER_DEG        ((SERVO_PULSE_MAX - SERVO_PULSE_MIN) / 180U)

static const struct device *pca9685 = DEVICE_DT_GET(DT_NODELABEL(pca9685));

struct servo_chan {
	bool active;
	uint16_t cur_us;
	uint16_t tgt_us;
	uint16_t step_us;
};

static struct servo_chan chans[SPIDER_SERVO_CHANNEL_CNT];
static uint32_t speed_deg_s = SPIDER_SERVO_SPEED_DEFAULT_DEG_S;
static K_MUTEX_DEFINE(servo_lock);

uint32_t spider_servo_deg_to_us(uint32_t deg)
{
	return SERVO_PULSE_MIN +
	       (SERVO_PULSE_MAX - SERVO_PULSE_MIN) * deg / 180U;
}

uint32_t spider_servo_us_to_deg(uint32_t us)
{
	if (us <= SERVO_PULSE_MIN) {
		return 0;
	}
	return (us - SERVO_PULSE_MIN) * 180U /
	       (SERVO_PULSE_MAX - SERVO_PULSE_MIN);
}

uint32_t spider_servo_get_speed_deg_s(void)
{
	return speed_deg_s;
}

void spider_servo_set_speed_deg_s(uint32_t deg_s)
{
	speed_deg_s = deg_s;
}

uint16_t spider_servo_speed_to_step(void)
{
	if (speed_deg_s == 0) {
		return 0;
	}
	uint32_t step = speed_deg_s * US_PER_DEG * MOTION_TICK_MS / 1000U;

	return MAX(step, 1U);
}

bool spider_servo_is_ready(void)
{
	return device_is_ready(pca9685);
}

static int pwm_write(uint32_t ch, uint32_t pulse_us)
{
	return pwm_set(pca9685, ch, PWM_USEC(SERVO_PERIOD_USEC),
		       PWM_USEC(pulse_us), 0);
}

int spider_servo_apply_us(uint32_t ch, uint32_t pulse_us, uint16_t step_us)
{
	int ret = 0;

	if (!device_is_ready(pca9685)) {
		return -ENODEV;
	}
	if (ch >= SPIDER_SERVO_CHANNEL_CNT) {
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

int spider_servo_set_target(uint32_t ch, uint32_t pulse_us, uint16_t step_us)
{
	kame_motion_stop();
	return spider_servo_apply_us(ch, pulse_us, step_us);
}

int spider_servo_off(uint32_t ch)
{
	int ret;

	kame_motion_stop();

	if (ch >= SPIDER_SERVO_CHANNEL_CNT) {
		return -EINVAL;
	}

	k_mutex_lock(&servo_lock, K_FOREVER);
	ret = pwm_write(ch, 0);
	chans[ch].active = false;
	k_mutex_unlock(&servo_lock);

	return ret;
}

bool spider_servo_get_chan(uint32_t ch, struct spider_servo_chan_state *out)
{
	if (ch >= SPIDER_SERVO_CHANNEL_CNT || out == NULL) {
		return false;
	}

	k_mutex_lock(&servo_lock, K_FOREVER);
	const struct servo_chan *c = &chans[ch];

	out->active = c->active;
	out->cur_us = c->cur_us;
	out->tgt_us = c->tgt_us;
	out->moving = c->active && (c->cur_us != c->tgt_us);
	k_mutex_unlock(&servo_lock);

	return true;
}

int kame_servo_apply(kame_servo_id_t id, uint8_t deg)
{
	const struct kame_servo_cal *s = kame_servo_get(id);

	if (s == NULL || s->ch == KAME_SERVO_CH_UNASSIGNED) {
		return -EINVAL;
	}

	uint8_t clamped = kame_servo_clamp(id, deg);

	return spider_servo_apply_us(s->ch, spider_servo_deg_to_us(clamped),
				     spider_servo_speed_to_step());
}

static void motion_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	for (;;) {
		k_mutex_lock(&servo_lock, K_FOREVER);
		for (uint32_t ch = 0; ch < SPIDER_SERVO_CHANNEL_CNT; ch++) {
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
