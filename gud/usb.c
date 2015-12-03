/*
 * usb.h
 * Header file for GoodUSB USB spec related
 * and GoodUSB core logic
 * Feb 11, 2015
 * root@davejingtian.org
 * http://davejingtian.org
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "usb.h"

/* Device info */
struct dev_info {
	int num;
	char *dev_name;
};

/* http://en.wikipedia.org/wiki/USB#Device_classes */
static const struct dev_info goodusb_dev[] = {
	/* Used for conversion between string and num */
	{USB_DEV_STORAGE,	"USB Storage (thumb drive, portable disk, SD reader)"},
	{USB_DEV_KEYBOARD,	"USB Keyboard"},
	{USB_DEV_MOUSE,		"USB Mouse"},
	{USB_DEV_JOYSTICK,	"USB Joystick"},
	{USB_DEV_WIRELESS,	"USB Wireless"},
	{USB_DEV_CELLPHONE,	"USB Cellphone (iPhone, Nexus, Galaxy)"},
	{USB_DEV_TABLET,	"USB Tablet (iPad, Nexus, Tab)"},
	{USB_DEV_MICROPHONE,	"USB Microphone"},
	{USB_DEV_SOUND,		"USB Sound (sound card, speaker, headphone)"},
	{USB_DEV_HUB,		"USB Hub (USB port extension)"},
	{USB_DEV_VIDEO,		"USB Video (WebCam)"},
	{USB_DEV_HEADSET,	"USB Headset"},
	{USB_DEV_CHARGER,	"USB Charger (E-cig, portable battery, toy)"},
	{USB_DEV_COMM,		"USB Communication (USB-USB networking, ATM/Ethernet)"},
	{USB_DEV_PRINTER,	"USB Printer"},
	{USB_DEV_SCANNER,	"USB Scanner"},
	{USB_DEV_UNKNOWN,	"USB UNKNOWN"}		/* The last */
};

#define USB_DEV_ARRAY_LEN	(sizeof(goodusb_dev)/sizeof(struct dev_info))

/* GoodUSB core logic */
#define USB_LEGAL_IF_ARRAY_LEN	8
struct core_info {
	int num;
	unsigned char ifs[USB_LEGAL_IF_ARRAY_LEN];
};

static const struct core_info goodusb_core[] = {
	/* Used for correlating the device and legal interfaces */
	/*
	 * Why we allow USB_CLASS_VENDOR_SPEC for all devices?
	 * 1. Like my Nexus 5, it is recognized as USB mass storage essentially.
	 * 	However, the interface is vender specific.
	 * 2. Being vendor spec means the vendor has to provide its own driver.
	 * 	To use the device, we have to load the NON generic driver anyway.
	 * 3. USB charger is always an exception, as there is no need to plug
	 * 	a USB microcontroller for just charging. Actually, this kind of
	 * 	device should be invisible to the kernel!
	 * Feb 12, 2015
	 * daveti
	 */
	{USB_DEV_STORAGE,	{USB_CLASS_MASS_STORAGE, USB_CLASS_CSCID, USB_CLASS_VENDOR_SPEC}},
	{USB_DEV_KEYBOARD,	{USB_CLASS_HID, USB_CLASS_VENDOR_SPEC}},
	{USB_DEV_MOUSE,		{USB_CLASS_HID, USB_CLASS_VENDOR_SPEC}},
	{USB_DEV_JOYSTICK,	{USB_CLASS_HID, USB_CLASS_PHYSICAL, USB_CLASS_VENDOR_SPEC}},
	{USB_DEV_WIRELESS,	{USB_CLASS_WIRELESS_CONTROLLER, USB_CLASS_VENDOR_SPEC}},
	{USB_DEV_CELLPHONE,	{USB_CLASS_MASS_STORAGE, USB_CLASS_VENDOR_SPEC}},
	{USB_DEV_TABLET,	{USB_CLASS_MASS_STORAGE, USB_CLASS_VENDOR_SPEC}},
	{USB_DEV_MICROPHONE,	{USB_CLASS_AUDIO, USB_CLASS_HID, USB_CLASS_VENDOR_SPEC}},
	{USB_DEV_SOUND,		{USB_CLASS_AUDIO, USB_CLASS_VENDOR_SPEC}},
	{USB_DEV_HUB,		{USB_CLASS_HUB, USB_CLASS_VENDOR_SPEC}},
	{USB_DEV_VIDEO,		{USB_CLASS_VIDEO, USB_CLASS_VENDOR_SPEC}},
	{USB_DEV_HEADSET,	{USB_CLASS_AUDIO, USB_CLASS_HID, USB_CLASS_VENDOR_SPEC}},
	{USB_DEV_CHARGER,	{0}},	/* Do not allow any USB interface */
	{USB_DEV_COMM,		{USB_CLASS_CDC_DATA, USB_CLASS_VENDOR_SPEC}},
	{USB_DEV_PRINTER,	{USB_CLASS_PRINTER, USB_CLASS_VENDOR_SPEC}},
	{USB_DEV_SCANNER,	{USB_CLASS_STILL_IMAGE, USB_CLASS_VENDOR_SPEC}},
	{USB_DEV_UNKNOWN,	{0}}	/* Always the last */
};

