/*
 * nlm.c
 * Source file for protocol NLM used by GoodUSB
 * Feb 10, 2015
 * Support for automatic untrusted dev rerouting to the HoneyUSB
 * May 8, 2015
 * Support for description index for the user mode
 * May 12, 2015
 * root@davejingtian.org
 * http://davejingtian.org
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nlm.h"

/* NLM queue definitions */
static nlmsgt nlm_queue[NLM_QUEUE_MSG_NUM];
static int nlm_queue_index; /* Always pointing to the next avalible position */

/* NLM protocol related methods */

/* Display the uchar given length */
void nlm_display_uchar(unsigned char *src, int len, char *header)
{
	int i;

	printf("%s\n", header);
	for (i = 0; i < len; i++) {
		if ((i+1) % NLM_UCHAR_NUM_PER_LINE != 0)
			printf("%02x ", src[i]);
		else
			printf("%02x\n", src[i]);
	}
	printf("\n");   
}

/* Display the NLM message */
void nlm_display_msg(nlmsgt *msg)
{
	int i;

	printf("Dump the GoodUSB nlmsg:\n"
		"opcode = [%u]\n", msg->opcode);

	switch (msg->opcode) {

	case GOODUSB_NETLINK_OP_INIT:
		printf("INIT:\nkernel msg = [%s]\n", msg->k2u.product);
		break;

	case GOODUSB_NETLINK_OP_K2U:
		printf("K2U:\nconfig_num = [%u]\n"
			"interface_total_num = [%u]\n"
			"dev = [%p]\n"
			"product = [%s]\n"
			"manufacturer = [%s]\n"
			"idVendor = [%04x]\n"
			"idProduct = [%04x]\n",
			msg->config_num,
			msg->interface_total_num,
			msg->dev,
			msg->k2u.product,
			msg->k2u.manufacturer,
			msg->k2u.idVendor,
			msg->k2u.idProduct);

		for (i = 0; i < msg->interface_total_num; i++)
			printf("interface [%d]:\n"
				"class = [0x%x]\n"
				"subClass = [0x%x]\n"
				"protocol = [0x%x]\n"
				"ep_num = [%u]\n",
				i,
				(msg->k2u.interface_array)[i].if_class,
				(msg->k2u.interface_array)[i].if_sub_class,
				(msg->k2u.interface_array)[i].if_protocol,
				(msg->k2u.interface_array)[i].if_ep_num);
		break;

	case GOODUSB_NETLINK_OP_FP_K2U_SEC:
                printf("K2U_SEC:\nconfig_num = [%u]\n"
                        "interface_total_num = [%u]\n"
                        "dev = [%p]\n"
                        "product = [%s]\n"
                        "manufacturer = [%s]\n"
			"idVendor = [%04x]\n"
			"idProduct = [%04x]\n"
			"limited_hid_driver = [%u]\n"
			"security_pic_index = [%u]\n"
			"description_index = [%u]\n",
                        msg->config_num,
                        msg->interface_total_num,
                        msg->dev,
                        msg->k2u_sec.k2u.product,
                        msg->k2u_sec.k2u.manufacturer,
			msg->k2u_sec.k2u.idVendor,
			msg->k2u_sec.k2u.idProduct,
			msg->k2u_sec.limited_hid_driver,
			msg->k2u_sec.security_pic_index,
			msg->k2u_sec.description_index);

                for (i = 0; i < msg->interface_total_num; i++)
                        printf("interface [%d]:\n"
                                "class = [0x%x]\n"
                                "subClass = [0x%x]\n"
                                "protocol = [0x%x]\n"
                                "ep_num = [%u]\n",
                                i,
                                (msg->k2u_sec.k2u.interface_array)[i].if_class,
                                (msg->k2u_sec.k2u.interface_array)[i].if_sub_class,
                                (msg->k2u_sec.k2u.interface_array)[i].if_protocol,
                                (msg->k2u_sec.k2u.interface_array)[i].if_ep_num);
		
		nlm_display_uchar(msg->k2u_sec.interface_mask,
			GOODUSB_INTERFACE_NUM_MAX, "interface_mask:");
		break;

	case GOODUSB_NETLINK_OP_U2K:
                printf("U2K:\nconfig_num = [%u]\n"
                        "interface_total_num = [%u]\n"
                        "dev = [%p]\n"
			"limited_hid = [%u]\n"
			"security_pic_index = [%u]\n"
			"description_index = [%u]\n"
			"disable = [%u]\n",
                        msg->config_num,
                        msg->interface_total_num,
                        msg->dev,
			msg->u2k.limited_hid,
			msg->u2k.security_pic_index,
			msg->u2k.description_index,
			msg->u2k.disable);
		nlm_display_uchar(msg->u2k.interface_mask,
			GOODUSB_INTERFACE_NUM_MAX, "interface_mask:");
		break;

	case GOODUSB_NETLINK_OP_FP_U2K_SEC:
                printf("U2K_SEC:\nconfig_num = [%u]\n"
                        "interface_total_num = [%u]\n"
                        "dev = [%p]\n"
                        "limited_hid = [%u]\n"
                        "security_pic_index = [%u]\n"
			"description_index = [%u]\n"
			"disable = [%u]\n"
			"enable = [%u]\n",
                        msg->config_num,
                        msg->interface_total_num,
                        msg->dev,
                        msg->u2k_sec.u2k.limited_hid,
                        msg->u2k_sec.u2k.security_pic_index,
			msg->u2k_sec.u2k.description_index,
			msg->u2k_sec.u2k.disable,
			msg->u2k_sec.enable);
                nlm_display_uchar(msg->u2k_sec.u2k.interface_mask,
                        GOODUSB_INTERFACE_NUM_MAX, "interface_mask:");

		break;

	case GOODUSB_NETLINK_OP_FP_K2U_SYN:
	case GOODUSB_NETLINK_OP_FP_U2K_SYN:
		printf("SYN:\nlimited_hid_driver = [%u]\n"
			"security_pic_index = [%u]\n"
			"description_index = [%u]\n",
			msg->fp.limited_hid_driver,
			msg->fp.security_pic_index,
			msg->fp.description_index);
		nlm_display_uchar(msg->fp.fingerprint,
			GOODUSB_FINGERPRINT_LEN, "fingerprint");
		nlm_display_uchar(msg->fp.interface_mask,
			GOODUSB_INTERFACE_NUM_MAX, "interface_mask");
		break;

	case GOODUSB_NETLINK_OP_BYE:
		/* Nothing to display */
		printf("BYE\n");
		break;

	default:
		printf("Error: invalid opcode [%u]\n", msg->opcode);
		break;
	}
}

