/*
 * gud.c
 * Source file for gud 
 * gud (GoodUSB user daemon) is a netlink server used to recv the
 * netlink msg from the Linux kernel USB core and user's expectation
 * from the user space and to send user's expectation to the Linux kernel.
 * Feb 9, 2015
 * Added support for security pic and FP DB sync with the kernel
 * Apr 17, 2015
 * Added automatic untrusted dev rerouting to the HoneyUSB
 * May 8, 2015
 * Added support for description index for the user mode
 * May 12, 2015
 * Added support for performance evaluation
 * May 26, 2015
 * root@davejingtian.org
 * http://davejingtian.org
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <asm/types.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <netinet/in.h>
#include <linux/netlink.h>
#include "nlm.h"
#include "usb.h"

/* Global defs */
#define GUD_NETLINK		31
#define GUD_RECV_BUFF_LEN	1024*1024
#define GUD_GUI_PATH		"./gudGUI.sh"
#define GUD_GUI_SEC_PATH	"./gudGUIsec.py"
#define GUD_HONEY_PATH		"./gudHoney.sh"
#define GUD_GUI_INPUT_FILE	"./input/gudGUI.input"
#define GUD_GUI_LIMITED_HID	"Use_limited_HID_driver"
#define GUD_GUI_SECURITY_PIC	"Security_pic_index"
#define GUD_GUI_SEC_ENABLE	"Enable"
#define GUD_GUI_SEPARATOR	"|"
#define GUD_GUI_RECV_BUFF_LEN	1024
#define GUD_GUI_DEV_DESC_LEN	128
#define GUD_HONEY_BUFF_LEN	64
#define GUD_DB_PATH		"./db/fpdb.dat"
#define GUD_PERF_US_IN_SEC	1000000LL
#define GUD_PERF_SUB_TV(s, e)			\
	((e.tv_sec*GUD_PERF_US_IN_SEC + e.tv_usec) -	\
	(s.tv_sec*GUD_PERF_US_IN_SEC + s.tv_usec))

/* Global variables */
extern char *optarg;
static struct sockaddr_nl gud_nl_addr;
static struct sockaddr_nl gud_nl_dest_addr;
static pid_t gud_pid;
static int gud_sock_fd;
static FILE *gud_db_fp;
static void *recv_buff;
static int debug_enabled;
static int pro_enabled;
static char gui_buff[GUD_GUI_RECV_BUFF_LEN];
static char gui_dev_desc[GUD_GUI_DEV_DESC_LEN];
static int gud_perf_enabled = 0;
static int gud_perf_honey_enabled = 0;

/* Signal term handler */
static void gud_signal_term(int signal)
{
	/* Close the db */
	if (gud_db_fp)
		fclose(gud_db_fp);

	/* Close the socket */
	if (gud_sock_fd != -1)    
		close(gud_sock_fd);

	/* Free netlink receive buffer */
 	if (recv_buff != NULL)
		free(recv_buff);

	exit(EXIT_SUCCESS);
}

/* Setup signal handler */
static int signals_init(void)
{
	int rc;
	sigset_t sigmask;
	struct sigaction sa;

	sigemptyset(&sigmask);
	if ((rc = sigaddset(&sigmask, SIGTERM)) || (rc = sigaddset(&sigmask, SIGINT))) {
		printf("gud - Error: sigaddset [%s]\n", strerror(errno));
		return -1;
 	}

	sa.sa_flags = 0;
	sa.sa_mask = sigmask;
	sa.sa_handler = gud_signal_term;
	if ((rc = sigaction(SIGTERM, &sa, NULL)) || (rc = sigaction(SIGINT, &sa, NULL))) {
		printf("gud - Error: signal SIGTERM or SIGINT not registered [%s]\n", strerror(errno));
		return -1;
	}

	return 0;
}

/* Send the nlmsgt via the netlink socket */
static int gud_netlink_send(nlmsgt *msg_ptr)
{
	struct nlmsghdr *nlh;
	struct iovec iov;
	struct msghdr msg;
	int rtn;
	unsigned char *data;
	int data_len;

	// Convert the nlmsgt into binary data
	data_len = NLM_MSG_LEN;
	data = (unsigned char *)malloc(data_len);
	memcpy(data, msg_ptr, NLM_MSG_LEN);

	// Init the stack struct to avoid potential error
	memset(&iov, 0, sizeof(iov));
	memset(&msg, 0, sizeof(msg));

	// Create the netlink msg
	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(data_len));
	memset(nlh, 0, NLMSG_SPACE(data_len));
	nlh->nlmsg_len = NLMSG_SPACE(data_len);
	nlh->nlmsg_pid = gud_pid;
	nlh->nlmsg_flags = 0;

	// Copy the binary data into the netlink message
	memcpy(NLMSG_DATA(nlh), data, data_len);

	// Nothing to do for test msg - it is already what it is
	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	msg.msg_name = (void *)&gud_nl_dest_addr;
	msg.msg_namelen = sizeof(gud_nl_dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	// Send the msg to the kernel
	rtn = sendmsg(gud_sock_fd, &msg, 0);
	if (rtn == -1)
	{
		printf("gud_netlink_send: Error on sending netlink msg to the kernel [%s]\n",
			strerror(errno));
		goto OUT;
	}

	if (debug_enabled)
		printf("gud_netlink_send: Info - send netlink msg to the kernel\n");

OUT:
	free(nlh);
	free(data);
	return 0;
}

