/** @file
 *  @brief HoG Service sample
 */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <zephyr/drivers/gpio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include "hog.h"

enum {
	HIDS_REMOTE_WAKE = BIT(0),
	HIDS_NORMALLY_CONNECTABLE = BIT(1),
};

struct hids_info {
	uint16_t version; /* version number of base USB HID Specification */
	uint8_t code; /* country HID Device hardware is localized for. */
	uint8_t flags;
} __packed;

struct hids_report {
	uint8_t id; // report id
	uint8_t type; // report type
} __packed;

static struct hids_info info = {
	.version = 0x0000,
	.code = 0x00,
	.flags = HIDS_NORMALLY_CONNECTABLE,
};

enum {
	HIDS_INPUT = 0x01,
	HIDS_OUTPUT = 0x02,
	HIDS_FEATURE = 0x03,
};

static struct hids_report input = {
	.id = 0x01,
	.type = HIDS_INPUT,
};

struct {
    // TODO: may want to add back some normal keyboard functionality from lautstarokfant-mbed
    //uint8_t key_codes[6];
    uint8_t media_key;
} hid_key_report;

const uint8_t MEDIA_KEY_CONFIGURATION   = 1 << 0;
const uint8_t MEDIA_KEY_PLAY_PAUSE      = 1 << 1;
const uint8_t MEDIA_KEY_SCAN_NEXT_TRACK = 1 << 2;
const uint8_t MEDIA_KEY_SCAN_PREV_TRACK = 1 << 3;
const uint8_t MEDIA_KEY_VOLUME_UP       = 1 << 4;
const uint8_t MEDIA_KEY_VOLUME_DOWN     = 1 << 5;
const uint8_t MEDIA_KEY_AC_FORWARD      = 1 << 6;
const uint8_t MEDIA_KEY_AC_BACK         = 1 << 7;

static uint8_t simulate_input;
static uint8_t ctrl_point;
static uint8_t report_map[] = {
    // https://devzone.nordicsemi.com/f/nordic-q-a/25351/hid-consumer-control-settings
    0x05, 0x0C,                     // Usage Page (Consumer)
    0x09, 0x01,                     // Usage (Consumer Control)
    0xA1, 0x01,                     // Collection (Application)
    0x85, 0x01,                     //     Report Id (1)
    0x15, 0x00,                     //     Logical minimum (0)
    0x25, 0x01,                     //     Logical maximum (1)
    0x75, 0x01,                     //     Report Size (1)
    0x95, 0x01,                     //     Report Count (1)

    0x0A, 0x83, 0x01,               //     Usage (AL Consumer Control Configuration)
    0x81, 0x02,                     //     Input (Data,Value,Relative,Bit Field)
    0x09, 0xCD,                     //     Usage (Play/Pause)
    0x81, 0x02,                     //     Input (Data,Value,Relative,Bit Field)
    0x09, 0xB5,                     //     Usage (Scan Next Track)
    0x81, 0x02,                     //     Input (Data,Value,Relative,Bit Field)
    0x09, 0xB6,                     //     Usage (Scan Previous Track)
    0x81, 0x02,                     //     Input (Data,Value,Relative,Bit Field)
    0x09, 0xEA,                     //     Usage (Volume Down)
    0x81, 0x02,                     //     Input (Data,Value,Relative,Bit Field)
    0x09, 0xE9,                     //     Usage (Volume Up)
    0x81, 0x02,                     //     Input (Data,Value,Relative,Bit Field)
    0x0A, 0x25, 0x02,               //     Usage (AC Forward)
    0x81, 0x02,                     //     Input (Data,Value,Relative,Bit Field)
    0x0A, 0x24, 0x02,               //     Usage (AC Back)
    0x81, 0x02,                     //     Input (Data,Value,Relative,Bit Field)
    0xC0                            // End Collection
};


static ssize_t read_info(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr, void *buf,
			  uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 sizeof(struct hids_info));
}

static ssize_t read_report_map(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, report_map,
				 sizeof(report_map));
}

