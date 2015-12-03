/*
 * GoodUSB
 * A countermeasure against BadUSB devices
 * and an enhancement to the kernel USB hub
 * NOTE: for details, please refer to the paper we have not written yet. THX~
 * Originated by daveti
 * Jan 26, 2015
 * Added support for security picture and fingerprint db sync with the user space
 * Apr 9, 2015
 * Added performance evaluation
 * May 26, 2015
 * Support for khub waken up from netlink
 * Jun 7, 2015
 * root@davejingtian
 * http://davejingtian.org
 */

#include <linux/usb.h>
#include <linux/usb/ch9.h>
#include <linux/usb/hcd.h>
#include <linux/usb/quirks.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/netlink.h>
#include <linux/err.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <asm/byteorder.h>
#include "usb.h"
#include "goodusb.h"

#define GOODUSB_DEV_WL_KERNEL_SIZE	8		/* Max 8 hardcoded dev within the kernel */

/* Global vars */
static struct sock *goodusb_nl_sock;			/* netlink socket */
static struct list_head goodusb_fp_db;			/* fingerprint DB */
static struct goodusb_dev_whitelist goodusb_dev_wl;     /* device whitelist */
static int goodusb_daemon_pid;				/* gud pid */
static int goodusb_debug;				/* debug */
static int goodusb_inited;				/* marked by the goodusb_init */
static int goodusb_dev_wl_k_size = 4;			/* Should be updated with goodusb_dev_wl_k */
static int goodusb_perf_fp = 0;				/* Performance measurement for fingerprint */
static int goodusb_khub_wake_up = 1;
/* Devices Dave should trust any way */
static struct goodusb_dev_whitelist_entry goodusb_dev_wl_k[GOODUSB_DEV_WL_KERNEL_SIZE] = {
	{0x1d6b, 0x0002},		/* EHCI Host Controller */
	{0x1d6b, 0x0001},		/* UHCI Host Controller */
	{0x0461, 0x4e22},		/* Dave's PixArt Optical mouse */
	{0x413c, 0x2107},		/* Dave's Dell Entry keyboard */
	{0x0000, 0x0000},		/* 0x0 */
	{0x0000, 0x0000},		/* 0x0 */
	{0x0000, 0x0000},		/* 0x0 */
	{0x0000, 0x0000}		/* 0x0 */
};


/* Debug routines */
static void goodusb_dump_usb_interface_descriptor(struct usb_interface_descriptor *desc)
{
	pr_info("goodusb: into [%s], desc [%p]\n", __func__, desc);

	if (!desc) {
		pr_err("Error: null interface descriptor\n");
		return;
	}

	pr_info("Interface descriptor:\n"
		"\tbLength\t\t%u\n"
		"\tbDescriptorType\t\t%u\n"
		"\tbInterfaceNumber\t\t%u\n"
		"\tbAlternateSetting\t\t%u\n"
		"\tbNumEndpoints\t\t%u\n"
		"\tbInterfaceClass\t\t%u\n"
		"\tbInterfaceSubClass\t\t%u\n"
		"\tbInterfaceProtocol\t\t%u\n"
		"\tiInterface\t\t%u\n",
		desc->bLength,
		desc->bDescriptorType,
		desc->bInterfaceNumber,
		desc->bAlternateSetting,
		desc->bNumEndpoints,
		desc->bInterfaceClass,
		desc->bInterfaceSubClass,
		desc->bInterfaceProtocol,
		desc->iInterface);
}

static void goodusb_dump_usb_config_descriptor(struct usb_config_descriptor *desc)
{
	pr_info("goodusb: into [%s], desc [%p]\n", __func__, desc);

	if (!desc) {
		pr_err("Error: null config descriptor\n");
		return;
	}

	pr_info("Configuration descriptor:\n"
		"\tbLength\t\t%u\n"
		"\tbDescriptorType\t\t%u\n"
		"\twTotalLength\t\t%u\n"
		"\tbNumInterfaces\t\t%u\n"
		"\tbConfigurationValue\t\t%u\n"
		"\tiConfiguration\t\t%u\n"
		"\tbmAttributes\t\t0x%x\n"
		"\tbMaxPower\t\t%umA\n",
		desc->bLength,
		desc->bDescriptorType,
		le16_to_cpu(desc->wTotalLength),
		desc->bNumInterfaces,
		desc->bConfigurationValue,
		desc->iConfiguration,
		desc->bmAttributes,
		desc->bMaxPower);
}

static void goodusb_dump_usb_device_descriptor(struct usb_device_descriptor *desc)
{
	pr_info("goodusb: into [%s], desc [%p]\n", __func__, desc);

	if (!desc) {
		pr_err("Error: null device descriptor\n");
		return;
	}

	pr_info("Device descriptor\n"
		"\tbLength\t\t%u\n"
		"\tbDescriptorType\t\t%u\n"
		"\tbcdUSB\t\t%u\n"
		"\tbDeviceClass\t\t%u\n"
		"\tbDeviceSubClass\t\t%u\n"
		"\tbDeviceProtocol\t\t%u\n"
		"\tbMaxPacketSize0\t\t%u\n"
		"\tidVendor\t\t0x%x\n"
		"\tidProduct\t\t0x%x\n"
		"\tbcdDevice\t\t%u\n"
		"\tiManufacturer\t\t%u\n"
		"\tiProduct\t\t%u\n"
		"\tiSerialNumber\t\t%u\n"
		"\tbNumConfigurations\t\t%u\n",
		desc->bLength,
		desc->bDescriptorType,
		le16_to_cpu(desc->bcdUSB),
		desc->bDeviceClass,
		desc->bDeviceSubClass,
		desc->bDeviceProtocol,
		desc->bMaxPacketSize0,
		le16_to_cpu(desc->idVendor),
		le16_to_cpu(desc->idProduct),
		le16_to_cpu(desc->bcdDevice),
		desc->iManufacturer,
		desc->iProduct,
		desc->iSerialNumber,
		desc->bNumConfigurations);
}

