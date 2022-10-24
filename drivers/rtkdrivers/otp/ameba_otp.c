#include <common.h>
#include <dm/device.h>
#include <dm/read.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <misc.h>
#include <linux/delay.h>


#include "ameba_ipc.c"

#define LINUX_IPC_OTP_PHY_READ8		    	0
#define LINUX_IPC_OTP_PHY_WRITE8		1
#define LINUX_IPC_OTP_LOGI_READ_MAP		2
#define LINUX_IPC_OTP_LOGI_WRITE_MAP		3
#define LINUX_IPC_EFUSE_REMAIN_LEN		4

#define OPT_REQ_MSG_PARAM_NUM			1024
#define OTP_IPC_RET_LEN				2

typedef struct otp_ipc_rx_res {
	int ret;
	int complete_num;
} otp_ipc_rx_res_t;

enum {
	IPC_USER_POINT = 0,
	IPC_USER_DATA = 1
};

typedef struct otp_ipc_host_req_msg {
	u32 otp_id;
	u32 addr;
	u32 len;
	u32 write_lock;
	u8 param_buf[OPT_REQ_MSG_PARAM_NUM];
} otp_ipc_host_req_t;

struct rtk_otp {
	struct udevice		*dev;
	struct aipc_ch          *potp_ipc_ch;
	otp_ipc_host_req_t      phy_msg;              /* host api message to send to device */
	//dma_addr_t              req_msg_phy_addr;       /* host api message to send to device */
	ipc_msg_struct_t        otp_ipc_msg;            /* to store ipc msg for api */
	struct completion       otp_complete;           /* only one otp process can send ipc instruction */
	struct rtk_otp_platdata *ameba_otp;
};

struct rtk_otp_platdata {
	struct aipc_device	pipc_dev;
	struct rtk_otp		otp_d;
	int			otp_done;
};

int otp_ipc_host_otp_send_msg(struct rtk_otp *otp_d)
{
	int ret = 0;

	if (!otp_d) {
		printf("ERROR: host_otp_priv is NULL when to send msg!\n");
		ret = -1;
		goto func_exit;
	}

	memset((u8*)&(otp_d->otp_ipc_msg), 0, sizeof(ipc_msg_struct_t));
	otp_d->otp_ipc_msg.msg = (u32)&otp_d->phy_msg;
	otp_d->otp_ipc_msg.msg_type = IPC_USER_POINT;
	otp_d->otp_ipc_msg.msg_len = sizeof(otp_ipc_host_req_t);
	flush_dcache_all();
	ameba_ipc_channel_send(otp_d->potp_ipc_ch, &(otp_d->otp_ipc_msg), &otp_d->ameba_otp->pipc_dev);
func_exit:
	return ret;
}

static void otp_ipc_host_otp_task(struct rtk_otp *otp_d) {
	struct udevice *pdev = NULL;
	int msg_len = 0;

	if (!otp_d || !otp_d->potp_ipc_ch) {
		printf("ERROR: potp_ipc_ch is NULL!\n");
		goto func_exit;
    	}

	pdev = otp_d->potp_ipc_ch->pdev;
	if (!pdev) {
		printf("ERROR: device is NULL!\n");
		goto func_exit;
	}

	if (!otp_d->otp_ipc_msg.msg || !otp_d->otp_ipc_msg.msg_len) {
		printf("ERROR: Invalid device message!\n");
		goto func_exit;
	}
	msg_len = otp_d->otp_ipc_msg.msg_len;

	otp_d->ameba_otp->otp_done = 1;

func_exit:
	return;
}

/* input: */
/* data: in type of otp_ipc_host_req_t. */
/* otp_ipc_host_req_t: otp_id is LINUX_IPC_OTP_PHY_READ8/LINUX_IPC_OTP_PHY_WRITE8/LINUX_IPC_OTP_LOGI_READ_MAP/LINUX_IPC_OTP_LOGI_WRITE_MAP/LINUX_IPC_EFUSE_REMAIN_LEN. */
/* otp_ipc_host_req_t: addr is the address to read/write. */
/* otp_ipc_host_req_t: len is the len to read/write. */
/* otp_ipc_host_req_t: write_lock is the lock to write. (if set to 0, the param_buf will be written into Efuse) */
/* otp_ipc_host_req_t: param_buf is the value to write, or the read value returned. */
/* output: */
/* result: for otp read, result shall be a malloc buffer. */
/*         for otp write, the result can be ignored. */
/*		   for EFUSE Remain read, result length is 4*(u8), combined as a (u32) remain value. result[0] is the largest edian. */
int rtk_otp_process(struct rtk_otp_platdata *plat)
{
	otp_ipc_host_req_t *preq_msg = &plat->otp_d.phy_msg;
	int ret = 0;
	struct rtk_otp *otp_d = NULL;
	extern int ameba_ipc_poll(struct aipc_device *pipc_dev);

	otp_d = &plat->otp_d;

	if (otp_d->ameba_otp->otp_done) {
		otp_d->ameba_otp->otp_done = 0;
	} else {
		return -EBUSY;
	}

	if (preq_msg->len > OPT_REQ_MSG_PARAM_NUM) {
		pr_err("OTP parameters requested is too much. Maximum for %d bytes. \n", OPT_REQ_MSG_PARAM_NUM);
		goto err_ret;
	}

	ret = otp_ipc_host_otp_send_msg(otp_d);

	ret = ameba_ipc_poll(&otp_d->ameba_otp->pipc_dev);
	if (ret) {
		otp_ipc_host_otp_task(otp_d);
	} else {
		printf("ERROR: OTP failed.");
		goto err_ret;
	}
	return ret;

err_ret:
	return -EINVAL;
}