/* Init the netlink with initial nlmsg */
static int gud_init_netlink(void)
{
	int result;
	nlmsgt init_msg;
	char *msg_str = "__hello_from_gud__";

	// Add the opcode into the msg
	memset(&init_msg, 0, sizeof(nlmsgt));
	init_msg.opcode = GOODUSB_NETLINK_OP_INIT;
	memcpy(init_msg.u2k.interface_mask, msg_str, strlen(msg_str) + 1);

	// Send the msg to the kernel
	printf("gud_init_netlink: Info - send netlink init msg to the kernel\n");
	result = gud_netlink_send(&init_msg);
	if (result == -1)
		printf("gud_init_netlink: Error on sending netlink init msg to the kernel [%s]\n",
			strerror(errno));

	return result;
}

/* Prepare for the GUI */
static int gud_init_gui(nlmsgt *req)
{
	/*
	 * Basically, we are trying to construct the
	 * gudGUI.input file based on the format file
	 */
	FILE *fp;
	int i;

	/* Defensive checking */
	if (!req) {
		printf("gud_init_gui: Error - Null req\n");
		return -1;
	}

	/* Open/create the input the file */
	fp = fopen(GUD_GUI_INPUT_FILE, "w+");
	if (!fp) {
		printf("gud_init_gui: fopen failed, err [%s]\n", strerror(errno));
		return -1;
	}

	/* Let's dance~ */
	fprintf(fp, "pro=%s\n"
		"configNum=%d\n"
		"interfaceTotalNum=%d\n"
		"product=%s\n"
		"manufacturer=%s\n",
		(pro_enabled == 1 ? "true" : "false"),
		req->config_num,
		req->interface_total_num,
		req->k2u.product,
		req->k2u.manufacturer);

	/* Handle pro mode */
	if (pro_enabled) {
		for (i = 0; i < req->interface_total_num; i++)
			fprintf(fp, "interface[%d]=%s:0x%x:0x%x:%d\n", i,
				usb_get_class_desc((req->k2u.interface_array)[i].if_class),
				(req->k2u.interface_array)[i].if_sub_class,
				(req->k2u.interface_array)[i].if_protocol,
				(req->k2u.interface_array)[i].if_ep_num);
	}

	fclose(fp);
	return 0;
}

/* Start gud GUI */
static int gud_start_gui(int isSec)
{
	FILE *output;
	
	/* Init the GUI buff */
	memset(gui_buff, 0x0, GUD_GUI_RECV_BUFF_LEN);

	/* Start GUI */
	if (isSec)
		output = popen(GUD_GUI_SEC_PATH, "r");
	else
		output = popen(GUD_GUI_PATH, "r");

	if (!output) {
		printf("gud_start_gui: Error on popen\n");
		return -1;
	}

	/* Read the user's input */
	while (fgets(gui_buff, GUD_GUI_RECV_BUFF_LEN, output));
	gui_buff[GUD_GUI_RECV_BUFF_LEN-1] = '\0';
	if (debug_enabled)
		printf("gud_start_gui: Info - gui_buff [%s]\n", gui_buff);

	return 0;
}