static ssize_t read_report(struct bt_conn *conn,
			   const struct bt_gatt_attr *attr, void *buf,
			   uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 sizeof(struct hids_report));
}

static void input_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	simulate_input = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

static ssize_t read_input_report(struct bt_conn *conn,
				 const struct bt_gatt_attr *attr, void *buf,
				 uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, NULL, 0);
}

static ssize_t write_ctrl_point(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset,
				uint8_t flags)
{
	uint8_t *value = attr->user_data;

	if (offset + len > sizeof(ctrl_point)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(value + offset, buf, len);

	return len;
}

#if CONFIG_SAMPLE_BT_USE_AUTHENTICATION
/* Require encryption using authenticated link-key. */
#define SAMPLE_BT_PERM_READ BT_GATT_PERM_READ_AUTHEN
#define SAMPLE_BT_PERM_WRITE BT_GATT_PERM_WRITE_AUTHEN
#else
/* Require encryption. */
#define SAMPLE_BT_PERM_READ BT_GATT_PERM_READ_ENCRYPT
#define SAMPLE_BT_PERM_WRITE BT_GATT_PERM_WRITE_ENCRYPT
#endif

/* HID Service Declaration */
BT_GATT_SERVICE_DEFINE(hog_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_info, NULL, &info),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_report_map, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       SAMPLE_BT_PERM_READ,
			       read_input_report, NULL, NULL),
	BT_GATT_CCC(input_ccc_changed,
		    SAMPLE_BT_PERM_READ | SAMPLE_BT_PERM_WRITE),
	BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,
			   read_report, NULL, &input),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,
			       BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_WRITE,
			       NULL, write_ctrl_point, &ctrl_point),
);

static const struct gpio_dt_spec buttonVolDown = GPIO_DT_SPEC_GET(DT_NODELABEL(button_vol_down), gpios);
static const struct gpio_dt_spec buttonVolUp = GPIO_DT_SPEC_GET(DT_NODELABEL(button_vol_up), gpios);
static const struct gpio_dt_spec buttonPause = GPIO_DT_SPEC_GET(DT_NODELABEL(button_pause), gpios);
static const struct gpio_dt_spec buttonExtra = GPIO_DT_SPEC_GET(DT_NODELABEL(button_extra), gpios);
static const struct gpio_dt_spec buttonMaintenance = GPIO_DT_SPEC_GET(DT_NODELABEL(button_maintenance), gpios);
const struct gpio_dt_spec statusLed = GPIO_DT_SPEC_GET(DT_NODELABEL(led_status), gpios);
const struct gpio_dt_spec actionLed = GPIO_DT_SPEC_GET(DT_NODELABEL(led_action), gpios);

volatile bool buttons_active = false;

#define BUTTON_THROTTLE_MILLIS 300L
enum ACTION {
  NONE,
  INVALID,
  REPEAT_SINGLE_PRESS,
  RESET,
  VOLUME_UP,
  VOLUME_DOWN,
  PAUSE,
  PAGE_DOWN,
  PAGE_UP,
  DOUBLE_PAGE_UP,
  NEXT,
  PREVIOUS
};

struct BUTTON_STATE {
  const struct gpio_dt_spec* button;
  const int action;
  const int longPressAction;
  const bool canRepeat;
  const int repeatThrottle;
};

struct BUTTON_STATE buttonStates[] = {
  {&buttonVolDown, VOLUME_DOWN, REPEAT_SINGLE_PRESS, NONE, BUTTON_THROTTLE_MILLIS},
  {&buttonVolUp, VOLUME_UP, REPEAT_SINGLE_PRESS, NONE, BUTTON_THROTTLE_MILLIS},
  {&buttonPause, PAUSE, NONE, BUTTON_THROTTLE_MILLIS},
  {&buttonExtra, PREVIOUS, NONE, BUTTON_THROTTLE_MILLIS},
  {&buttonMaintenance, RESET, NONE, BUTTON_THROTTLE_MILLIS}
};

void hog_init(void)
{
}