void goodusb_dump_usb_interface(struct usb_interface *uif)
{
	pr_info("goodusb: into [%s], uif [%p]\n", __func__, uif);

	if (!uif) {
		pr_err("Error: null usb interface\n");
		return;
	}

	/* Check the alternate setting at first - current setting may not be ready */
	if (uif->altsetting)
		goodusb_dump_usb_interface_descriptor(&(uif->altsetting->desc));
	else
		pr_err("Error: no alternate setting for this interface\n");

	/* Check the current settings then */
	if (uif->cur_altsetting)
		goodusb_dump_usb_interface_descriptor(&(uif->cur_altsetting->desc));
	else
		pr_info("Warn: no current alternate setting for this interface\n");
}

void goodusb_dump_usb_descriptors(struct usb_device *udev)
{
	pr_info("goodusb: into [%s], udev [%p]\n", __func__, udev);

	if (!udev) {
		pr_err("Error: null udev ptr\n");
		return;
	}

	/* Dump the device descriptor */
	goodusb_dump_usb_device_descriptor(&(udev->descriptor));

	/* Check the config at first - active config may not be ready */
	if (udev->config)
		goodusb_dump_usb_config_descriptor(&(udev->config->desc));
	else
		pr_err("Error: no config for this device\n");

	/* Check the active config then */
	if (udev->actconfig)
		goodusb_dump_usb_config_descriptor(&(udev->actconfig->desc));
	else
		pr_info("Warn: no active config for this device yet\n");

	/* Dump the interface descriptors
	 * NOTE: We could NOT use usb_ifnum_to_if() as it is looking
	 * for the active settings...
	 */
	int i, j;
	int has_intf;
	struct usb_host_config *config;
	struct usb_interface *uif;
	
	for (j = 0; j < 2; j++) {
		has_intf = 0;
		if ((j == 0) && (udev->config)) {
			config = udev->config;
			has_intf = 1;
			pr_info("goodusb: got [%d] interfaces in total for this config\n", config->desc.bNumInterfaces);
		} else if ((j == 1) && (udev->actconfig)) {
			config = udev->actconfig;
			has_intf = 1;
			pr_info("goodusb: got [%d] interfaces in total for this active config\n", config->desc.bNumInterfaces);
		}

		if (!has_intf)
			continue;
	
		/* NOTE: the interfaces are not saved in a reasonable order.
		 * Thus we should not rely on finding interface #1 at index 1.
		 * Instead, go thru the whole array!
		 */	
		for (i = 0; i < USB_MAXINTERFACES; i++) {
			//uif = usb_ifnum_to_if(udev, i);
			uif = config->interface[i];
			if (uif)
				pr_info("goodusb: got an interface with number [%d]\n", i);
			else
				continue;
			/* Dump the usb interface */
			goodusb_dump_usb_interface(uif);
		}
	}
}

void goodusb_dump_fp(struct goodusb_fp *fp)
{
	pr_info("goodusb: into [%s], fp [%p]\n", __func__, fp);

	if (!fp) {
		pr_err("goodusb: Error - Null fp ptr\n");
		return;
	}

	/* Dump the fp */
	print_hex_dump(KERN_INFO, "fp: ", DUMP_PREFIX_NONE, 16, 1,
		fp->fingerprint, GOODUSB_FINGERPRINT_LEN, 0);

	/* Dump the interface_mask */
	print_hex_dump(KERN_INFO, "if: ", DUMP_PREFIX_NONE, 16, 1,
		fp->interface_mask, GOODUSB_INTERFACE_NUM_MAX, 0);

	/* Dump the left */
	pr_info("limited_hid_driver = [%u]\n"
		"security_pic_index = [%u]\n"
		"description_index = [%u]\n",
		fp->limited_hid_driver,
		fp->security_pic_index,
		fp->description_index);
	
}