#define USB_CORE_ARRAY_LEN	(sizeof(goodusb_core)/sizeof(struct core_info))

/* ported from drivers/usb/devices.c */
struct class_info {
	int class;
	char *class_name;
};

static const struct class_info goodusb_class[] = {
	/* max. 5 chars. per name string */
	{USB_CLASS_PER_INTERFACE,       ">ifc"},
	{USB_CLASS_AUDIO,               "audio"},
	{USB_CLASS_COMM,                "comm."},
	{USB_CLASS_HID,                 "HID"},
	{USB_CLASS_PHYSICAL,            "PID"},
	{USB_CLASS_STILL_IMAGE,         "still"},
	{USB_CLASS_PRINTER,             "print"},
	{USB_CLASS_MASS_STORAGE,        "stor."},
	{USB_CLASS_HUB,                 "hub"},
	{USB_CLASS_CDC_DATA,            "data"},
	{USB_CLASS_CSCID,               "scard"},
	{USB_CLASS_CONTENT_SEC,         "c-sec"},
	{USB_CLASS_VIDEO,               "video"},
	{USB_CLASS_WIRELESS_CONTROLLER, "wlcon"},
	{USB_CLASS_MISC,                "misc"},
	{USB_CLASS_APP_SPEC,            "app."},
	{USB_CLASS_VENDOR_SPEC,         "vend."},
	{-1,                            "unk."}         /* leave as last */
};

#define USB_IFCLASS_ARRAY_LEN	(sizeof(goodusb_class)/sizeof(struct class_info))

/* Global var */
static int usb_debug = 1;

/*
 * Check if limited HID driver is needed
 * If yes, return 1
 * Otherwise 0
 */
static unsigned char usb_check_hid(int dev)
{
	if (usb_debug)
		printf("usb: into [%s], dev [%d]\n", __func__, dev);

	switch (dev) {
	case USB_DEV_JOYSTICK:
	case USB_DEV_MICROPHONE:
	case USB_DEV_HEADSET:
		return 1;
	default:
		break;
	}

	return 0;
}

/*
 * Generate the interface mask based on the GoodUSB core
 * Return 0 and ifmask is filled up if success.
 * Otherwise -1.
 */
static int usb_generate_interface_mask(unsigned char *ifs, unsigned char *ifmask, nlmsgt *req)
{
	int i, j;

	if (usb_debug)
		printf("usb: into [%s], ifs [%p], ifmask [%p], req [%p]\n",
			__func__, ifs, ifmask, req);

	/* Defensive checking */
	if (!ifs || !ifmask || !req) {
		printf("usb - Error: Null ifs/ifmask/req\n");
		return -1;
	}

	/* Go thru all interfaces requested */
	for (i = 0; i < req->interface_total_num; i++) {
		/* Go thru the interfaces allowed */
		for (j = 0; j < USB_LEGAL_IF_ARRAY_LEN; j++) {
			if ((req->k2u.interface_array)[i].if_class == ifs[j]) {
				ifmask[i] = 1;
				break;
			}
		}
	}

	return 0;
}

