/*
 * usb.h
 * Header file for GoodUSB USB spec related
 * Feb 11, 2015
 * root@davejingtian.org
 * http://davejingtian.org
 */
#include "nlm.h"

/* Common USB devices */
#define USB_DEV_STORAGE			0
#define USB_DEV_KEYBOARD		1
#define USB_DEV_MOUSE			2
#define USB_DEV_JOYSTICK		3
#define USB_DEV_WIRELESS		4
#define USB_DEV_CELLPHONE		5
#define USB_DEV_TABLET			6
#define USB_DEV_MICROPHONE		7
#define USB_DEV_SOUND			8
#define USB_DEV_HUB			9
#define USB_DEV_VIDEO			0xa
#define USB_DEV_HEADSET			0xb
#define USB_DEV_CHARGER			0xc
#define USB_DEV_COMM			0xd
#define USB_DEV_PRINTER			0xe
#define USB_DEV_SCANNER			0xf
#define USB_DEV_UNKNOWN			0xff

/* Ported from the famous ch9.h */
#define USB_CLASS_PER_INTERFACE         0       /* for DeviceClass */
#define USB_CLASS_AUDIO                 1
#define USB_CLASS_COMM                  2
#define USB_CLASS_HID                   3
#define USB_CLASS_PHYSICAL              5
#define USB_CLASS_STILL_IMAGE           6
#define USB_CLASS_PRINTER               7
#define USB_CLASS_MASS_STORAGE          8
#define USB_CLASS_HUB                   9
#define USB_CLASS_CDC_DATA              0x0a
#define USB_CLASS_CSCID                 0x0b    /* chip+ smart card */
#define USB_CLASS_CONTENT_SEC           0x0d    /* content security */
#define USB_CLASS_VIDEO                 0x0e
#define USB_CLASS_WIRELESS_CONTROLLER   0xe0
#define USB_CLASS_MISC                  0xef
#define USB_CLASS_APP_SPEC              0xfe
#define USB_CLASS_VENDOR_SPEC           0xff

#define USB_SUBCLASS_NONE		0
#define USB_SUBCLASS_VENDOR_SPEC        0xff

/* HID */
#define USB_INTERFACE_PROTOCOL_NONE	0
#define USB_INTERFACE_PROTOCOL_KEYBOARD	1
#define USB_INTERFACE_PROTOCOL_MOUSE	2

/*
 * NOTE: we may need to port different subClass and interface defs
 * distributed into different driver categories if we want to convert
 * the hex value into friendly text description. On the other hand,
 * I am fine with the hex value:)
 * Feb 11, 2015
 * daveti
 */

/* Core routines */
int usb_generate_u2k(char *exp, nlmsgt *req, unsigned char *ifmask, unsigned char *limhid);
int usb_get_dev_from_desc(char *desc);
const char *usb_get_class_desc(int class);
const char *usb_get_dev_desc(int index);