void goodusb_dump_nlmsg(struct goodusb_nlmsg *msg)
{
	int i;

	pr_info("goodusb: into [%s], msg [%p]\n", __func__, msg);

	if (!msg) {
		pr_err("goodusb: Error - Null msg ptr\n");
		return;
	}

	pr_info("opcode = [%u]\n"
		"config_num = [%u]\n"
		"interface_total_num = [%u]\n"
		"dev = [%p]\n",
		msg->opcode,
		msg->config_num,
		msg->interface_total_num,
		msg->dev);

	switch (msg->opcode) {

	case GOODUSB_NETLINK_OP_INIT:
		pr_info("init msg = [%s]\n", msg->k2u.product);
		break;

	case GOODUSB_NETLINK_OP_K2U:
		pr_info("k2u msg:\n"
			"product = [%s]\n"
			"manufacturer = [%s]\n"
			"idVendor = [%04x]\n"
			"idProduct = [%04x]\n",	
			msg->k2u.product,
			msg->k2u.manufacturer,
			msg->k2u.idVendor,
			msg->k2u.idProduct);

		for (i = 0; i < msg->interface_total_num; i++)
			pr_info("interface desc:\n"
				"class = [0x%x]\n"
				"subClass = [0x%x]\n"
				"protocol = [0x%x]\n"
				"ep_num = [%u]\n",
				(msg->k2u.interface_array)[i].if_class,
				(msg->k2u.interface_array)[i].if_sub_class,
				(msg->k2u.interface_array)[i].if_protocol,
				(msg->k2u.interface_array)[i].if_ep_num);
		break;

	case GOODUSB_NETLINK_OP_U2K:
		print_hex_dump(KERN_INFO, "u2k: ", DUMP_PREFIX_NONE, 16, 1,
			msg->u2k.interface_mask, 
			GOODUSB_INTERFACE_NUM_MAX, 0);
		pr_info("limited_hid = [%u]\n"
			"security_pic_index = [%u]\n"
			"description_index = [%u]\n"
			"disable=[%u]\n",
			msg->u2k.limited_hid,
			msg->u2k.security_pic_index,
			msg->u2k.description_index,
			msg->u2k.disable);
		break;

	case GOODUSB_NETLINK_OP_FP_U2K_SEC:
		print_hex_dump(KERN_INFO, "u2k_sec: ", DUMP_PREFIX_NONE, 16, 1,
			msg->u2k_sec.u2k.interface_mask,
			GOODUSB_INTERFACE_NUM_MAX, 0);
		pr_info("limited_hid = [%u]\n"
			"security_pic_index = [%u]\n"
			"description_index = [%u]\n"
			"disable = [%u]\n"
			"enable = [%u]\n",
			msg->u2k_sec.u2k.limited_hid,
			msg->u2k_sec.u2k.security_pic_index,
			msg->u2k_sec.u2k.description_index,
			msg->u2k_sec.u2k.disable,
			msg->u2k_sec.enable);
		break;

	case GOODUSB_NETLINK_OP_BYE:
		pr_info("bye msg\n");
		break;

	case GOODUSB_NETLINK_OP_FP_K2U_SEC:
		pr_info("k2u_sec msg:\n"
			"product = [%s]\n"
			"manufacturer = [%s]\n"
			"idVendor = [%04x]\n"
			"idProduct = [%04x]\n"
			"limited_hid_driver = [%u]\n"
			"security_pic_index = [%u]\n"
			"description_index = [%u]\n",
			msg->k2u_sec.k2u.product,
			msg->k2u_sec.k2u.manufacturer,
			msg->k2u_sec.k2u.idVendor,
			msg->k2u_sec.k2u.idProduct,
			msg->k2u_sec.limited_hid_driver,
			msg->k2u_sec.security_pic_index,
			msg->k2u_sec.description_index);

		for (i = 0; i < msg->interface_total_num; i++)
			pr_info("interface desc:\n"
				"class = [0x%x]\n"
				"subClass = [0x%x]\n"
				"protocol = [0x%x]\n"
				"ep_num = [%u]\n"
				"enabled = [%u]\n",
				(msg->k2u_sec.k2u.interface_array)[i].if_class,
				(msg->k2u_sec.k2u.interface_array)[i].if_sub_class,
				(msg->k2u_sec.k2u.interface_array)[i].if_protocol,
				(msg->k2u_sec.k2u.interface_array)[i].if_ep_num,
				(msg->k2u_sec.interface_mask)[i]);
		break;

	case GOODUSB_NETLINK_OP_FP_K2U_SYN:
	case GOODUSB_NETLINK_OP_FP_U2K_SYN:
		goodusb_dump_fp(&msg->fp);
		break;

	default:
		pr_err("goodusb: unknown opcode [%u]\n", msg->opcode);
		break;
	}
}


/* Netlink routines */
static int goodusb_nl_send(int opcode, u8 *data, int len)
{
        struct nlmsghdr *nlh;
        struct sk_buff *skb_out;
        struct goodusb_nlmsg msg_req;
        int msg_size;
        int data_len;
        int rtn;

        pr_info("goodusb: into [%s], opcode [%d], data [%p], len [%d]\n",
		__func__, opcode, data, len);

        /* Defensive checking */
        if (!goodusb_daemon_pid) {
                pr_err("goodusb: gud pid is unknown yet - abort\n");
                return -1;
        }

        /* Construct the request */
        msg_size = sizeof(msg_req);
        memset(&msg_req, 0, msg_size);
        msg_req.opcode = opcode;

	/* Follow the opcode */
	switch (opcode) {

	case GOODUSB_NETLINK_OP_INIT:
		/* Defensive checking */
		if (len > GOODUSB_STRING_BUFF_LEN) {
			pr_warn("goodusb: data len [%d] exceeds the max init len [%d] - truncate it\n",
				len, GOODUSB_STRING_BUFF_LEN);
			data_len = GOODUSB_STRING_BUFF_LEN;
		} else {
			data_len = len;
		}
		/* Copy the data into the nlmsg - reuse k2u */
		memcpy(msg_req.k2u.product, data, data_len);
		break;

	case GOODUSB_NETLINK_OP_K2U:
	case GOODUSB_NETLINK_OP_FP_K2U_SEC:
		/* Defensive checking */
		if (len != msg_size) {
			pr_err("goodusb: data len [%d] does not equal to nlmsg size [%d] - abort it\n",
				len, msg_size);
			return -1;
		} else {
			data_len = msg_size;
		}
		/* Copy the data into the nlmsg - here we have constructed the complete mesg from the caller */
		memcpy(&msg_req, data, data_len);
		break;

	case GOODUSB_NETLINK_OP_BYE:
		/* Defensive checking */
		if (len != 0)
			pr_warn("goodusb: data len [%d] is not zero for bye - drop it\n", len);
		/* No need to memcpy */
		data_len = 0;
		break;

	case GOODUSB_NETLINK_OP_FP_K2U_SYN:
		/* Defensive checking */
		if (len != sizeof(struct goodusb_fp)) {
			pr_err("goodusb: data len [%d] does not equal to fp size [%u] - abort it\n",
				len, sizeof(struct goodusb_fp));
			return -1;
		} else {
			data_len = sizeof(struct goodusb_fp);
		}
		/* Copy the data into the nlmsg */
		memcpy(&(msg_req.fp), data, data_len);
		break;

	default:
		pr_err("goodusb: invalid opcode [%d]\n", opcode);
		return -1;
	}

        /* Send the msg from kernel to the user */
        skb_out = nlmsg_new(msg_size, 0);
        if (!skb_out) {
                pr_err("goodusb: failed to allocate new skb\n");
                return -1;
        }

        nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
        NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */
        memcpy(nlmsg_data(nlh), &msg_req, msg_size);

        rtn = nlmsg_unicast(goodusb_nl_sock, skb_out, goodusb_daemon_pid);
        if (rtn) {
                pr_err("goodusb: failed to send request to gud, rtn [%d]\n", rtn);
                return -1;
        }

        pr_info("goodusb: sent an nlmsg request to the gud\n");

        return 0;
}

