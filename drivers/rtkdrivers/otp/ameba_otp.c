// SPDX-License-Identifier: GPL-2.0+
/*
* Realtek OTP support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/
#include <asm/io.h>
#include <common.h>
#include <dm/device.h>
#include <dm/device_compat.h>
#include <dm/read.h>
#include <misc.h>

#include <linux/delay.h>
#include <linux/bitops.h>

#define RETRY_COUNT					3
#define RTK_OTP_ACCESS_PWD			0x69
#define RTK_OTP_POLL_TIMES			20000

#define RTK_OTPC_BIT_BUSY			BIT(8)
#define RTK_OTPC_OTP_AS				0x0008
#define RTK_OTPC_OTP_CTRL			0x0014
#define RTK_OTPC_OTP_PARAM			0x0040
#define RTK_REG_AON_PWC				0x0000

/*!<R/WPD/ET 0  Write this bit will trig an indirect read or write. Write 1: trigger write write 0: trigger read After this operation done, this bit will toggle. */
#define RTK_OTPC_BIT_EF_RD_WR_NS	((u32)0x00000001 << 31)
#define RTK_OTPC_MASK_EF_PG_PWD		((u32)0x000000FF << 24)
#define RTK_OTPC_MASK_EF_DATA_NS	((u32)0x000000FF << 0)
#define RTK_AON_BIT_PWC_AON_OTP		((u32)0x00000001 << 0)
#define RTK_OTPC_EF_PG_PWD(x)		((u32)(((x) & 0x000000FF) << 24))
#define RTK_OTPC_EF_ADDR_NS(x)		((u32)(((x) & 0x000007FF) << 8))
#define RTK_OTPC_EF_MODE_SEL_NS(x)	((u32)(((x) & 0x00000007) << 19))

enum rtk_otp_opmode {
	RTK_OTP_USER_MODE = 0,
	RTK_OTP_PGR_MODE = 2,
};

struct rtk_otp_platdata {
	fdt_size_t mem_len;
	void __iomem * mem_base;
	void __iomem * sys_base;
};

static void rtk_otp_wait_for_busy(struct rtk_otp_platdata *rtk_otp, u32 flags)
{
	u32 val = 0;

	if (!rtk_otp) return;

	val = readl(rtk_otp->mem_base + RTK_OTPC_OTP_PARAM);

	if (flags) {
		while (val & RTK_OTPC_BIT_BUSY) {
			udelay(100);
		}

		val |= RTK_OTPC_BIT_BUSY;
	} else
		val &= ~RTK_OTPC_BIT_BUSY;

	writel(val, rtk_otp->mem_base + RTK_OTPC_OTP_PARAM);
}

static bool rtk_otp_get_power_state(struct rtk_otp_platdata *rtk_otp)
{
	u32 state = 0;

	state = readl((volatile void __iomem *)(rtk_otp->sys_base + RTK_REG_AON_PWC));
	if (state & RTK_AON_BIT_PWC_AON_OTP)
		return true;

	return false;
}

static void rtk_otp_set_power_cmd(struct rtk_otp_platdata *rtk_otp, bool enable)
{
	u32 state = readl((volatile void __iomem *)(rtk_otp->sys_base + RTK_REG_AON_PWC));

	if (enable)
		state |= RTK_AON_BIT_PWC_AON_OTP;
	else
		state &= ~RTK_AON_BIT_PWC_AON_OTP;

	writel(state, (volatile void __iomem *)(rtk_otp->sys_base + RTK_REG_AON_PWC));
}

static void rtk_otp_access_cmd(struct rtk_otp_platdata *rtk_otp, bool enable)
{
	u32 val = 0;

	val = readl(rtk_otp->mem_base + RTK_OTPC_OTP_CTRL);
	if (enable) {
		val |= RTK_OTPC_EF_PG_PWD(RTK_OTP_ACCESS_PWD);
	} else
		val &= ~RTK_OTPC_MASK_EF_PG_PWD;

	writel(val, rtk_otp->mem_base + RTK_OTPC_OTP_CTRL);
}

static void rtk_otp_power_switch(struct rtk_otp_platdata *rtk_otp, bool bwrite, bool pstate)
{
	if (pstate == true) {
		if (rtk_otp_get_power_state(rtk_otp) == false)
			rtk_otp_set_power_cmd(rtk_otp, true);
	}

	rtk_otp_access_cmd(rtk_otp, bwrite);
}