void hog_button_loop(void)
{
    int64_t now;
    int action;
    bool anyPressed;
    uint8_t media_key;
    uint8_t key;
    uint8_t keyCount;
    static unsigned long lastAction = 0;
    static bool lastPressed = false;
    static bool lastLongPressed = false;

	gpio_pin_configure_dt(&buttonVolDown, GPIO_INPUT);
	gpio_pin_configure_dt(&buttonVolUp, GPIO_INPUT);
	gpio_pin_configure_dt(&buttonPause, GPIO_INPUT);
	gpio_pin_configure_dt(&buttonExtra, GPIO_INPUT);
	gpio_pin_configure_dt(&buttonMaintenance, GPIO_INPUT);

    memset(&hid_key_report, 0, sizeof(hid_key_report));

	for (;;) {
        now = k_uptime_get();
        action = NONE;
        anyPressed = false;
        for (unsigned int i = 0; i < sizeof(buttonStates) / sizeof(struct BUTTON_STATE); i++) {
            struct BUTTON_STATE buttonState = buttonStates[i];
            int currentState = gpio_pin_get_dt(buttonState.button) == 1;

            if (currentState) {
                // Light LED while any is pressed
                anyPressed = true;
                int thisAction = NONE;
                if (!lastPressed) {
                    // Immediately execute action on first press
                    thisAction = buttonState.action;
                    lastLongPressed = false;
                } else if ((now - lastAction) > buttonState.repeatThrottle) {
                    // Long press action
                    if (buttonState.longPressAction == REPEAT_SINGLE_PRESS) {
                        thisAction = buttonState.action;
                    } else if (!lastLongPressed && buttonState.longPressAction != NONE) {
                        thisAction = buttonState.longPressAction;
                        // We only want to do long press action once
                        if (!buttonState.canRepeat) {
                            lastLongPressed = true;
                        }
                    }
                }
                if (thisAction != NONE) {
                    // Ensure we don't do any action when pressing multiple buttons
                    if (action == NONE) {
                        action = thisAction;
                    } else {
                        action = INVALID;
                    }
                    lastAction = now;
                }
            }
        }

        lastPressed = anyPressed;

        if (buttons_active) {
            gpio_pin_set_dt(&actionLed, anyPressed);
        }

        media_key = 0;
        key = 0;
        keyCount = 1;
        switch (action) {
            case VOLUME_DOWN:
                media_key = MEDIA_KEY_VOLUME_DOWN;
                break;
            case VOLUME_UP:
                media_key = MEDIA_KEY_VOLUME_UP;
                break;
            case PAUSE:
                media_key = MEDIA_KEY_PLAY_PAUSE;
                break;
            case PAGE_DOWN:
                // TODO: key = KEYCODE_PAGE_DOWN;
                break;
            case PAGE_UP:
                // TODO: key = KEYCODE_PAGE_UP;
                break;
            case DOUBLE_PAGE_UP:
                // TODO
                //key = KEYCODE_PAGE_UP;
                //keyCount = 2;
                break;
            case PREVIOUS:
                media_key = MEDIA_KEY_SCAN_PREV_TRACK;
                break;
            case NEXT:
                media_key = MEDIA_KEY_SCAN_NEXT_TRACK;
                break;
            case RESET:
                // TODO?
                bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);
                NVIC_SystemReset();
                break;
        }
        if (buttons_active && (media_key || key)) {
            while (keyCount > 0) {
                if (media_key) {
                    hid_key_report.media_key = media_key;
                }
                if (key) {
                    // TODO re-implement for pageup/pagedown keys
                }
                // Press
                bt_gatt_notify(NULL, &hog_svc.attrs[5],
                           &hid_key_report, sizeof(hid_key_report));
                // Release
                memset(&hid_key_report, 0, sizeof(hid_key_report));
                bt_gatt_notify(NULL, &hog_svc.attrs[5],
                           &hid_key_report, sizeof(hid_key_report));
                keyCount--;
            }
        }

		k_sleep(K_MSEC(50));
	}
}