int goodusb_nl_handle_init(struct nlmsghdr *nlh)
{
        int rtn = 0;
        char *init_msg = "_goodusb_hello_from_kernel_";

        pr_info("goodusb: into [%s]\n", __func__); 


	if (nlh != NULL) {
		goodusb_daemon_pid = nlh->nlmsg_pid; /*pid of sending process */
		pr_info("goodusb: gud pid [%d]\n", goodusb_daemon_pid);

		/* Send hello to the gud */
		rtn = goodusb_nl_send(GOODUSB_NETLINK_OP_INIT, init_msg, strlen(init_msg)+1);
		if (rtn != 0)
			pr_err("goodusb: goodusb_nl_send() failed\n");
		else
			pr_info("goodusb: sent init msg to gud\n");
	}

        return rtn;
}

int goodusb_nl_send_bye(void)
{
	pr_info("goodusb: into [%s]\n", __func__);

	return goodusb_nl_send(GOODUSB_NETLINK_OP_BYE, NULL, 0);
}

int goodusb_nl_send_fp_k2u_syn(struct goodusb_fp *fp)
{
	pr_info("goodusb: into [%s]\n", __func__);

	return goodusb_nl_send(GOODUSB_NETLINK_OP_FP_K2U_SYN, fp, sizeof(*fp));
}

int goodusb_nl_handle_fp_u2k_syn(struct goodusb_nlmsg *msg)
{
	struct goodusb_fingerprint *fp;

	pr_info("goodusb: into [%s]\n", __func__);

	fp = kmalloc(sizeof(*fp), GFP_KERNEL);
	if (!fp) {
		pr_err("goodusb: kmalloc failed\n");
		return -1;
	}

	memcpy(&fp->fp, &msg->fp, sizeof(struct goodusb_fp));

	/*
	 * Note: this should happen during the gud start-up time
	 * and we do not have duplicate checking here as we assume
	 * all the fp sync'd from the kernel to the user is unique
	 */

	/* Add this fp into the DB */
	list_add_tail(&fp->list, &goodusb_fp_db);

	return 0;
}

int goodusb_nl_handle_u2k(struct goodusb_nlmsg *msg)
{
	struct usb_device *udev;

	pr_info("goodusb: into [%s]\n", __func__);

	/* Defensive checking
	 * (we could do better by checking if the ptr
	 * belongs to the kernel space - daveti)
	 */
	if (!msg->dev) {
		pr_err("goodusb: invalid dev ptr [NULL]\n");
		return -1;
	}

	/* Cast the device ptr */
	udev = msg->dev;
	pr_info("goodusb: using device ptr [%p]\n", udev);

	/* Lock
	 * NOTE: we do not need to lock the device as we have
	 * our own mutex for the goodusb.
	 */
	mutex_lock(&udev->goodusb_mutex);

	/* Update the device */
	memcpy(udev->goodusb_interface_mask,
		msg->u2k.interface_mask,
		GOODUSB_INTERFACE_NUM_MAX);
	udev->goodusb_limited_hid = msg->u2k.limited_hid;
	udev->goodusb_security_pic_index = msg->u2k.security_pic_index;
	udev->goodusb_description_index = msg->u2k.description_index;
	udev->goodusb_dev_enable = !msg->u2k.disable;
	udev->goodusb_ready_to_go = 1;
	if (goodusb_debug)
		pr_info("goodusb: udev ready_to_go!\n");

	/* Unlock */
	mutex_unlock(&udev->goodusb_mutex);

	/* Check for wake-up */
	if (goodusb_khub_wake_up)
		wake_up(&udev->goodusb_wait);

	return 0;
}