static int rtk_otp_readb(struct udevice *dev, int addr, u8 *data, int mode)
{
	struct rtk_otp_platdata *rtk_otp = dev_get_platdata(dev);
	volatile void __iomem * otp_as_addr = NULL;
	int ret = 0;
	u32 val = 0;
	u32 idx = 0;

	if (!rtk_otp) {
		*data = 0xff;
		ret = -1;
		goto exit;
	}

	if (addr >= rtk_otp->mem_len) {
		dev_err(dev, "Read addr: %x, mem_len: %x\n", addr, rtk_otp->mem_len);
		*data = 0xff;
		ret = -1;
		goto exit;
	}

	otp_as_addr = (volatile void __iomem *)(rtk_otp->mem_base + RTK_OTPC_OTP_AS);

	rtk_otp_wait_for_busy(rtk_otp, 1); //wait and set busy flag
	rtk_otp_power_switch(rtk_otp, false, true);

	val = RTK_OTPC_EF_ADDR_NS(addr);
	if (mode == RTK_OTP_PGR_MODE)  // for Program Margin Read
		val |= RTK_OTPC_EF_MODE_SEL_NS(RTK_OTP_PGR_MODE);

	writel(val, otp_as_addr);

	/* 10~20us is needed */
	val = readl(otp_as_addr);
	while (idx < RTK_OTP_POLL_TIMES && (!(val & RTK_OTPC_BIT_EF_RD_WR_NS))) {
		udelay(5);
		idx++;
		val = readl(otp_as_addr);
	}

	if (idx < RTK_OTP_POLL_TIMES) {
		*data = (u8)(val & RTK_OTPC_MASK_EF_DATA_NS);
	} else {
		*data = 0xff;
		ret = -1;
		dev_err(dev, "Read addr: %x failed\n", addr);
	}

	rtk_otp_power_switch(rtk_otp, false, false);
	rtk_otp_wait_for_busy(rtk_otp, 0); //reset busy flag

exit:
	return ret;
}

static int rtk_otp_program(struct udevice *dev, int addr, u8 data)
{
	struct rtk_otp_platdata *rtk_otp = dev_get_platdata(dev);
	volatile void __iomem * otp_as_addr = NULL;
	int ret = 0;
	u32 idx = 0;
	u32 val = 0;

	if (data == 0xff) return 0;

	if (!rtk_otp) {
		ret = -1;
		goto exit;
	}

	if (addr >= rtk_otp->mem_len) {
		dev_err(dev, "Write addr: %x, mem_len: %x\n", addr, rtk_otp->mem_len);
		ret = -1;
		goto exit;
	}

	otp_as_addr = (volatile void __iomem *)(rtk_otp->mem_base + RTK_OTPC_OTP_AS);

	rtk_otp_wait_for_busy(rtk_otp, 1); //wait and set busy flag
	rtk_otp_power_switch(rtk_otp, true, true);

	val = data | RTK_OTPC_EF_ADDR_NS(addr) | RTK_OTPC_BIT_EF_RD_WR_NS;

	writel(val, otp_as_addr);

	/* 10~20us is needed */
	val = readl(otp_as_addr);
	while (idx < RTK_OTP_POLL_TIMES && (val & RTK_OTPC_BIT_EF_RD_WR_NS)) {
		udelay(5);
		idx++;
		val = readl(otp_as_addr);
	}

	if (idx >= RTK_OTP_POLL_TIMES) {
		ret = -1;
		dev_err(dev, "Write addr: %x failed\n", addr);
	}

	rtk_otp_power_switch(rtk_otp, false, false);
	rtk_otp_wait_for_busy(rtk_otp, 0); //reset busy flag

exit:
	return ret;
}

static int rtk_otp_writeb(struct udevice *dev, int addr, u8 data)
{
	struct rtk_otp_platdata *rtk_otp = dev_get_platdata(dev);
	int ret = 0;
	u8 val = 0;
	u8 retry = 0;
	u8 target = data;

	if (rtk_otp_readb(dev, addr, &val, RTK_OTP_PGR_MODE) < 0) {
		dev_err(dev, "Addr: %x PMR read error\n", addr);
		ret = -1;
		goto exit;
	}

retry:
	/*do not need program bits include originally do not need program
	(bits equals 1 in data) and already prgoramed bits(bits euqals 0 in Temp) */
	data |= ~val;

	/*program*/
	if (rtk_otp_program(dev, addr, data) < 0) {
		dev_err(dev, "Addr: %x otp write error\n", addr);
		ret = -1;
		goto exit;
	}

	/*Read after program*/
	if (rtk_otp_readb(dev, addr, &val, RTK_OTP_PGR_MODE) < 0) {
		dev_err(dev, "Addr: %x PMR read after program error\n", addr);
		ret = -1;
		goto exit;
	}

	/*program do not get desired value,the OTP can be programmed at most 3 times
		here only try once.*/
	if (val != target) {
		if (retry++ >= RETRY_COUNT) {
			dev_err(dev, "Addr: %x read check error\n", addr);
			ret = -1;
			goto exit;
		} else
			goto retry;
	}

exit:
	return ret;
}

static int rtk_otp_read(struct udevice *dev, int offset, void *buf, int size)
{
	struct rtk_otp_platdata *rtk_otp = dev_get_platdata(dev);
	u8 *pbuf = (u8 *)buf;
	u8 val;
	int i = 0;
	for (; i < size; i++) {
		if (rtk_otp_readb(dev, offset++, &val, RTK_OTP_USER_MODE) < 0) {
			return -1;
		}

		*pbuf++ = val;
	}
	return size;
}

static int rtk_otp_write(struct udevice *dev, int offset, const void *buf, int size)
{
	int i = 0;
	u8 *data = (u8 *)buf;

	for (; i < size; i++) {
		if (rtk_otp_writeb(dev, offset++, *data++) < 0) {
			dev_err(dev, "Rtk otp write failed\n");
			return -1;
		}
	}

	return 0;
}

static int rtk_otp_ofdata_to_platdata(struct udevice *dev)
{
	struct rtk_otp_platdata *plat = dev_get_platdata(dev);

	plat->mem_base = (void __iomem *)dev_read_addr_size_index(dev, 0, &plat->mem_len);
	plat->sys_base = (void __iomem *)dev_read_addr_index(dev, 1);

	return 0;
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
};