/* Build the reply to the kernel (u2k) */
static int gud_build_reply(nlmsgt *req, nlmsgt *rep, int gui_failed)
{
	int i, ret;
	char tmp[32] = {0};
	char *sec;

	if (debug_enabled)
		printf("gud: Info - into [%s], req [%p], rep [%p], gui_failed [%d]\n",
			__func__, req, rep, gui_failed);

	if (!req || !rep) {
		printf("gud: Error - Null req/rep, aborting\n");
		return -1;
	}

	/* Init the reply */
	memset(rep, 0x0, sizeof(*rep));
	rep->opcode = GOODUSB_NETLINK_OP_U2K;
	rep->config_num = req->config_num;
	rep->interface_total_num = req->interface_total_num;
	rep->dev = req->dev;

	/* Handle the GUI failure */
	if (gui_failed) {
		printf("gud: Warning - GUI failed, enabling the complete device\n");
		memset(rep->u2k.interface_mask, 0x1, rep->interface_total_num);
		rep->u2k.limited_hid = 0;
		rep->u2k.security_pic_index = 0;
		rep->u2k.description_index = USB_DEV_UNKNOWN;
		rep->u2k.disable = 0;
		return 0;
	}

	/* Handle the "Cancel" case from GUI */
	if (gui_buff[0] == 0x0) {
		printf("gud: Warning - User Canceled, enabling the complete device\n");
		memset(rep->u2k.interface_mask, 0x1, rep->interface_total_num);
		rep->u2k.limited_hid = 0;
		rep->u2k.security_pic_index = 0;
		rep->u2k.description_index = USB_DEV_UNKNOWN;
		rep->u2k.disable = 0;
		return 0;
	}

	/* Handle the pro mode */
	if (pro_enabled) {
		/* Check the GUI buff for key word */
		for (i = 0; i < rep->interface_total_num; i++) {
			/* Construct the keyword */
			snprintf(tmp, 32, "interface[%d]", i);
			if (strstr(gui_buff, tmp))
				(rep->u2k.interface_mask)[i] = 1;
		}
		/* Set the HID flag */
		if (strstr(gui_buff, GUD_GUI_LIMITED_HID))
			rep->u2k.limited_hid = 1;
		/* Set the description index */
		rep->u2k.description_index = USB_DEV_UNKNOWN;

	} else {
		/* Get the description of the device */
		memset(gui_dev_desc, 0x0, GUD_GUI_DEV_DESC_LEN);
		sec = strstr(gui_buff, GUD_GUI_SEPARATOR);
		if (!sec) {
			printf("gud - Warning: no separator - set the device unknown\n");
			rep->u2k.description_index = USB_DEV_UNKNOWN;
		} else {
			memcpy(gui_dev_desc, gui_buff, (sec-gui_buff));
			rep->u2k.description_index = usb_get_dev_from_desc(gui_dev_desc);
		}
		/* Handle the stupid user mode */
		ret = usb_generate_u2k(gui_dev_desc, req,
					rep->u2k.interface_mask,
					&(rep->u2k.limited_hid));
		if (ret)
			printf("gud - Error: usb_generate_u2k failed\n");
	}

	/* Handle the security pic index */
	sec = strstr(gui_buff, GUD_GUI_SECURITY_PIC);
	if (!sec) {
		printf("gud - Warning: no seucrity pic index - enable the device by default\n");
		rep->u2k.security_pic_index = 0;
		rep->u2k.disable = 0;
	} else {
		/* NOTE: we assume there is no newline in the gui_buff */
		rep->u2k.security_pic_index = (unsigned char)strtoul(sec+strlen(GUD_GUI_SECURITY_PIC), NULL, 10);
		if (rep->u2k.security_pic_index == 0) {
			printf("gud - Warning: the device will be disabled\n");
			rep->u2k.disable = 1;
		} else {
			printf("gud - Info: security picture index [%d]\n", rep->u2k.security_pic_index);
			rep->u2k.disable = 0;
		}
	}
	if (debug_enabled)
		printf("gud - Debug: security_pic_index[%u], description_index[%u]\n",
			rep->u2k.security_pic_index,
			rep->u2k.description_index);

	return 0;	
}

/* Prepare for the GUIsec */
static int gud_init_gui_sec(nlmsgt *req)
{
	/*
	 * Basically, we are trying to construct the
	 * gudGUI.input file based on the format file
	 * NOTE: the gudGUI.input is reused for GUIsec
	 * and the format is different with new fields appended!
	 */
	FILE *fp;
	int i;

	/* Defensive checking */
	if (!req) {
		printf("gud_init_gui_sec: Error - Null req\n");
		return -1;
	}

	/* Open/create the input the file */
	fp = fopen(GUD_GUI_INPUT_FILE, "w+");
	if (!fp) {
		printf("gud_init_gui_sec: fopen failed, err [%s]\n", strerror(errno));
		return -1;
	}

	/* Let's dance~ */
	fprintf(fp, "pro=%s\n"
		"configNum=%d\n"
		"interfaceTotalNum=%d\n"
		"product=%s\n"
		"manufacturer=%s\n"
		"limitedHidDriver=%u\n"
		"securityPicIndex=%u\n"
		"description=%s\n",
		(pro_enabled == 1 ? "true" : "false"),
		req->config_num,
		req->interface_total_num,
		req->k2u_sec.k2u.product,
		req->k2u_sec.k2u.manufacturer,
		req->k2u_sec.limited_hid_driver,
		req->k2u_sec.security_pic_index,
		usb_get_dev_desc(req->k2u_sec.description_index));

	/* Handle pro mode */
	if (pro_enabled) {
		for (i = 0; i < req->interface_total_num; i++)
			fprintf(fp, "interface=[%d]%s:0x%x:0x%x:%d,enable[%u]\n", i,
				usb_get_class_desc((req->k2u_sec.k2u.interface_array)[i].if_class),
				(req->k2u_sec.k2u.interface_array)[i].if_sub_class,
				(req->k2u_sec.k2u.interface_array)[i].if_protocol,
				(req->k2u_sec.k2u.interface_array)[i].if_ep_num,
				(req->k2u_sec.interface_mask)[i]);
	}

	fclose(fp);
	return 0;
}

