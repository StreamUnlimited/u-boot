// SPDX-License-Identifier: GPL-2.0+
/*
* Realtek OTP support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <common.h>
#include <cpu_func.h>
#include <dm/device.h>
#include <dm/read.h>
#include <dm/device_compat.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <misc.h>
#include <linux/delay.h>

#include "ameba_ipc.h"

#define LINUX_IPC_OTP_PHY_READ8		    0U
#define LINUX_IPC_OTP_PHY_WRITE8		1U
#define LINUX_IPC_OTP_LOGI_READ_MAP		2U
#define LINUX_IPC_OTP_LOGI_WRITE_MAP	3U
#define LINUX_IPC_EFUSE_REMAIN_LEN		4U

#define OPT_REQ_MSG_PARAM_NUM			1024
#define OTP_IPC_RET_LEN					2

enum {
	IPC_USER_POINT = 0,
	IPC_USER_DATA = 1
};

struct otp_ipc_host_req_msg {
	u32 otp_id;
	u32 addr;
	u32 len;
	u32 write_lock;
	u8 param_buf[OPT_REQ_MSG_PARAM_NUM];
};

struct rtk_otp {
	struct udevice *dev;
	struct aipc_ch *potp_ipc_ch;
	struct otp_ipc_host_req_msg phy_msg;	/* host api message to send to device */
	struct ipc_msg_struct otp_ipc_msg;		/* to store ipc msg for api */
	struct completion otp_complete;			/* only one otp process can send ipc instruction */
	struct rtk_otp_platdata *ameba_otp;
};

struct rtk_otp_platdata {
	struct aipc_device	pipc_dev;
	struct rtk_otp		otp;
	int			otp_done;
};

static int otp_ipc_host_otp_send_msg(struct udevice *dev, struct rtk_otp *otp)
{
	if (!otp) {
		dev_err(dev, "OTP is NULL\n");
		return -1;
	}

	memset((u8 *)&(otp->otp_ipc_msg), 0, sizeof(struct ipc_msg_struct));
	otp->otp_ipc_msg.msg = (u32)&otp->phy_msg;
	otp->otp_ipc_msg.msg_type = IPC_USER_POINT;
	otp->otp_ipc_msg.msg_len = sizeof(struct otp_ipc_host_req_msg);
	flush_dcache_all();

	ameba_ipc_channel_send(dev, otp->potp_ipc_ch, &(otp->otp_ipc_msg), &otp->ameba_otp->pipc_dev);

	return 0;
}

static void otp_ipc_host_otp_task(struct udevice *dev, struct rtk_otp *otp)
{
	struct udevice *pdev = NULL;
	int msg_len = 0;

	if (!otp || !otp->potp_ipc_ch) {
		dev_err(dev, "IPC channel is NULL\n");
		goto func_exit;
	}

	pdev = otp->potp_ipc_ch->pdev;
	if (!pdev) {
		dev_err(dev, "Device is NULL\n");
		goto func_exit;
	}

	if (!otp->otp_ipc_msg.msg || !otp->otp_ipc_msg.msg_len) {
		dev_err(dev, "Invalid device message\n");
		goto func_exit;
	}

	msg_len = otp->otp_ipc_msg.msg_len;

	otp->ameba_otp->otp_done = 1;

func_exit:
	return;
}

/* input: */
/* data: in type of struct otp_ipc_host_req_msg. */
/* struct otp_ipc_host_req_msg: otp_id is LINUX_IPC_OTP_PHY_READ8/LINUX_IPC_OTP_PHY_WRITE8/LINUX_IPC_OTP_LOGI_READ_MAP/LINUX_IPC_OTP_LOGI_WRITE_MAP/LINUX_IPC_EFUSE_REMAIN_LEN. */
/* struct otp_ipc_host_req_msg: addr is the address to read/write. */
/* struct otp_ipc_host_req_msg: len is the len to read/write. */
/* struct otp_ipc_host_req_msg: write_lock is the lock to write. (if set to 0, the param_buf will be written into Efuse) */
/* struct otp_ipc_host_req_msg: param_buf is the value to write, or the read value returned. */
/* output: */
/* result: for otp read, result shall be a malloc buffer. */
/*         for otp write, the result can be ignored. */
/*		   for EFUSE Remain read, result length is 4*(u8), combined as a (u32) remain value. result[0] is the largest edian. */
static int rtk_otp_process(struct udevice *dev, struct rtk_otp_platdata *plat)
{
	struct otp_ipc_host_req_msg *preq_msg = &plat->otp.phy_msg;
	int ret = 0;
	struct rtk_otp *otp = NULL;

	otp = &plat->otp;

	if (otp->ameba_otp->otp_done) {
		otp->ameba_otp->otp_done = 0;
	} else {
		return -EBUSY;
	}

	if (preq_msg->len > OPT_REQ_MSG_PARAM_NUM) {
		dev_err(dev, "Too many OTP parameters, max %d bytes\n", OPT_REQ_MSG_PARAM_NUM);
		return -EINVAL;
	}

	ret = otp_ipc_host_otp_send_msg(dev, otp);

	ret = ameba_ipc_poll(&otp->ameba_otp->pipc_dev);
	if (ret) {
		dev_err(dev, "IPC poll failed\n");
		return ret;
	}
	
	otp_ipc_host_otp_task(dev, otp);

	return 0;
}