static int rtk_otp_read(struct udevice *dev, int offset, void *buf, int size)
{
	struct rtk_otp_platdata *plat = dev_get_platdata(dev);
	otp_ipc_host_req_t *preq_msg = &plat->otp_d.phy_msg;

	plat->otp_d.phy_msg.otp_id = (u32)LINUX_IPC_OTP_PHY_READ8;
	plat->otp_d.phy_msg.addr = (u32)offset;
	plat->otp_d.phy_msg.len = (u32)size;

	rtk_otp_process(plat);

	flush_dcache_all();
	if (buf && preq_msg->len) {
		memcpy(buf, preq_msg->param_buf, preq_msg->len);
	}

	return size;
}

static int rtk_otp_write(struct udevice *dev, int offset, const void *buf, int size)
{
	struct rtk_otp_platdata *plat = dev_get_platdata(dev);

	plat->otp_d.phy_msg.otp_id = (u32)LINUX_IPC_OTP_PHY_WRITE8;
	plat->otp_d.phy_msg.addr = (u32)offset;
	plat->otp_d.phy_msg.len = (u32)size;
	memcpy(plat->otp_d.phy_msg.param_buf, buf, size);
	plat->otp_d.phy_msg.write_lock = 0;

	rtk_otp_process(plat);
	return 0;
}

static int rtk_otp_ofdata_to_platdata(struct udevice *dev)
{
	struct rtk_otp_platdata *plat = dev_get_platdata(dev);

	ameba_ipc_probe(dev, &plat->pipc_dev);

	return 0;
}

int rtk_otp_probe(struct udevice *dev)
{
	struct rtk_otp *otp_d = NULL;
	int ret;
	struct rtk_otp_platdata *plat = dev_get_platdata(dev);

	otp_d = &plat->otp_d;
    	otp_d->dev = dev;

	/* allocate the ipc channel */
	otp_d->potp_ipc_ch = ameba_ipc_alloc_ch(sizeof(struct rtk_otp));
	if (!otp_d->potp_ipc_ch) {
		ret = -ENOMEM;
		printf("ERROR: no memory for ipc channel.\n");
		goto func_exit;
	}

	/* initialize the ipc channel */
	otp_d->potp_ipc_ch->port_id = AIPC_PORT_NP;
	otp_d->potp_ipc_ch->ch_id = 6; /* configure channel 6 */
	otp_d->potp_ipc_ch->ch_config = AIPC_CONFIG_NOTHING;
	otp_d->potp_ipc_ch->priv_data = otp_d;

	/* regist the otp_d ipc channel */
	ret = ameba_ipc_channel_register(otp_d->potp_ipc_ch, &plat->pipc_dev);
	if (ret < 0) {
        	printf("ERROR: regist otp_d channel error.\n");
		goto free_ipc_ch;
	}

	if (!otp_d->potp_ipc_ch->pdev) {
        	printf("ERROR: no device in registed IPC channel.\n");
		goto free_ipc_ch;
	}

    	plat->otp_done = 1;
	otp_d->ameba_otp = plat;
	goto func_exit;

free_ipc_ch:
	kfree(otp_d->potp_ipc_ch);

func_exit:
	return ret;
}

static const struct misc_ops rtk_otp_ops = {
	.read = rtk_otp_read,
	.write = rtk_otp_write,
};

static const struct udevice_id rtk_otp_ids[] = {
	{ .compatible = "realtek,ameba-otp" },
	{}
};

U_BOOT_DRIVER(rtk_otp) = {
	.name = "rtk_otp",
	.id = UCLASS_MISC,
	.of_match = rtk_otp_ids,
	.ofdata_to_platdata = rtk_otp_ofdata_to_platdata,
	.platdata_auto_alloc_size = sizeof(struct rtk_otp_platdata),
	.ops = &rtk_otp_ops,
   	.probe = rtk_otp_probe,
};
