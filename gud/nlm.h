/*
 * nlm.h
 * Header file for netlink messaging
 * Feb 10, 2015
 * Update for security picture and FP DB sync support
 * Apr 16, 2015
 * Added support for automatic untrusted dev rerouting to the HoneyUSB
 * May 8, 2015
 * Added support for description index for GUI user mode
 * May 12, 2015
 * root@davejingtian.org
 * http://davejingtian.org
 *
 */

#ifndef NLM_INCLUDE
#define NLM_INCLUDE

/* Defs - aligned with drivers/usb/core/goodusb.h */
#define GOODUSB_NETLINK				31
#define GOODUSB_NETLINK_OP_INIT			0
#define GOODUSB_NETLINK_OP_K2U			1
#define GOODUSB_NETLINK_OP_U2K			2
#define GOODUSB_NETLINK_OP_BYE			3
#define GOODUSB_NETLINK_OP_FP_K2U_SEC		4	/* Send the FP and security picture to the user space*/
#define GOODUSB_NETLINK_OP_FP_U2K_SEC		5	/* Send the Yes/No to the kernel space about this security picture */
#define GOODUSB_NETLINK_OP_FP_K2U_SYN		6	/* Synchronize the FP from the kernel to the user space */
#define GOODUSB_NETLINK_OP_FP_U2K_SYN		7	/* Synchronize the FP from the user to the kernel space */
#define GOODUSB_NETLINK_WAIT_TIME		10	/* ms */
#define GOODUSB_STRING_BUFF_LEN			64
#define GOODUSB_FINGERPRINT_LEN			20	/* SHA1 */
#define GOODUSB_INTERFACE_NUM_MAX		32	/* Should be the same as USB_MAXINTERFACES */

struct goodusb_interface {
	unsigned char if_class;
	unsigned char if_sub_class;
	unsigned char if_protocol;
	unsigned char if_ep_num;
};

struct goodusb_k2u {
	char	product[GOODUSB_STRING_BUFF_LEN];
	char	manufacturer[GOODUSB_STRING_BUFF_LEN];
	struct goodusb_interface	interface_array[GOODUSB_INTERFACE_NUM_MAX];
	short	idVendor;
	short	idProduct;
};

struct goodusb_fp {
	unsigned char fingerprint[GOODUSB_FINGERPRINT_LEN];
	unsigned char interface_mask[GOODUSB_INTERFACE_NUM_MAX];
	unsigned char limited_hid_driver;     /* Flag for calling capability-limited HID driver */
	unsigned char security_pic_index;     /* Security picture index combined with this device/fp */
	unsigned char description_index;      /* Only works for the user mode */
};

struct goodusb_k2u_sec {
	struct goodusb_k2u	k2u;
	unsigned char		interface_mask[GOODUSB_INTERFACE_NUM_MAX];
	unsigned char		limited_hid_driver;
	unsigned char		security_pic_index;
	unsigned char 		description_index;
};

struct goodusb_u2k {
	unsigned char interface_mask[GOODUSB_INTERFACE_NUM_MAX];
	unsigned char limited_hid;		/* Flag used to tell if limited HID driver should be used */
	unsigned char security_pic_index;	/* The index for the security picture used in future */
	unsigned char description_index;	/* The index for the dev description used by user mode */
	unsigned char disable;			/* Again, ugly...*/
};

struct goodusb_u2k_sec {
	struct goodusb_u2k	u2k;
	unsigned char		enable;	/* Should be used by normal user mode where Yes/No is expected */
};


struct goodusb_nlmsg {
	unsigned char opcode;			/* netlink OP code */
	unsigned char config_num;		/* configuation number */
	unsigned char interface_total_num;	/* interface total number */
	union {
		struct goodusb_k2u	k2u;		/* from kernel to the user */
		struct goodusb_u2k	u2k;		/* from the user to kernel */
		struct goodusb_k2u_sec	k2u_sec;	/* from the kernel to the user to display the security picture */
		struct goodusb_u2k_sec	u2k_sec;	/* from the user to the kernel to enable/disable this device */
		struct goodusb_fp	fp;		/* Fingerprint synchronization between the kernel and the user */
	};
	void	*dev;				/* udev pointer */
};


/* Definition for the netlink msgs */
typedef struct goodusb_nlmsg nlmsgt;

#define NLM_UCHAR_NUM_PER_LINE	20
#define NLM_MSG_LEN		sizeof(nlmsgt)
#define NLM_QUEUE_MSG_NUM	10
#define NLM_QUEUE_SIZE		(NLM_MSG_LEN*NLM_QUEUE_MSG_NUM)

/* NLM protocol related methods */

/* Display the nlmsgt msg */
void nlm_display_msg(nlmsgt *msg);

/* Display the uchar given length */
void nlm_display_uchar(unsigned char *src, int len, char *header);

/* NLM queue related methods */

/* Init the NLM queue */
void nlm_init_queue(void);

/* Add msgs into the NLM queue from raw binary data */
int nlm_add_msg_queue(nlmsgt *msg);

/* Clear all the msgs in the queue */
void nlm_clear_all_msg_queue(void);

/* Get the number of msgs in the queue */
int nlm_get_msg_num_queue(void);

/* Get the msg based on index */
nlmsgt * nlm_get_msg_queue(int index);

#endif
