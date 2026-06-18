/*
 * hello_nrf52840 — 最小 nRF52840 DK 示例
 * - LED0 闪烁
 * - UART 控制台输出 (printk)
 * - BLE 不可连接广播 (设备名 Hello_nRF52840)
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

#define LED0_NODE DT_ALIAS(led0)
#define SLEEP_TIME_MS 500

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_NAME_COMPLETE,
		      'H', 'e', 'l', 'l', 'o', '_', 'n', 'R', 'F', '5', '2', '8', '4', '0'),
};

int main(void)
{
	int ret;
	bool led_on = false;

	if (!gpio_is_ready_dt(&led)) {
		printk("LED device not ready\n");
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		printk("LED configure failed: %d\n", ret);
		return 0;
	}

	printk("\n=== hello_nrf52840 ===\n");
	printk("Board: nRF52840 DK\n");
	printk("Features: LED blink + UART console + BLE advertising\n\n");

	ret = bt_enable(NULL);
	if (ret) {
		printk("Bluetooth init failed (err %d)\n", ret);
		return 0;
	}
	printk("Bluetooth initialized\n");

	ret = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);
	if (ret) {
		printk("Advertising failed to start (err %d)\n", ret);
		return 0;
	}
	printk("BLE advertising started (non-connectable)\n");

	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			printk("LED toggle failed: %d\n", ret);
			return 0;
		}
		led_on = !led_on;
		printk("LED: %s\n", led_on ? "ON" : "OFF");
		k_msleep(SLEEP_TIME_MS);
	}

	return 0;
}