/*
 * Get the legal interfaces from the dev num
 */
static const unsigned char *usb_get_legal_ifs_from_dev(int dev)
{
	int i;

	if (usb_debug)
		printf("usb: into [%s], dev [%d]\n", __func__, dev);

	for (i = 0; i < USB_CORE_ARRAY_LEN; i++)
		if (dev == goodusb_core[i].num)
			return goodusb_core[i].ifs;

	/* Return the last unknown */
	return goodusb_core[USB_CORE_ARRAY_LEN-1].ifs;
}

/*
 * Get the device ID from the string description
 */
int usb_get_dev_from_desc(char *desc)
{
	int i;

	if (usb_debug)
		printf("usb: into [%s], desc [%s]\n", __func__, desc);

	for (i = 0; i < USB_DEV_ARRAY_LEN; i++)
		if (!strcasecmp(desc, goodusb_dev[i].dev_name))
			return goodusb_dev[i].num;

	return USB_DEV_UNKNOWN;
}

/*
 * Return the description string based on the device index
 * NOTE: this should only be used by K2U_SEC GUI
 */
const char *usb_get_dev_desc(int index)
{
	int i;
	
	if (usb_debug)
		printf("usb: into [%s], index [%d]\n", __func__, index);

	for (i = 0; i < USB_DEV_ARRAY_LEN; i++)
		if (index == goodusb_dev[i].num)
			return goodusb_dev[i].dev_name;

	return goodusb_dev[USB_DEV_ARRAY_LEN-1].dev_name;
}

/*
 * Return the description string based on the interface class
 * NOTE: this should only be used in Pro mode.
 */
const char *usb_get_class_desc(int class)
{
	int i;

	if (usb_debug)
		printf("usb: into [%s], class [%d]\n", __func__, class);

	for (i = 0; i < USB_IFCLASS_ARRAY_LEN; i++)
		if (class == goodusb_class[i].class)
			return goodusb_class[i].class_name;

	return "INVAL";
}

/*
 * Generate the u2k (interface mask + limited HID flag)
 * based on the user's expection (output from GUI).
 * NOTE: this should be only used in NON pro mode (stupid user mode)!
 * The ifmask and limhid will be filled up accordingly.
 * Return 0 if success, otherwise 0.
 */
int usb_generate_u2k(char *exp, nlmsgt *req, unsigned char *ifmask, unsigned char *limhid)
{
	int dev;
	int ret;
	unsigned char *ifs;

	if (usb_debug)
		printf("usb: into [%s], req [%p], ifmask [%p], limhid [%p]\n",
			__func__, req, ifmask, limhid);

	/* Defensive checking */
	if (!exp || !req || !ifmask || !limhid) {
		printf("usb - Error: Null exp/req/ifmask/limhid\n");
		return -1;
	}

	/* Get the device */
	dev = usb_get_dev_from_desc(exp);
	if (dev == -1) {
		printf("usb - Error: usb_get_dev_from_desc failed\n");
		return -1;
	}
	if (usb_debug)
		printf("usb: got dev ID [%d]\n", dev);

	/* Get the legal interface mask */
	ifs = usb_get_legal_ifs_from_dev(dev);
	if (usb_debug)
		nlm_display_uchar(ifs, USB_LEGAL_IF_ARRAY_LEN, "Allowed interfaces:");

	/* Generate the new interface mask */
	ret = usb_generate_interface_mask(ifs, ifmask, req);
	if (ret) {
		printf("usb - Error: usb_generate_interface_mask failed\n");
		return -1;
	}

	/* Set the limited HID flag */
	*limhid = usb_check_hid(dev);

	return 0;
}