int goodusb_nl_handle_fp_u2k_sec(struct goodusb_nlmsg *msg)
{
        struct usb_device *udev;

        pr_info("goodusb: into [%s]\n", __func__);

        /* Defensive checking
         * (we could do better by checking if the ptr
         * belongs to the kernel space - daveti)
         */
        if (!msg->dev) {
                pr_err("goodusb: invalid dev ptr [NULL]\n");
                return -1;
        }

        /* Cast the device ptr */
        udev = msg->dev;
        pr_info("goodusb: using device ptr [%p]\n", udev);

        /* Lock
         * NOTE: we do not need to lock the device as we have
         * our own mutex for the goodusb.
         */
        mutex_lock(&udev->goodusb_mutex);

        /* Update the device */
	if (msg->u2k_sec.enable) {
		memcpy(udev->goodusb_interface_mask,
                	msg->u2k_sec.u2k.interface_mask,
                	GOODUSB_INTERFACE_NUM_MAX);
		udev->goodusb_limited_hid = msg->u2k_sec.u2k.limited_hid;
		udev->goodusb_security_pic_index = msg->u2k_sec.u2k.security_pic_index;
		udev->goodusb_description_index = msg->u2k_sec.u2k.description_index;
	}
	/* Ignore the disable flag within u2k */
	udev->goodusb_dev_enable = msg->u2k_sec.enable;
        udev->goodusb_ready_to_go = 1;
        if (goodusb_debug)
                pr_info("goodusb: udev ready_to_go!\n");

        /* Unlock */
        mutex_unlock(&udev->goodusb_mutex);

	/* Check for wake-up */
	if (goodusb_khub_wake_up)
		wake_up(&udev->goodusb_wait);

        return 0;
}

/*
 * This function is used to construct a K2U_SEC nlmsg
 * for the gud (GoodUSB daemon), which will use this
 * information to let the user know that the device is
 * recognized as it claims.
 */
int goodusb_nl_send_fp_k2u_sec(struct usb_device *udev,
                        struct usb_host_config *config,
			struct goodusb_fp *fp,
                        int config_num, int interface_num)
{
        struct goodusb_nlmsg *msg;
        struct usb_interface *uif;
        struct usb_interface_descriptor *desc;
        int ret;
        int i;

        pr_info("goodusb: into [%s], udev [%p], config [%p], fp [%p], config_num [%d], interface_num [%d]\n",
                __func__, udev, config, fp, config_num, interface_num);

        if (!udev || !config || !fp) {
                pr_err("goodusb: Null udev/config/fp ptr\n");
                return -1; 
        }

        /* Alloc the mem for nlmsg */
        msg = kmalloc(sizeof(*msg), GFP_KERNEL);
        if (!msg) {
                pr_err("goodusb: failed to allocate memory for nlmsg\n");
                return -1; 
        }

        /* Construct the nlmsg */
        memset(msg, 0x0, sizeof(*msg));
        msg->opcode = GOODUSB_NETLINK_OP_FP_K2U_SEC;
        msg->config_num = config_num;
        msg->interface_total_num = interface_num;
        msg->dev = udev;
        /* Make sure both product and manufacturer are valid */
        if (udev->product)
                strncpy(msg->k2u_sec.k2u.product, udev->product, GOODUSB_STRING_BUFF_LEN);
        else
                strncpy(msg->k2u_sec.k2u.product, "UNKNOWN", GOODUSB_STRING_BUFF_LEN);
        if (udev->manufacturer)
                strncpy(msg->k2u_sec.k2u.manufacturer, udev->manufacturer, GOODUSB_STRING_BUFF_LEN);
        else
                strncpy(msg->k2u_sec.k2u.manufacturer, "UNKNOWN", GOODUSB_STRING_BUFF_LEN);
	/* Add the IDs */
	msg->k2u_sec.k2u.idVendor = le16_to_cpu(udev->descriptor.idVendor);
	msg->k2u_sec.k2u.idProduct = le16_to_cpu(udev->descriptor.idProduct);
	/* Make it safe for the user space*/
        (msg->k2u_sec.k2u.product)[GOODUSB_STRING_BUFF_LEN-1] = '\0';
        (msg->k2u_sec.k2u.manufacturer)[GOODUSB_STRING_BUFF_LEN-1] = '\0';

        /* Construct the interface array */
        for (i = 0; i < interface_num; i++) {
                uif = config->interface[i];
                /* Use current alternate settings */
                if (!uif->cur_altsetting) {
                        pr_err("goodusb: Null current alternate setting\n");
                        ret = -1;
                        goto K2U_SEC_OUT;
                }
                /* Save the interface descriptor */
                desc = &uif->cur_altsetting->desc;
                (msg->k2u_sec.k2u.interface_array)[i].if_class = desc->bInterfaceClass;
                (msg->k2u_sec.k2u.interface_array)[i].if_sub_class = desc->bInterfaceSubClass;
                (msg->k2u_sec.k2u.interface_array)[i].if_protocol = desc->bInterfaceProtocol;
                (msg->k2u_sec.k2u.interface_array)[i].if_ep_num = desc->bNumEndpoints;
        }

	/* Add the configuration from the FP */
	memcpy(msg->k2u_sec.interface_mask, fp->interface_mask, GOODUSB_INTERFACE_NUM_MAX);
	msg->k2u_sec.limited_hid_driver = fp->limited_hid_driver;
	msg->k2u_sec.security_pic_index = fp->security_pic_index;
	msg->k2u_sec.description_index = fp->description_index;

        /* Debug */
        if (goodusb_debug)
                goodusb_dump_nlmsg(msg);

        /* Send it */
        ret = goodusb_nl_send(GOODUSB_NETLINK_OP_FP_K2U_SEC, (u8 *)msg, sizeof(*msg));
        if (ret)
                pr_err("goodusb: goodusb_nl_send() failed with ret [%d]\n", ret);

K2U_SEC_OUT:
        /* Free the memory */
        kfree(msg);

        return ret;
}

/*
 * This function is used to construct a K2U nlmsg
 * for the gud (GoodUSB daemon), which will use this
 * information to let the user know what is going on
 * in the kernel after this device is plugged.
 */