/* NLM queue related methods */

/* Init the NLM queue */
void nlm_init_queue(void)
{
	memset((unsigned char *)nlm_queue, 0x0, NLM_QUEUE_SIZE);
	nlm_queue_index = 0;
}

/* Add msgs into the NLM queue from raw binary data */
int nlm_add_msg_queue(nlmsgt *msg)
{
	/* Save the TLV into nlm msg queue */
	if (nlm_queue_index < NLM_QUEUE_MSG_NUM)
		nlm_queue[nlm_queue_index++] = *msg;
	else {
		printf("nlm_add_raw_msg_queue: Error - nlm queue is full\n");
		return -1;
	}

	return 0;
}

/* Clear all the msgs in the queue */
void nlm_clear_all_msg_queue(void)
{
	int i;

	/* Zero out the structs */
	for (i = 0; i < nlm_queue_index; i++)
		memset(&nlm_queue[i], 0x0, sizeof(nlmsgt));

	/* Reinit the index */
	nlm_queue_index = 0;
}

/* Get the number of msgs in the queue */
int nlm_get_msg_num_queue(void)
{
	return nlm_queue_index;
}

/* Get the msg from the queue based on the index */
nlmsgt * nlm_get_msg_queue(int index)
{
	return &(nlm_queue[index]);
}