/* Build the reply to the kernel (u2k_sec) */
static int gud_build_reply_sec(nlmsgt *req, nlmsgt *rep, int gui_failed)
{
	/*
	 * Technically, we only need to care about the 'enable' field within
	 * the u2k_sec struct, as the whole point of GUIsec is to determine if
	 * this device should be enabled or not. However, to support the pro mode
	 * in GUIsec, we may allow users to reconfigure the device during this phase.
	 * As a result, all fields within the fingerprint except the fingerprint could
	 * be changed/updated. In this case, we need to make sure we have something valid
	 * within the u2k_sec.u2k!
	 */
	char *sec;

	if (debug_enabled)
		printf("gud: Info - into [%s], req [%p], rep [%p], gui_failed [%d]\n",
			__func__, req, rep, gui_failed);

	if (!req || !rep) {
		printf("gud: Error - Null req/rep, aborting\n");
		return -1;
	}

	/* Init the reply */
	memset(rep, 0x0, sizeof(*rep));
	rep->opcode = GOODUSB_NETLINK_OP_FP_U2K_SEC;
	rep->config_num = req->config_num;
	rep->interface_total_num = req->interface_total_num;
	rep->dev = req->dev;

	/* Handle the GUI failure */
	if (gui_failed)
		printf("gud: Warning - GUI sec failed, enabling the device using previous configuration\n");
	/* Handle the "Cancel" case from GUI */
	else if (gui_buff[0] == 0x0)
		printf("gud: Warning - User canceled, enabling the device using previous configuration\n");

	/* Copy the request regardless of enable flag */
	memcpy(rep->u2k_sec.u2k.interface_mask, req->k2u_sec.interface_mask, GOODUSB_INTERFACE_NUM_MAX);
	rep->u2k_sec.u2k.limited_hid = req->k2u_sec.limited_hid_driver;
	rep->u2k_sec.u2k.security_pic_index = req->k2u_sec.security_pic_index;
	rep->u2k_sec.u2k.description_index = req->k2u_sec.description_index;

	/* Handle the pro mode */
	if (pro_enabled) {
		/*
		 * TODO: need the gudGUIsec.py support to allow users to
		 * reconfigure the device. For now, there is no difference between
		 * user mode and pro mode for the GUIsec.
		 * Apr 23, 2015
		 * daveti
		 */
	}

	/* Handle the enable flag */
	sec = strstr(gui_buff, GUD_GUI_SEC_ENABLE);
	if (!sec) {
		/* By default, we should enable the device for better usability */
		printf("gud - Warning: no enable flag, enabling the device using previous configuration\n");
		rep->u2k_sec.enable = 1;
	} else {
		/* NOTE: we assume there should be NO newline in the gui_buff */
		rep->u2k_sec.enable = (unsigned char)strtoul(sec+strlen(GUD_GUI_SEC_ENABLE), NULL, 10);
	}

	if (debug_enabled)
		printf("gud - Debug: enable[%u]\n", rep->u2k_sec.enable);

	return 0;	
}

/* Check if device redirect is needed */
static int gud_need_to_redirect(nlmsgt *rep)
{
	/* Defensive checking */
	if (!rep) {
		printf("gud - Error: null reply msg\n");
		return -1;
	}

	/* Handle different cases */
	switch (rep->opcode) {

	case GOODUSB_NETLINK_OP_U2K:
		/* Simple check */
		if (rep->u2k.disable == 1)
			return 1;
		else
			return 0;
		break;

	case GOODUSB_NETLINK_OP_FP_U2K_SEC:
		/* Simple check */
		if (rep->u2k_sec.enable == 1)
			return 0;
		else
			return 1;
		break;

	default:
		printf("gud - Error: unsupported opcode [%d]\n", rep->opcode);
		break;
	}

	return -1;
}