int goodusb_nl_send_k2u(struct usb_device *udev,
			struct usb_host_config *config,
			int config_num, int interface_num)
{
	struct goodusb_nlmsg *msg;
	struct usb_interface *uif;
	struct usb_interface_descriptor *desc;
	int ret;
	int i;

	pr_info("goodusb: into [%s], udev [%p], config [%p], config_num [%d], interface_num [%d]\n",
		__func__, udev, config, config_num, interface_num);

	if (!udev || !config) {
		pr_err("goodusb: Null udev/config ptr\n");
		return -1;
	}

	/* Alloc the mem for nlmsg */
	msg = kmalloc(sizeof(*msg), GFP_KERNEL);
	if (!msg) {
		pr_err("goodusb: failed to allocate memory for nlmsg\n");
		return -1;
	}

	/* Construct the nlmsg */
	memset(msg, 0x0, sizeof(*msg));
	msg->opcode = GOODUSB_NETLINK_OP_K2U;
	msg->config_num = config_num;
	msg->interface_total_num = interface_num;
	msg->dev = udev;
	/* Make sure both product and manufacturer are valid */
	if (udev->product)
		strncpy(msg->k2u.product, udev->product, GOODUSB_STRING_BUFF_LEN);
	else
		strncpy(msg->k2u.product, "UNKNOWN", GOODUSB_STRING_BUFF_LEN);
	if (udev->manufacturer)
		strncpy(msg->k2u.manufacturer, udev->manufacturer, GOODUSB_STRING_BUFF_LEN);
	else
		strncpy(msg->k2u.manufacturer, "UNKNOWN", GOODUSB_STRING_BUFF_LEN);
	/* Add the IDs */
	msg->k2u.idVendor = le16_to_cpu(udev->descriptor.idVendor);
	msg->k2u.idProduct = le16_to_cpu(udev->descriptor.idProduct);
	/* Make it safe for the user space*/
	(msg->k2u.product)[GOODUSB_STRING_BUFF_LEN-1] = '\0';
	(msg->k2u.manufacturer)[GOODUSB_STRING_BUFF_LEN-1] = '\0';

	/* Construct the interface array */
	for (i = 0; i < interface_num; i++) {
		uif = config->interface[i];
		/* Use current alternate settings */
		if (!uif->cur_altsetting) {
			pr_err("goodusb: Null current alternate setting\n");
			ret = -1;
			goto K2U_OUT;
		}
		/* Save the interface descriptor */
		desc = &uif->cur_altsetting->desc;
		(msg->k2u.interface_array)[i].if_class = desc->bInterfaceClass;
		(msg->k2u.interface_array)[i].if_sub_class = desc->bInterfaceSubClass;
		(msg->k2u.interface_array)[i].if_protocol = desc->bInterfaceProtocol;
		(msg->k2u.interface_array)[i].if_ep_num = desc->bNumEndpoints;
	}

	/* Debug */
	if (goodusb_debug)
		goodusb_dump_nlmsg(msg);

	/* Send it */
	ret = goodusb_nl_send(GOODUSB_NETLINK_OP_K2U, (u8 *)msg, sizeof(*msg));
	if (ret)
		pr_err("goodusb: goodusb_nl_send() failed with ret [%d]\n", ret);

K2U_OUT:
	/* Free the memory */
	kfree(msg);

	return ret;
}

void goodusb_nl_handler(struct sk_buff *skb)
{
        struct nlmsghdr *nlh;
        struct goodusb_nlmsg *goodusb_nlmsg_ptr;
        int opcode;
        int rtn;

        pr_info("goodusb: entering [%s]\n", __func__);

        /* Retrive the opcode */
        nlh = (struct nlmsghdr *)skb->data;
        goodusb_nlmsg_ptr = (struct goodusb_nlmsg *)nlmsg_data(nlh);
        opcode = goodusb_nlmsg_ptr->opcode;
        pr_info("goodusb: netlink received msg opcode: %u\n", opcode);

        switch (opcode) {

	case GOODUSB_NETLINK_OP_INIT:
		rtn = goodusb_nl_handle_init(nlh);
		break;

	case GOODUSB_NETLINK_OP_U2K:
		rtn = goodusb_nl_handle_u2k(goodusb_nlmsg_ptr);
		break;

	case GOODUSB_NETLINK_OP_FP_U2K_SEC:
		rtn = goodusb_nl_handle_fp_u2k_sec(goodusb_nlmsg_ptr);
		break;

	case GOODUSB_NETLINK_OP_FP_U2K_SYN:
		rtn = goodusb_nl_handle_fp_u2k_syn(goodusb_nlmsg_ptr);
		break;

	default:
		rtn = 0;
		pr_err("goodusb: unknown netlink opcode: %u\n", opcode);
        }

        if (rtn < 0)
                pr_err("goodusb: netlink processing failure\n");
}

/* GoodUSB device DB routines */
struct goodusb_fingerprint *goodusb_db_find_fp(struct usb_device *udev, u8 *fp)
{
	struct goodusb_fingerprint *ptr;

	pr_info("goodusb: into [%s], udev [%p], fp [%p]\n",
		__func__, udev, fp);

	if (!udev || !fp) {
		pr_err("goodusb: Null udev/fp ptr\n");
		return NULL;
	}

	if (goodusb_debug) {
		/* Dump the fp */
		print_hex_dump(KERN_INFO, "fp: ", DUMP_PREFIX_NONE, 16, 1,
			fp, GOODUSB_FINGERPRINT_LEN, 0);
	}

	/* Go thur the DB */
	list_for_each_entry(ptr, &goodusb_fp_db, list) {
		if (memcmp(ptr->fp.fingerprint, fp, GOODUSB_FINGERPRINT_LEN) == 0)
			return ptr;
	}

