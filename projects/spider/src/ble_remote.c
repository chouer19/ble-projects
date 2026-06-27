/*
 * spider BLE 遥控：Peripheral + GATT cmd characteristic
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/sys/printk.h>

#include "ble_remote.h"
#include "spider_ble_protocol.h"
#include "spider_control.h"

static struct bt_uuid_128 spider_svc_uuid = BT_UUID_INIT_128(SPIDER_BLE_SVC_UUID_VAL);
static struct bt_uuid_128 spider_cmd_uuid = BT_UUID_INIT_128(SPIDER_BLE_CHR_CMD_UUID_VAL);

static ssize_t cmd_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(flags);

	if (offset != 0U) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	/* 非法帧静默丢弃（玩具级，无 Notify） */
	(void)spider_control_ble_frame(buf, len);
	return len;
}

BT_GATT_SERVICE_DEFINE(spider_remote_svc,
	BT_GATT_PRIMARY_SERVICE(&spider_svc_uuid),
	BT_GATT_CHARACTERISTIC(&spider_cmd_uuid.uuid,
			       BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_WRITE,
			       NULL, cmd_write, NULL),
);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err != 0U) {
		printk("BLE connect failed (err %u)\n", err);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);
	printk("BLE disconnected (reason %u) -> auto stand\n", reason);
	spider_control_stop();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void bt_ready(int err)
{
	if (err != 0) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized (name: %s)\n", CONFIG_BT_DEVICE_NAME);

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err != 0) {
		printk("BLE advertising failed (err %d)\n", err);
		return;
	}

	printk("BLE advertising started (connectable)\n");
}

int spider_ble_init(void)
{
	return bt_enable(bt_ready);
}