/* Redirect the device to the HoneyUSB */
static int gud_go_to_HoneyUSB(nlmsgt *req)
{
	char buf[GUD_HONEY_BUFF_LEN] = {0};
	int ret;
	struct timeval start_tv, end_tv;

	/* Start perf */
	if (gud_perf_honey_enabled)
		gettimeofday(&start_tv, 0);

	/* Defensive checking */
	if (!req) {
		printf("gud - Error: null request msg\n");
		return -1;
	}

	/* Handle different cases */
	switch (req->opcode) {

	case GOODUSB_NETLINK_OP_U2K:
                if (debug_enabled)
                        printf("gud - Debug: redirecting the device [0x%04x,0x%04x] to the HoneyUSB\n",
                                req->k2u.idVendor, req->k2u.idProduct);

                /* Call the redirecting script */
                snprintf(buf, GUD_HONEY_BUFF_LEN, "%s 0x%04x 0x%04x",
                        GUD_HONEY_PATH, req->k2u.idVendor, req->k2u.idProduct);
                if (debug_enabled)
                        printf("gud - Debug: calling [%s]\n", buf);
                ret = system(buf);
                /* FIXME: ignore the ret for now */
		break;

	case GOODUSB_NETLINK_OP_FP_K2U_SEC:
		if (debug_enabled)
			printf("gud - Debug: redirecting the device [0x%04x,0x%04x] to the HoneyUSB\n",
				req->k2u_sec.k2u.idVendor, req->k2u_sec.k2u.idProduct);

		/* Call the redirecting script */
		snprintf(buf, GUD_HONEY_BUFF_LEN, "%s 0x%04x 0x%04x",
			GUD_HONEY_PATH, req->k2u_sec.k2u.idVendor, req->k2u_sec.k2u.idProduct);
		if (debug_enabled)
			printf("gud - Debug: calling [%s]\n", buf);
		ret = system(buf);
		/* FIXME: ignore the ret for now */
		break;

	default:
		printf("gud - Error: unsupported opcode [%d]\n", req->opcode);
		break;
	}

	/* End perf */
	if (gud_perf_honey_enabled) {
		gettimeofday(&end_tv, 0);
		printf("gud-perf: HoneyUSB took [%llu] us\n",
			GUD_PERF_SUB_TV(start_tv, end_tv));
	}

	return 0;
}

/* Build and send the reply to the kernel (perf) */
static int gud_perf_build_send_reply(nlmsgt *req, nlmsgt *rep, int is_sec)
{
        /*
         * For perf purpose, we do not start GUI serving the users.
         * Instead, upon receiving the request from the kernel, we
         * fabricate the reply immediately, enabling all interfaces
         * and hardcoding other FP related stuffs.
         * May 27, 2015
         * daveti
         */
        int ret;

        if (debug_enabled)
                printf("gud: Info - into [%s], req [%p], rep [%p], is_sec [%d]\n",
                        __func__, req, rep, is_sec);

        if (!req || !rep) {
                printf("gud: Error - Null req/rep, aborting\n");
                return -1;
        }

        /* Init the reply */
        memset(rep, 0x0, sizeof(*rep));
        rep->config_num = req->config_num;
        rep->interface_total_num = req->interface_total_num;
        rep->dev = req->dev;

        /* Continue */
        if (is_sec) {
                rep->opcode = GOODUSB_NETLINK_OP_FP_U2K_SEC;
                memcpy(rep->u2k_sec.u2k.interface_mask, req->k2u_sec.interface_mask, GOODUSB_INTERFACE_NUM_MAX);
                rep->u2k_sec.u2k.limited_hid = req->k2u_sec.limited_hid_driver;
                rep->u2k_sec.u2k.security_pic_index = req->k2u_sec.security_pic_index;
                rep->u2k_sec.u2k.description_index = req->k2u_sec.description_index;
                rep->u2k_sec.enable = 1;
		if (gud_perf_honey_enabled)
			rep->u2k_sec.enable = 0;
        } else {
                rep->opcode = GOODUSB_NETLINK_OP_U2K;
                memset(rep->u2k.interface_mask, 0x1, rep->interface_total_num);
                rep->u2k.limited_hid = 0;
                rep->u2k.security_pic_index = 0;
                rep->u2k.description_index = USB_DEV_UNKNOWN;
                rep->u2k.disable = 0;
		if (gud_perf_honey_enabled)
			rep->u2k.disable = 1;
        }

        /* Send the reply to the kernel */
        ret = gud_netlink_send(rep);
        if (ret != 0)
                printf("gud - Error: gud_netlink_send failed\n");

        /* Check for HoneyUSB redirection */
        if (gud_perf_honey_enabled) {
                ret = gud_go_to_HoneyUSB(req);
                if (ret != 0)
                        printf("gud - Error: gud_go_to_HoneyUSB failed\n");
        }

        return ret;
}

