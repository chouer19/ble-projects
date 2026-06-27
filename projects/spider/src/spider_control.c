/*
 * spider 统一命令分发
 */

#include <errno.h>

#include "kame_motion.h"
#include "spider_ble_protocol.h"
#include "spider_control.h"
#include "spider_servo.h"

int spider_control_motion(kame_motion_id_t id, kame_leg_id_t leg)
{
	if (id >= KAME_MOTION_COUNT) {
		return -EINVAL;
	}

	kame_motion_request(id, leg);
	return 0;
}

int spider_control_stop(void)
{
	kame_motion_request(KAME_MOTION_STAND, KAME_LEG_NONE);
	return 0;
}

int spider_control_set_motion_delay_ms(uint32_t ms)
{
	kame_motion_set_delay(ms);
	return 0;
}

uint32_t spider_control_get_motion_delay_ms(void)
{
	return kame_motion_get_delay();
}

int spider_control_set_servo_speed_deg_s(uint32_t deg_s)
{
	spider_servo_set_speed_deg_s(deg_s);
	return 0;
}

uint32_t spider_control_get_servo_speed_deg_s(void)
{
	return spider_servo_get_speed_deg_s();
}

static bool ble_leg_valid(kame_motion_id_t id, uint8_t leg_raw, kame_leg_id_t *leg_out)
{
	if (!kame_motion_needs_leg(id)) {
		*leg_out = KAME_LEG_NONE;
		return true;
	}

	if (leg_raw == SPIDER_BLE_LEG_NONE || leg_raw >= KAME_LEG_COUNT) {
		return false;
	}

	*leg_out = (kame_leg_id_t)leg_raw;
	return true;
}

int spider_control_ble_frame(const uint8_t *data, size_t len)
{
	if (data == NULL || len == 0) {
		return -EINVAL;
	}

	switch (data[0]) {
	case SPIDER_BLE_OP_MOTION:
		if (len != 3U) {
			return -EINVAL;
		}

		{
			kame_motion_id_t id = (kame_motion_id_t)data[1];
			kame_leg_id_t leg;

			if (id >= KAME_MOTION_COUNT) {
				return -EINVAL;
			}
			if (!ble_leg_valid(id, data[2], &leg)) {
				return -EINVAL;
			}

			return spider_control_motion(id, leg);
		}

	case SPIDER_BLE_OP_STOP:
		if (len != 1U) {
			return -EINVAL;
		}
		return spider_control_stop();

	case SPIDER_BLE_OP_SET_MOTION_DELAY:
		if (len != 3U) {
			return -EINVAL;
		}

		{
			uint32_t ms = (uint32_t)data[1] | ((uint32_t)data[2] << 8);

			if (ms < SPIDER_BLE_MOTION_DELAY_MIN_MS ||
			    ms > SPIDER_BLE_MOTION_DELAY_MAX_MS) {
				return -EINVAL;
			}
			return spider_control_set_motion_delay_ms(ms);
		}

	case SPIDER_BLE_OP_SET_SERVO_SPEED:
		if (len != 3U) {
			return -EINVAL;
		}

		{
			uint32_t deg_s = (uint32_t)data[1] | ((uint32_t)data[2] << 8);

			if (deg_s > SPIDER_BLE_SERVO_SPEED_MAX_DEG_S) {
				return -EINVAL;
			}
			return spider_control_set_servo_speed_deg_s(deg_s);
		}

	default:
		return -EINVAL;
	}
}