static int rtk_otp_read(struct udevice *dev, int offset, void *buf, int size)
{
	struct rtk_otp_platdata *plat = dev_get_platdata(dev);
	struct otp_ipc_host_req_msg *preq_msg = &plat->otp.phy_msg;

	plat->otp.phy_msg.otp_id = LINUX_IPC_OTP_PHY_READ8;
	plat->otp.phy_msg.addr = (u32)offset;
	plat->otp.phy_msg.len = (u32)size;

	rtk_otp_process(dev, plat);

	flush_dcache_all();
	if (buf && preq_msg->len) {
		memcpy(buf, preq_msg->param_buf, preq_msg->len);
	}

	return size;
}

static int rtk_otp_write(struct udevice *dev, int offset, const void *buf, int size)
{
	struct rtk_otp_platdata *plat = dev_get_platdata(dev);

	plat->otp.phy_msg.otp_id = LINUX_IPC_OTP_PHY_WRITE8;
	plat->otp.phy_msg.addr = (u32)offset;
	plat->otp.phy_msg.len = (u32)size;
	memcpy(plat->otp.phy_msg.param_buf, buf, size);
	plat->otp.phy_msg.write_lock = 0;

	rtk_otp_process(dev, plat);
	return 0;
}

static int rtk_otp_ofdata_to_platdata(struct udevice *dev)
{
	struct rtk_otp_platdata *plat = dev_get_platdata(dev);

	ameba_ipc_probe(dev, &plat->pipc_dev);

	return 0;
}

static int rtk_otp_probe(struct udevice *dev)
{
	int ret;
	struct rtk_otp *otp = NULL;
	struct rtk_otp_platdata *plat = dev_get_platdata(dev);

	otp = &plat->otp;
	otp->dev = dev;

	/* allocate the ipc channel */
	otp->potp_ipc_ch = ameba_ipc_alloc_ch(sizeof(struct rtk_otp));
	if (!otp->potp_ipc_ch) {
		ret = -ENOMEM;
		dev_err(dev, "No memory for IPC channel\n");
		goto func_exit;
	}

	/* Initialize the IPC channel */
	otp->potp_ipc_ch->port_id = AIPC_PORT_NP;
	otp->potp_ipc_ch->ch_id = 6; /* configure channel 6 */
	otp->potp_ipc_ch->ch_config = AIPC_CONFIG_NOTHING;
	otp->potp_ipc_ch->priv_data = otp;

	/* Register the otp ipc channel */
	ret = ameba_ipc_channel_register(dev, otp->potp_ipc_ch, &plat->pipc_dev);
	if (ret < 0) {
		dev_err(dev, "Fail to register IPC channel\n");
		goto free_ipc_ch;
	}

	if (!otp->potp_ipc_ch->pdev) {
		dev_err(dev, "No device in registered IPC channel\n");
		goto free_ipc_ch;
	}

	plat->otp_done = 1;
	otp->ameba_otp = plat;

	goto func_exit;

free_ipc_ch:
	kfree(otp->potp_ipc_ch);

func_exit:
	return ret;
}

static const struct misc_ops rtk_otp_ops = {
	.read = rtk_otp_read,
	.write = rtk_otp_write,
};

static const struct udevice_id rtk_otp_ids[] = {
	{ .compatible = "realtek,amebad2-otp" },
	{}
};

U_BOOT_DRIVER(rtk_otp) = {
	.name = "realtek-amebad2-otp",
	.id = UCLASS_MISC,
	.of_match = rtk_otp_ids,
	.ofdata_to_platdata = rtk_otp_ofdata_to_platdata,
	.platdata_auto_alloc_size = sizeof(struct rtk_otp_platdata),
	.ops = &rtk_otp_ops,
	.probe = rtk_otp_probe,
};