/* Sync one FP with the DB */
static int gud_sync_fp_db(nlmsgt *msg)
{
	if (!msg) {
		printf("gud - Error: null msg\n");
		return -1;
	}

	/*
	 * We should NOT recv fp with security_pic_index == 0...
	 * But in case...
	 */
	if (msg->fp.security_pic_index == 0) {
		printf("gud - Warning: security_pic_index is 0\n");
		return -1;
	}

	/* Defensive checking for db file */
	if (!gud_db_fp) {
		printf("gud - Error: FP DB is not opened\n");
		return -1;
	}

	/* Write to the DB file */
	if (fwrite(&msg->fp, sizeof(msg->fp), 1, gud_db_fp) != 1) {
		printf("gud - Error: fwrite failed\n");
		return -1;
	}
	
	return 0;	
}

/* Sync one FP with the kernel */
static int gud_sync_fp(struct goodusb_fp *fp)
{
	nlmsgt rep;

	if (!fp) {
		printf("gud - Error: null fp\n");
		return -1;
	}

	/* Init the reply */
	memset(&rep, 0x0, sizeof(rep));
	rep.opcode = GOODUSB_NETLINK_OP_FP_U2K_SYN;

	/* Copy the fp */
	memcpy(&rep.fp, fp, sizeof(*fp));

	/* Send the msg to the kernel */
	if (gud_netlink_send(&rep) != 0) {
		printf("gud - Error: gud_netlink_send failed\n");
		return -1;
	}

	return 0;
}

/* Sync the FP DB with the kernel */
static int gud_sync_db(void)
{
	int ret;
	struct goodusb_fp fp;

	/* Open the db */
	gud_db_fp = fopen(GUD_DB_PATH, "a+b");
	if (!gud_db_fp) {
		printf("gud - Info: fopen DB [%s] failed - %s\n",
			GUD_DB_PATH, strerror(errno));
		return -1;
	}

	/* Read-in the saved fingerprints */
	while (!feof(gud_db_fp)) {
		ret = fread(&fp, sizeof(fp), 1, gud_db_fp);
		if (ret == 0) {
			/* Done */
			return 0;
		}
		else if (ret != 1) {
			printf("gud - Error: fread failed\n");
			return -1;
		}
		/* Sync this fp with the kernel */
		ret = gud_sync_fp(&fp);
		if (ret != 0) 
			printf("gud - Error: gud_sync_fp failed\n");
	}

	return 0;	
}

static void usage(void)
{
	fprintf(stderr, "\tusage: gud [-p] [-c <config file> [-h]\n\n");
	fprintf(stderr, "\t-p|--pro\tenable professional mode\n");
	fprintf(stderr, "\t-d|--debug\tenable debug mode\n");
	fprintf(stderr, "\t-c|--config\tpath to configuration file (TBD)\n");
	fprintf(stderr, "\t-h|--help\tdisplay this help message\n");
	fprintf(stderr, "\n");
}