	return NULL;
}

struct goodusb_fp *goodusb_db_add_fp(struct usb_device *udev, u8 *fp)
{
	struct goodusb_fingerprint *gfp;

	pr_info("goodusb: into [%s], udev [%p], fp [%p]\n",
		__func__, udev, fp);

	if (!udev || !fp) {
		pr_err("goodusb: Null udev/fp ptr\n");
		return NULL;
	}

	if (!udev->goodusb_ready_to_go) {
		pr_err("goodusb: udev is NOT ready-to-go\n");
		return NULL;
	}

	if (goodusb_debug) {
		/* Dump the fp */
		print_hex_dump(KERN_INFO, "fp: ", DUMP_PREFIX_NONE, 16, 1,
			fp, GOODUSB_FINGERPRINT_LEN, 0);

		/* Dump the interface mask */
		print_hex_dump(KERN_INFO, "if: ", DUMP_PREFIX_NONE, 16, 1,
			udev->goodusb_interface_mask, GOODUSB_INTERFACE_NUM_MAX, 0);

		/* Dump others */
		pr_info("limtied_hid_driver = [%u]\n"
			"security_pic_index = [%u]\n"
			"description_index = [%u]\n"
			"dev_enable = [%u]\n",
			udev->goodusb_limited_hid,
			udev->goodusb_security_pic_index,
			udev->goodusb_description_index,
			udev->goodusb_dev_enable);
	}

	/* Alloc the mem for this new fp */
	gfp = kmalloc(sizeof(*gfp), GFP_KERNEL);
	if (!gfp) {
		pr_err("goodusb: memory allocation for fingerprint failed\n");
		return NULL;
	}

	/* Init this new fp */
	memcpy(gfp->fp.fingerprint, fp, GOODUSB_FINGERPRINT_LEN);
	memcpy(gfp->fp.interface_mask,
		udev->goodusb_interface_mask,
		GOODUSB_INTERFACE_NUM_MAX);
	gfp->fp.limited_hid_driver = udev->goodusb_limited_hid;
	gfp->fp.security_pic_index = udev->goodusb_security_pic_index;
	gfp->fp.description_index = udev->goodusb_description_index;

	/* Add this fp into the DB */
	list_add_tail(&gfp->list, &goodusb_fp_db);

	return &gfp->fp;
}

void goodusb_db_sync(void)
{
	struct goodusb_fingerprint *ptr;
	struct goodusb_fp *fp;
	int ret;

	/* Go thru the list and sync with gud */
	list_for_each_entry(ptr, &goodusb_fp_db, list) {
		fp = &ptr->fp;
		ret = goodusb_nl_send_fp_k2u_syn(fp);
		if (ret)
			pr_err("goodusb: goodusb_nl_send_fp_k2u_syn() failed\n");
	}
}

void goodusb_db_destory(void)
{
	struct goodusb_fingerprint *ptr, *next;

	/* Go thru the list and free the memory */
	list_for_each_entry_safe(ptr, next, &goodusb_fp_db, list) {
		list_del(&ptr->list);
		kfree(ptr);
	}
}

/* GoodUSB device whitelist routines */
int goodusb_wl_filter_dev(struct usb_device *udev)
{
	int i;
	u16 idVendor;
	u16 idProduct;

	pr_info("goodusb: into [%s], udev [%p]\n", __func__, udev);

	if (!udev) {
		pr_err("goodusb: Null udev ptr\n");
		return -1;
	}

	/* Get the vendor and product */	
	idVendor = le16_to_cpu(udev->descriptor.idVendor);
	idProduct = le16_to_cpu(udev->descriptor.idProduct);

	if (goodusb_debug)
		pr_info("goodusb: idVendor [0x%x], idProduct [0x%x]\n",
			idVendor, idProduct);

	/* Search in the whitelist */
	for (i = 0; i < goodusb_dev_wl.count; i++)
		if (	((goodusb_dev_wl.dev)[i].idVendor == idVendor)
		     && ((goodusb_dev_wl.dev)[i].idProduct == idProduct))
			return 1;

	return 0;
}

int goodusb_wl_add_dev(struct usb_device *udev)
{
	/* TODO - netlink support for this */
	return -1;
}

