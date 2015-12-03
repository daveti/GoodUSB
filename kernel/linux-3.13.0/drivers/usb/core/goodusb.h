/*
 * GoodUSB
 * Header file for GoodUSB
 * Jan 27, 2015
 * Added support for security picture and fingerprint sync with gud
 * Apr 8, 2015
 * Added support for automatic untrusted dev rerouting to the HoneyUSB
 * May 8, 2015
 * Added description index to support user mode
 * May 12, 2015
 * root@davejingtian.org
 * http://davejingtian.org
 */

#include <linux/pm.h>
#include <linux/acpi.h>
#include <linux/list.h>
#include <linux/skbuff.h>
#include <net/sock.h>

/* Defs */
#define GOODUSB_NETLINK				31
#define GOODUSB_NETLINK_OP_INIT			0
#define GOODUSB_NETLINK_OP_K2U			1
#define GOODUSB_NETLINK_OP_U2K			2
#define GOODUSB_NETLINK_OP_BYE			3
#define GOODUSB_NETLINK_OP_FP_K2U_SEC		4	/* Send the FP and security picture to the user space*/
#define GOODUSB_NETLINK_OP_FP_U2K_SEC		5	/* Send the Yes/No to the kernel space about this security picture */
#define GOODUSB_NETLINK_OP_FP_K2U_SYN		6	/* Synchronize the FP from the kernel to the user space */
#define GOODUSB_NETLINK_OP_FP_U2K_SYN		7	/* Synchronize the FP from the user to the kernel space */
#define GOODUSB_NETLINK_WAIT_TIME		1	/* ms */
#define GOODUSB_NETLINK_WAIT_USEC_MIN		25	/* us */
#define GOODUSB_NETLINK_WAIT_USEC_MAX		50	/* us */
#define GOODUSB_STRING_BUFF_LEN			64
#define GOODUSB_INTERFACE_NUM_MAX		32	/* Should be the same as USB_MAXINTERFACES */
#define GOODUSB_FINGERPRINT_LEN			20	/* SHA1 */
#define GOODUSB_FINGERPRINT_DB_SIZE		128	/* Max 128 entries in the DB */
#define GOODUSB_DEVICE_WIHTELIST_SIZE		32	/* Max 32 devices could be added into the WL */
#define GOODUSB_USB_MAXCONFIG			8	/* From drivers/usb/core/config.c */

/* Perf */
#define GOODUSB_MBM_SEC_IN_USEC         1000000         /* GoodUSB micro benchmark */
#define GOODUSB_MBM_SUB_TV(s, e)                \
        ((e.tv_sec*GOODUSB_MBM_SEC_IN_USEC+e.tv_usec) - \
        (s.tv_sec*GOODUSB_MBM_SEC_IN_USEC+s.tv_usec))

struct goodusb_interface {
	u8	if_class;
	u8	if_sub_class;
	u8	if_protocol;
	u8	if_ep_num;
};

struct goodusb_k2u {
	char	product[GOODUSB_STRING_BUFF_LEN];
	char	manufacturer[GOODUSB_STRING_BUFF_LEN];
	struct goodusb_interface	interface_array[GOODUSB_INTERFACE_NUM_MAX];
	u16	idVendor;
	u16	idProduct;
};

struct goodusb_fp {
	u8      fingerprint[GOODUSB_FINGERPRINT_LEN];
	u8      interface_mask[GOODUSB_INTERFACE_NUM_MAX];
	u8      limited_hid_driver;     /* Flag for calling capability-limited HID driver */
	u8      security_pic_index;     /* Security picture index combined with this device/fp */
	u8	description_index;	/* Description index used by the gud for user mode */
};

struct goodusb_k2u_sec {
	struct goodusb_k2u	k2u;
	u8			interface_mask[GOODUSB_INTERFACE_NUM_MAX];
	u8			limited_hid_driver;
	u8			security_pic_index;
	u8			description_index;
};

struct goodusb_u2k {
	u8	interface_mask[GOODUSB_INTERFACE_NUM_MAX];
	u8	limited_hid;		/* flag to tell if limited HID driver should be used */
	u8	security_pic_index;	/* The index for the security picture used in future */
	u8 	description_index;	/* Description index used by the gud for the user mode */
	u8	disable;		/* This is ugly - we should remove the enable flag in the u2k_sec...*/
};

struct goodusb_u2k_sec {
	struct goodusb_u2k	u2k;
	u8			enable;	/* Should be used by normal user mode where Yes/No is expected */
};
	
struct goodusb_nlmsg {
	u8	opcode;			/* netlink OP code */
	u8	config_num;		/* configuation number */
	u8	interface_total_num;	/* interface total number */
	union {
		struct goodusb_k2u	k2u;		/* from the kernel to the user to display the device for the first time */
		struct goodusb_u2k	u2k;		/* from the user to the kernel to configure the device for the first time */
		struct goodusb_k2u_sec	k2u_sec;	/* from the kernel to the user to display the security picture */
		struct goodusb_u2k_sec	u2k_sec;	/* from the user to the kernel to enable/disable this device */
		struct goodusb_fp	fp;		/* Fingerprint synchronization between the kernel and the user */
	};
	void	*dev;			/* udev pointer */
};

struct goodusb_fingerprint {
	struct goodusb_fp	fp;
	struct list_head	list;
};

struct goodusb_dev_whitelist_entry {
	u16	idVendor;
	u16	idProduct;
};

struct goodusb_dev_whitelist {
	struct goodusb_dev_whitelist_entry dev[GOODUSB_DEVICE_WIHTELIST_SIZE];
	int count;
};

/* Init/Exit */
int goodusb_init(void);
void goodusb_exit(void);
int goodusb_start(void);

/* Debug routines */
void goodusb_dump_fp(struct goodusb_fp *fp);
void goodusb_dump_nlmsg(struct goodusb_nlmsg *msg);
void goodusb_dump_usb_interface(struct usb_interface *uif);
void goodusb_dump_usb_descriptors(struct usb_device *udev);

/* Netlink routines */
void goodusb_nl_handler(struct sk_buff *skb);
int goodusb_nl_handle_init(struct nlmsghdr *nlh);
int goodusb_nl_handle_u2k(struct goodusb_nlmsg *msg);
int goodusb_nl_handle_fp_u2k_sec(struct goodusb_nlmsg *msg);
int goodusb_nl_handle_fp_u2k_syn(struct goodusb_nlmsg *msg);
int goodusb_nl_send_k2u(struct usb_device *udev,
			struct usb_host_config *config,
			int config_num, int interface_num);
int goodusb_nl_send_fp_k2u_sec(struct usb_device *udev,
			struct usb_host_config *config,
			struct goodusb_fp *fp,
			int config_num, int interface_num);
int goodusb_nl_send_fp_k2u_syn(struct goodusb_fp *fp);
int goodusb_nl_send_bye(void);

/* GoodUSB device DB routines */
struct goodusb_fingerprint *goodusb_db_find_fp(struct usb_device *udev, u8 *fp);
struct goodusb_fp *goodusb_db_add_fp(struct usb_device *udev, u8 *fp);
void goodusb_db_destory(void);
void goodusb_db_sync(void);

/* GoodUSB device WL (whitelist) routines */
int goodusb_wl_filter_dev(struct usb_device *udev);
int goodusb_wl_add_dev(struct usb_device *udev);

/* GoodUSB fingerprint routines */
int goddusb_fp_get_fp(struct usb_device *udev,
		      struct usb_host_config *config,
		      int config_num,
		      int interface_num,
		      u8 *fp);