int main(int argc, char **argv)
{
	int result;
	int c, option_index = 0;
	int recv_size;
	int num_of_msg;
	int i;
	int gui_failed;
	nlmsgt *msg_ptr;
	nlmsgt reply_msg;
	struct option long_options[] = {
		{"help", 0, NULL, 'h'},
		{"debug", 0, NULL, 'd'},
		{"pro", 0, NULL, 'p'},
		{"config", 1, NULL, 'c'},
		{0, 0, 0, 0}
	};
	struct nlmsghdr *nh;
	struct nlmsgerr *nlm_err_ptr;

	/* Process the arguments */
	while ((c = getopt_long(argc, argv, "hpdc:", long_options, &option_index)) != -1) {
		switch (c) {
		case 'p':
			printf("gud - Info: pro mode enabled\n");
			pro_enabled = 1;
			break;
		case 'd':
			printf("gud - Info: debug mode enabled\n");
			debug_enabled = 1;
			break;
		case 'c':
			printf("gud - Warning: may support in future\n");
			break;
		case 'h':
			/* fall through */
		default:
			usage();
			return -1;
		}
	}

	/* Set the signal handlers */
	if (signals_init() != 0) {
		printf("gud - Error: failed to set up the signal handlers\n");
		return -1;
	}

	/* Init the NLM queue */
	nlm_init_queue();

	do{
		/* Create the netlink socket */
		printf("gud - Info: waiting for a new connection\n");
		while ((gud_sock_fd = socket(PF_NETLINK, SOCK_RAW, GUD_NETLINK)) < 0);

		/* Bind the socket */
		memset(&gud_nl_addr, 0, sizeof(gud_nl_addr));
		gud_nl_addr.nl_family = AF_NETLINK;
		gud_pid = getpid();
		printf("gud - Info: pid [%u]\n", gud_pid);
		gud_nl_addr.nl_pid = gud_pid;
		if (bind(gud_sock_fd, (struct sockaddr*)&gud_nl_addr, sizeof(gud_nl_addr)) == -1)
		{
			printf("gud - Error: netlink bind failed [%s], aborting\n", strerror(errno));
			return -1;
		}

		/* Setup the netlink destination socket address */
		memset(&gud_nl_dest_addr, 0, sizeof(gud_nl_dest_addr));
		gud_nl_dest_addr.nl_family = AF_NETLINK;
		gud_nl_dest_addr.nl_pid = 0;
		gud_nl_dest_addr.nl_groups = 0;
		printf("gud - Info: gud netlink socket init done\n");

		/* Prepare the recv buffer */
		recv_buff = calloc(1, GUD_RECV_BUFF_LEN);
		struct iovec iov = { recv_buff, GUD_RECV_BUFF_LEN };
		struct msghdr msg = { &gud_nl_dest_addr,
			sizeof(gud_nl_dest_addr),
			&iov, 1, NULL, 0, 0 };

		/* Send the initial testing nlmsgt to the kernel module */
		result = gud_init_netlink();
		if (result != 0)
		{
			printf("gud - Error: gud_init_netlink failed\n");
			return -1;
		}

		/* Sync with the kernel */
		result = gud_sync_db();
		if (result != 0)
			printf("gud - Warning: gud_sync_db failed\n");

		/* Retrive the data from the kernel */
		recv_size = recvmsg(gud_sock_fd, &msg, 0);
		if (debug_enabled)
			nlm_display_msg((nlmsgt *)(NLMSG_DATA(recv_buff)));
		printf("gud - Info: got netlink init msg response from the kernel [%s]\n",
			((nlmsgt *)(NLMSG_DATA(recv_buff)))->k2u.product);
		printf("gud - Info: gud is up and running\n");

		do {
			/* Recv the msg from the kernel */
			printf("gud - Info: waiting for a new request\n");
			recv_size = recvmsg(gud_sock_fd, &msg, 0);
			if (recv_size == -1) {
				printf("gud - Error: recv failed [%s]\n", strerror(errno));
				continue;
            		}
			else if (recv_size == 0) {
   				printf("gud - Warning: kernel netlink socket is closed\n");
				continue;
			}
			printf("gud - Info: received a new msg\n");

			/* Pop nlmsgs into the NLM queue
			 * Note that we do not allow multipart msg from the kernel.
			 * So we do not have to call NLMSG_NEXT() and only one msg
			 * would be recv'd for each recvmsg call. NLM queue seems
			 * to be redundant if gud is single thread. But it is
			 * needed if gud supports multiple threads.
			 * Feb 9, 2015
			 * daveti
			 */
			nh = (struct nlmsghdr *)recv_buff;
			if (NLMSG_OK(nh, recv_size))
			{
				/* Make sure the msg is alright */
				if (nh->nlmsg_type == NLMSG_ERROR)
				{
					nlm_err_ptr = (struct nlmsgerr *)(NLMSG_DATA(nh));
					printf("gud - Error: nlmsg error [%d]\n",
						nlm_err_ptr->error);
					continue;
				}

				/* Ignore the noop */
				if (nh->nlmsg_type == NLMSG_NOOP)
					continue;

				/* Defensive checking - should always be non-multipart msg */
				if (nh->nlmsg_type != NLMSG_DONE)
				{
					printf("gud - Error: nlmsg type [%d] is not supported\n",
						nh->nlmsg_type);
					continue;
				}

				/* Break if received goodbye msg */
				if (((nlmsgt *)NLMSG_DATA(nh))->opcode == GOODUSB_NETLINK_OP_BYE)
				{
					printf("gud - Info: got goodbye msg\n");
					if (debug_enabled)
						nlm_display_msg(NLMSG_DATA(nh));
					break;
				}

				/* Pop the msg into the NLM queue */
				if (nlm_add_msg_queue(NLMSG_DATA(nh)) != 0)
				{
					printf("gud - Error: nlm_add_raw_msg_queue failed\n");
					continue;
				}
			}
			else
			{
				printf("gud - Error: netlink msg is corrupted\n");
				continue;
			}

			/* NOTE: even if nlm_add_raw_msg_queue may fail, there may be msgs in queue
			 * Right now, gud is single thread - recving msgs from the kernel space
			 * and then processing each msg upon this recving. However, the code below
			 * could be separated into a worker thread which could run parallelly with
			 * the main thread. This may be an option to improve the performance even
			 * the mutex has to be added into NLM queue implementation...
			 * Feb 24, 2014
			 * daveti
			 */

			/* Go thru the queue */
			num_of_msg = nlm_get_msg_num_queue(); /* should be always 1 */
			if (debug_enabled)
				printf("gud - Debug: got [%d] msgs(packets) in the queue\n", num_of_msg);

			for (i = 0; i < num_of_msg; i++)
			{
				/* Get the nlmsgt msg */
				msg_ptr = nlm_get_msg_queue(i);

				/* Debug */
				if (debug_enabled)
					nlm_display_msg(msg_ptr);

				switch (msg_ptr->opcode) {

				case GOODUSB_NETLINK_OP_FP_K2U_SYN:
					/* Sync with the db */
					result = gud_sync_fp_db(msg_ptr);
					if (result != 0)
						printf("gud - Error: gud_sync_fp_db failed\n");
					break;

				case GOODUSB_NETLINK_OP_K2U:
					/* Check for perf mode */
					if (gud_perf_enabled) {
						result = gud_perf_build_send_reply(msg_ptr, &reply_msg, 0);
						if (result != 0)
							printf("gud - Error gud_perf_build_send_reply failed\n");
						break;
					}

					/* Init the GUI failed flag */
					gui_failed = 0;

					/* Construct gud tmp file */
					result = gud_init_gui(msg_ptr);
					if (result != 0) {
						printf("gud - Error: gud_prepare_gui failed\n");
						gui_failed = 1;
					}
					else {
						/* Call the gud GUI */
						result = gud_start_gui(0);
						if (result != 0) {
							printf("gud - Error: gud_start_gui failed\n");
							gui_failed = 1;
						}
					}

					/* Construct the reply from GUI results */
					result = gud_build_reply(msg_ptr, &reply_msg, gui_failed);
					if (result != 0) {
						printf("gud - Error: gud_build_reply failed\n");
					} else {
						/* Send the nlmsgt with quote to the kernel */
						printf("gud - Info: sending a reply\n");
						result = gud_netlink_send(&reply_msg);
						if (result != 0)
							printf("gud - Error: gud_netlink_send failed\n");
						/* Check if we need to go to the HoneyUSB */
						if (gud_need_to_redirect(&reply_msg) == 1) {
							result = gud_go_to_HoneyUSB(msg_ptr);
							if (result != 0)
								printf("gud - Error: gud_go_to_HoneyUSB failed\n");
							else
								printf("gud - Info: this device has been redirected into the HoneyUSB\n");
                                                }
					}
					break;

				case GOODUSB_NETLINK_OP_FP_K2U_SEC:
					/* Check for perf mode */
					if (gud_perf_enabled) {
						result = gud_perf_build_send_reply(msg_ptr, &reply_msg, 1);
						if (result != 0)
							printf("gud - Error: gud_perf_build_send_reply failed\n");
						break;
					}

					/* Init the GUI failed flag */
					gui_failed = 0;

                                        /* Construct gud tmp file */
                                        result = gud_init_gui_sec(msg_ptr);
                                        if (result != 0) {
                                                printf("gud - Error: gud_prepare_gui failed\n");
                                                gui_failed = 1;
                                        }
                                        else {
                                                /* Call the gud GUI */
                                                result = gud_start_gui(1);
                                                if (result != 0) {
                                                        printf("gud - Error: gud_start_gui failed\n");
                                                        gui_failed = 1;
                                                }
                                        }

                                        /* Construct the reply from GUI results */
                                        result = gud_build_reply_sec(msg_ptr, &reply_msg, gui_failed);
                                        if (result != 0) {
                                                printf("gud - Error: gud_build_reply failed\n");
                                        } else {
                                                /* Send the nlmsgt with quote to the kernel */
                                                printf("gud - Info: sending a reply\n");
                                                result = gud_netlink_send(&reply_msg);
                                                if (result != 0)
                                                        printf("gud - Error: gud_netlink_send failed\n");
						/* Check if we need to go to the HoneyUSB */
						if (gud_need_to_redirect(&reply_msg) == 1) {
							result = gud_go_to_HoneyUSB(msg_ptr);
							if (result != 0)
								printf("gud - Error: gud_go_to_HoneyUSB failed\n");
							else
								printf("gud - Info: this device has been redirected into the HoneyUSB\n");
						}
                                        }
                                        break;

				case GOODUSB_NETLINK_OP_U2K:
				case GOODUSB_NETLINK_OP_FP_U2K_SEC:
				case GOODUSB_NETLINK_OP_FP_U2K_SYN:
				case GOODUSB_NETLINK_OP_BYE:
					/* Go thru to the default case */
				default:
					printf("gud - Error: unsupported opcode [%u]\n", msg_ptr->opcode);
					break;
				}
			}

			/* Clear the queue before receiving again */
			nlm_clear_all_msg_queue();

		} while (1);

		/* Clean up before next connection */
		printf("gud - Info: closing the current connection\n");
		nlm_clear_all_msg_queue();
		close(gud_sock_fd);
		gud_sock_fd = -1;
		free(recv_buff);
		recv_buff = NULL;

	} while (1);

	/* To close correctly, we must receive a SIGTERM */
	return 0;
}
