/*
 * goodusb_hid.h
 * Header file for GoodUSB HID device driver (usbhid)
 * Mar 30, 2015
 * root@davejingtian.org
 * http://davejingtian.org
 */
#include <linux/list.h>

#define GOODUSB_CAP_LIMITED_URB_NUM_MAX		6	/* Imagine a 2-key volume control in a USB headset */
#define GOODUSB_CAP_LIMITED_URN_LEN_MAX		256	/* Based on my testing devices, it could be 32, 64 and 120 */

struct goodusb_cap_limited_input {
	u8	urb[GOODUSB_CAP_LIMITED_URN_LEN_MAX];
	u8	len;
	u8	valid;
};

struct goodusb_cap_limited_driver {
	void*	hid;		/* NOTE: we use hid device ptr directly - not accurate but working */
	struct goodusb_cap_limited_input input[GOODUSB_CAP_LIMITED_URB_NUM_MAX];
	struct list_head list;
};