/* GoodUSB fingerprint routines */
int goddusb_fp_get_fp(struct usb_device *udev,
		      struct usb_host_config *config,
		      int config_num,
		      int interface_num,
		      u8 *fp)
{
	struct hash_desc desc;
	struct scatterlist sg;
	struct crypto_hash *tfm;
	struct usb_interface *uif;
	struct usb_interface_descriptor *uid;
	struct timeval start_tv, end_tv;
	u8 *buf, *ptr;
	int len, total_len;
	int ret;
	int cfgn;
	int i;

if (goodusb_perf_fp)
	do_gettimeofday(&start_tv);

	/*
	 * GoodUSB: Fingerprint the device from the controller side
	 * FP := device descriptor + all config descriptors, which contains
	 * all the interfaces, endpoints and other descriptors as well.
	 * To implement this, we only need device descriptor and raw descriptors
	 * in the usb_device struct.
	 * For the fingerprint, we use SHA1 for now.
	 * Apr 10, 2015
	 * As the device may request different configurations with different interfaces,
	 * (e.g., the BadUSB 2.0 attacks we have imagined where the 2nd time the device is
	 * plugged, it is something totally different from what it acted the first time!)
	 * we need to add this information to the fingerprint as well.
	 * Apr 23, 2015
	 * daveti
	 */
	if (!udev || !config || !fp) {
		pr_err("goodusb: Null udev/config/fp ptr\n");
		return -1;
	}

	/* Check if the raw descriptors lens are ready */
	if (!udev->goodusb_rawdescriptors_lens) {
		pr_err("goodusb: No lens available for the raw descriptors\n");
		return -1;
	}

	/* Construct the big buffer */
	cfgn = udev->descriptor.bNumConfigurations;
	if (cfgn > GOODUSB_USB_MAXCONFIG)
		cfgn = GOODUSB_USB_MAXCONFIG;
	len = 0;
	/* Go thru all configs */
	for (i = 0; i < cfgn; i++)
		len += (udev->goodusb_rawdescriptors_lens)[i];
	/* Count in the device descriptor */
	len += sizeof(struct usb_device_descriptor);
	/* Consider the current config and interfaces requested */
	len += sizeof(config_num);	/* config num */
	len += sizeof(interface_num);	/* interface total num */
	len += interface_num * sizeof(struct usb_interface_descriptor);	/* interfaces requested */

	/* Alloc memory */
	total_len = len;
	buf = kmalloc(total_len, GFP_KERNEL);
	if (!buf) {
		pr_err("goodusb: kmalloc failed for big buffer with len [%d]\n", total_len);
		return -1;
	}

	/* Fill the buffer */
	memcpy(buf, &udev->descriptor, sizeof(struct usb_device_descriptor));
	ptr = buf + sizeof(struct usb_device_descriptor);
	for (i = 0; i < cfgn; i++) {
		len = (udev->goodusb_rawdescriptors_lens)[i];
		memcpy(ptr, udev->rawdescriptors[i], len);
		ptr += len;
	}
	memcpy(ptr, &config_num, sizeof(config_num));
	ptr += sizeof(config_num);
	memcpy(ptr, &interface_num, sizeof(interface_num));
	ptr += sizeof(interface_num);
	for (i = 0; i < interface_num; i++) {
		uif = config->interface[i];
		if (!uif) {
			pr_err("goodusb: Null interface ptr\n");
			memset(ptr, 0x0, sizeof(struct usb_interface_descriptor));
		} else if (!uif->cur_altsetting) {
			pr_err("goodusb: Null current alternate setting\n");
			memset(ptr, 0x0, sizeof(struct usb_interface_descriptor));
		} else {
			uid = &(uif->cur_altsetting->desc);
			memcpy(ptr, uid, sizeof(struct usb_interface_descriptor));
		}
		ptr += sizeof(struct usb_interface_descriptor);
	}
	
	/* SHA1 */
	tfm = crypto_alloc_hash("sha1", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm)) {
		pr_err("goodusb: tfm allocation failed\n");
		ret = -1;
		goto BUF_FREE;
	}

	/* Generate the fingerprint */
	ret = 0;
	desc.tfm = tfm;
	desc.flags = 0;
	sg_init_one(&sg, buf, total_len);
	ret = crypto_hash_digest(&desc, &sg, sg.length, fp);
	if (ret) {
		pr_err("goodusb: crypto_hash_digest() failed\n");
		ret = -1;
	}

	crypto_free_hash(tfm);

BUF_FREE:
	kfree(buf);

if (goodusb_perf_fp) {
	do_gettimeofday(&end_tv);
	pr_info("goodusb-perf: fingerprint took [%lu] us\n",
		GOODUSB_MBM_SUB_TV(start_tv, end_tv));
}

	return ret;
}

/* Init/Exit */
int goodusb_init(void)
{
	pr_info("goodusb: into [%s]\n", __func__);

	/* Init netlink socket */
        struct netlink_kernel_cfg cfg = {
                .input = goodusb_nl_handler,
        };

        if (!goodusb_nl_sock) {
                goodusb_nl_sock = netlink_kernel_create(&init_net, GOODUSB_NETLINK, &cfg);
                if(!goodusb_nl_sock) {
                        pr_err("goodusb: netlink socket creation failure - abort\n");
			return -1;
		}
                pr_info("goodusb: netlink socket created\n");
        }

	/* Init FP DB */
	INIT_LIST_HEAD(&goodusb_fp_db);

	/* Init Dev WL */
	memcpy(goodusb_dev_wl.dev,
		goodusb_dev_wl_k,
		sizeof(struct goodusb_dev_whitelist_entry)*GOODUSB_DEV_WL_KERNEL_SIZE);
	goodusb_dev_wl.count = goodusb_dev_wl_k_size;

	/* Mark it */
	goodusb_inited = 1;

	/* Others are init'd by the compiler - static global vars */
	return 0;
}

void goodusb_exit(void)
{
	/* 
	 * We can save the FP DB and WL into user-space files
	 * via netlink. In this way, we do not need to learn the
	 * device capability and information again. Correspondingly,
	 * when goodusb is init'd with khubd, the gud should try
	 * to sync the files saved before with the goodusb once the
	 * netlink is ready. However, I am not going to implement it:)
	 * Feb 5, 2015
	 * daveti
	 * Alright, I am doing it now - K.R.K.C.
	 * Apr 9, 2015
	 * daveti
	 */
	if (!goodusb_inited)
		return;

	/* Sync the FP db with the gud */
	/* This may NOT be needed as we will sync FP for
	 * each new device with a new fingerprint
	 */
	goodusb_db_sync();

	/* Say bye to the gud */
	goodusb_nl_send_bye();

	/* Close the socket */
	netlink_kernel_release(goodusb_nl_sock);

	/* Destory the FP DB */
	goodusb_db_destory();
}

int goodusb_start(void)
{
	return goodusb_inited;
}
