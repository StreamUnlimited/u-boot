// SPDX-License-Identifier: GPL-2.0+
/*
* Realtek USB PHY support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <common.h>
#include <clk.h>
#include <div64.h>
#include <dm.h>
#include <fdtdec.h>
#include <generic-phy.h>
#include <log.h>
#include <reset.h>
#include <syscon.h>
#include <usb.h>
#include <asm/io.h>
#include <dm/device_compat.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <realtek/misc.h>

/* USB PHY registers */
#define USB_OTG_PHY_REG_E0								0xE0U
#define USB_OTG_PHY_REG_E1								0xE1U
#define USB_OTG_PHY_REG_E2								0xE2U
#define USB_OTG_PHY_REG_E3								0xE3U
#define USB_OTG_PHY_REG_E4								0xE4U
#define USB_OTG_PHY_REG_E5								0xE5U
#define USB_OTG_PHY_REG_E6								0xE6U
#define USB_OTG_PHY_REG_E7								0xE7U
#define USB_OTG_PHY_REG_F0								0xF0U
#define USB_OTG_PHY_REG_F1								0xF1U
#define USB_OTG_PHY_REG_F2								0xF2U
#define USB_OTG_PHY_REG_F3								0xF3U
#define USB_OTG_PHY_REG_F4								0xF4U
#define USB_OTG_PHY_REG_F5								0xF5U
#define USB_OTG_PHY_REG_F6								0xF6U
#define USB_OTG_PHY_REG_F7								0xF7U

/* USB_OTG_PHY_REG_E1 bits */
#define USB_OTG_PHY_REG_E1_BIT_EN_OTG_POS				7U
#define USB_OTG_PHY_REG_E1_BIT_EN_OTG					(0x1U << USB_OTG_PHY_REG_E1_BIT_EN_OTG_POS)

/* USB_OTG_PHY_REG_F4 bits */
#define USB_OTG_PHY_REG_F4_BIT_PAGE_SEL_POS				5U
#define USB_OTG_PHY_REG_F4_BIT_PAGE_SEL_MASK			(0x3U << USB_OTG_PHY_REG_F4_BIT_PAGE_SEL_POS)
#define USB_OTG_PHY_REG_F4_BIT_PAGE0					0U
#define USB_OTG_PHY_REG_F4_BIT_PAGE1					1U
#define USB_OTG_PHY_REG_F4_BIT_PAGE2					2U
#define USB_OTG_PHY_REG_F4_BIT_PAGE3					3U

/* USB OTG addon registers */
#define USB_OTG_ADDON_REG_CTRL							0x004 // 0x30004UL
#define USB_OTG_ADDON_REG_VND_STS_OUT					0x01C // 0x3001CUL

#define USB_OTG_ADDON_REG_CTRL_BIT_UPLL_CKRDY			BIT(5)  /* 1: USB PHY clock ready */
#define USB_OTG_ADDON_REG_CTRL_BIT_USB_OTG_RST			BIT(8)  /* 1: Enable USB OTG */
#define USB_OTG_ADDON_REG_CTRL_BIT_USB_DPHY_FEN			BIT(9)  /* 1: Enable USB DPHY */
#define USB_OTG_ADDON_REG_CTRL_BIT_USB_APHY_EN			BIT(14) /* 1: Enable USB APHY */
#define USB_OTG_ADDON_REG_CTRL_BIT_LS_HST_UTMI_EN		BIT(22) /* 1: Enable the support of low-speed host mode when using utmi 16bit */
#define USB_OTG_ADDON_REG_CTRL_BIT_HS_IP_GAP_OPT_POS	20U		/* MAC high-speed host inter-packet delay */
#define USB_OTG_ADDON_REG_CTRL_BIT_HS_IP_GAP_OPT_MASK	(0x3U << USB_OTG_ADDON_REG_CTRL_BIT_HS_IP_GAP_OPT_POS)
#define USB_OTG_ADDON_REG_CTRL_BIT_HS_IP_GAP_OPT		USB_OTG_ADDON_REG_CTRL_BIT_HS_IP_GAP_OPT_MASK

/* USB control registers */
#define REG_HSYS_USB_CTRL								0UL // 0x0060

#define HSYS_BIT_USB2_DIGOTGPADEN						BIT(28)
#define HSYS_BIT_USB_OTGMODE            				BIT(27)
#define HSYS_BIT_USB2_DIGPADEN           				BIT(26)
#define HSYS_BIT_ISO_USBA_EN             				BIT(25) /* Default 1, 1: enable signal from USBPHY analog */
#define HSYS_BIT_ISO_USBD_EN             				BIT(24)
#define HSYS_BIT_USBA_LDO_EN             				BIT(23)
#define HSYS_BIT_PDN_UPHY_EN             				BIT(20) /* Default 1, 0: enable USBPHY shutdown */
#define HSYS_BIT_PWC_UABG_EN             				BIT(19) /* Default 0, 1: enable USBPHY BG power cut */
#define HSYS_BIT_PWC_UAHV_EN             				BIT(18) /* Default 0, 1: enable USBPHY HV power cut */
#define HSYS_BIT_PWC_UALV_EN             				BIT(17) /* Default 0, 1: enable USBPHY LV power cut */
#define HSYS_BIT_OTG_ANA_EN              				BIT(16) /* Default 0, 1: enable USBPHY lv2hv, hv2lv check for OTG */

/* USB PHY vendor control register */
#define USB_OTG_GPVNDCTL								0x0034

#define USB_OTG_GPVNDCTL_REGDATA_Pos					(0U)
#define USB_OTG_GPVNDCTL_REGDATA_Msk					(0xFFUL << USB_OTG_GPVNDCTL_REGDATA_Pos) /*!< 0x000000FF */
#define USB_OTG_GPVNDCTL_REGDATA						USB_OTG_GPVNDCTL_REGDATA_Msk /*!< Register Data */
#define USB_OTG_GPVNDCTL_VCTRL_Pos						(8U)
#define USB_OTG_GPVNDCTL_VCTRL_Msk						(0xFFUL << USB_OTG_GPVNDCTL_VCTRL_Pos) /*!< 0x0000FF00 */
#define USB_OTG_GPVNDCTL_VCTRL							USB_OTG_GPVNDCTL_VCTRL_Msk /*!< UTMI+ Vendor Control Register Address */
#define USB_OTG_GPVNDCTL_VSTSDONE_Pos					(27U)
#define USB_OTG_GPVNDCTL_VSTSDONE_Msk					(0x1UL << USB_OTG_GPVNDCTL_VSTSDONE_Pos) /*!< 0x02000000 */
#define USB_OTG_GPVNDCTL_VSTSDONE						USB_OTG_GPVNDCTL_VSTSDONE_Msk /*!< VStatus Done */

/* USB PHY defines */
#define PHY_HIGH_ADDR(x)								(((x) & 0xF0) >> 4)
#define PHY_LOW_ADDR(x)									((x) & 0x0F)
#define PHY_DATA_MASK									0xFF

/* USB clocks */
#define REG_LSYS_FEN_GRP1								0x020C
#define REG_LSYS_CKE_GRP1								0x0218
#define REG_LSYS_CKE_GRP1								0x0218
#define REG_LSYS_AIP_CTRL1								0x025C
#define APBPeriph_USB_CLOCK								BIT(10)
#define APBPeriph_USB									BIT(12)

/* LSYS BG registers */
#define LSYS_BIT_BG_PWR                                 ((u32)0x00000001 << 8)          /* 1: power on ddrphy bandgap 0: shutdown bg */
#define LSYS_BIT_BG_ON_USB2                             ((u32)0x00000001 << 5)          /* 1: Bandgap USB2 current enable */
#define LSYS_MASK_BG_ALL                                ((u32)0x00000003 << 0)          /* 0x3  Bandgap enable mode */
#define LSYS_BG_ALL(x)                                  ((u32)(((x) & 0x00000003) << 0))
#define LSYS_GET_BG_ALL(x)                              ((u32)(((x >> 0) & 0x00000003)))

struct rtk_usb_phy_t {
	struct phy uphy;
	fdt_addr_t clk_base;
	fdt_addr_t hp_base;
	fdt_addr_t addon_base;
};

struct rtk_usb_phy_cal_data_t {
	u8 page;
	u8 addr;
	u8 val;
};

static const struct rtk_usb_phy_cal_data_t rtk_usb_cut_a_cal_data[] = {
	{0x00, 0xE0, 0x9D},
	{0x00, 0xE1, 0x19},
	{0x00, 0xE2, 0xDB},
	{0x00, 0xE4, 0x6D},
	{0x01, 0xE5, 0x0A},
	{0x01, 0xE6, 0xD8},
	{0x02, 0xE7, 0x32},
	{0x01, 0xE0, 0x04},
	{0x01, 0xE0, 0x00},
	{0x01, 0xE0, 0x04},

	{0xFF, 0x00, 0x00}
};

static const struct rtk_usb_phy_cal_data_t rtk_usb_cut_b_cal_data[] = {
	{0x00, 0xE0, 0x9D},
	{0x00, 0xE1, 0x19},
	{0x00, 0xE2, 0xDB},
	{0x00, 0xE4, 0x6B},
	{0x01, 0xE5, 0x0A},
	{0x01, 0xE6, 0xD8},
	{0x02, 0xE7, 0x32},
	{0x01, 0xE0, 0x04},
	{0x01, 0xE0, 0x00},
	{0x01, 0xE0, 0x04},

	{0xFF, 0x00, 0x00}
};

static int rtk_load_vcontrol(struct phy *p, uintptr_t dwc, u8 addr)
{
	u32 pvndctl = 0x0A300000;
	u32 count = 0;

	pvndctl |= (PHY_LOW_ADDR(addr) << USB_OTG_GPVNDCTL_VCTRL_Pos);
	writel(pvndctl, dwc + USB_OTG_GPVNDCTL);
	do {
		/* 1us timeout expected, 1ms for safe */
		udelay(1);
		if (++count > 1000) {
			dev_err(p->dev, "Vcontrol timeout\n");
			return -ETIMEDOUT;
		}

		pvndctl = readl(dwc + USB_OTG_GPVNDCTL);
	} while ((pvndctl & USB_OTG_GPVNDCTL_VSTSDONE) == 0);

	return 0;
}

static int rtk_phy_write(struct phy *p, uintptr_t dwc, u8 addr, u8 val)
{
	u32 tmp;
	int ret = 0;
	struct rtk_usb_phy_t *uphy = dev_get_priv(p->dev);
	fdt_addr_t addon_base = uphy->addon_base;

	tmp = readl(addon_base + USB_OTG_ADDON_REG_VND_STS_OUT);
	tmp &= (~PHY_DATA_MASK);
	tmp |= val;
	writel(tmp, addon_base + USB_OTG_ADDON_REG_VND_STS_OUT);

	ret = rtk_load_vcontrol(p, dwc, PHY_LOW_ADDR(addr));
	if (ret == 0) {
		ret = rtk_load_vcontrol(p, dwc, PHY_HIGH_ADDR(addr));
	}

	return ret;
}

static int rtk_phy_read(struct phy *p, uintptr_t dwc, u8 addr, u8 *val)
{
	u32 pvndctl;
	int ret = 0;
	u8 addr_read;
	if (addr >= 0xE0) {
		addr_read = addr - 0x20;
	} else {
		addr_read = addr;
	}

	ret = rtk_load_vcontrol(p, dwc, PHY_LOW_ADDR(addr_read));
	if (ret == 0) {
		ret = rtk_load_vcontrol(p, dwc, PHY_HIGH_ADDR(addr_read));
		if (ret == 0) {
			pvndctl = readl(dwc + USB_OTG_GPVNDCTL);
			*val = (pvndctl & USB_OTG_GPVNDCTL_REGDATA_Msk) >> USB_OTG_GPVNDCTL_REGDATA_Pos;
		}
	}

	return ret;
}

static int rtk_phy_page_set(struct phy *p, uintptr_t dwc, u8 page)
{
	int ret;
	u8 reg;

	ret = rtk_phy_read(p, dwc, USB_OTG_PHY_REG_F4, &reg);
	if (ret != 0) {
		return ret;
	}

	reg &= (~USB_OTG_PHY_REG_F4_BIT_PAGE_SEL_MASK);
	reg |= (page << USB_OTG_PHY_REG_F4_BIT_PAGE_SEL_POS) & USB_OTG_PHY_REG_F4_BIT_PAGE_SEL_MASK;
	ret = rtk_phy_write(p, dwc, USB_OTG_PHY_REG_F4, reg);

	return ret;
}

/**
  * @brief  USB PHY calibration
  *			Shall be called after soft disconnect
  * @param  None
  * @retval HAL status
  */
int rtk_phy_calibrate(struct phy *p, uintptr_t dwc)
{
	u8 ret = 0;
	struct rtk_usb_phy_cal_data_t *data;
	u8 old_page = 0xFF;

	if (p == NULL) {
		return -1;
	}

	/* 3ms + 2.5us from DD, 3ms already delayed after soft disconnect */
	udelay(3);

	if (rtk_misc_get_rl_version() != RTK_CUT_VERSION_A) {
		data = (struct rtk_usb_phy_cal_data_t *)rtk_usb_cut_b_cal_data;
	} else {
		data = (struct rtk_usb_phy_cal_data_t *)rtk_usb_cut_a_cal_data;
	}

	while (data->page != 0xFF) {
		if (data->page != old_page) {
			ret = rtk_phy_page_set(p, dwc, data->page);
			if (ret != 0) {
				dev_err(p->dev, "Fail to switch to page %d: %d\n", data->page, ret);
				break;
			}
			old_page = data->page;
		}
		ret = rtk_phy_write(p, dwc, data->addr, data->val);
		if (ret != 0) {
			dev_err(p->dev, "Fail to write page %d register 0x%02X: %d\n", data->page, data->addr, ret);
			break;
		}
		data++;
	}

	return ret;
}

EXPORT_SYMBOL_GPL(rtk_phy_calibrate);

static int rtk_usbphy_phy_init(struct phy *p)
{
	u32 reg;
	u32 count = 0;
	struct rtk_usb_phy_t *uphy = dev_get_priv(p->dev);
	fdt_addr_t addon_base = uphy->addon_base;
	fdt_addr_t hp_base = uphy->hp_base;
	fdt_addr_t clk_base = uphy->clk_base;

	reg = readl(clk_base + REG_LSYS_AIP_CTRL1);
	reg |= (LSYS_BIT_BG_PWR | LSYS_BIT_BG_ON_USB2);
	if (rtk_misc_get_rl_version() != RTK_CUT_VERSION_A) {
		reg &= ~LSYS_MASK_BG_ALL;
		reg |= LSYS_BG_ALL(0x2);
	}
	writel(reg, clk_base + REG_LSYS_AIP_CTRL1);

	setbits_le32(clk_base + REG_LSYS_CKE_GRP1, APBPeriph_USB_CLOCK);
	setbits_le32(clk_base + REG_LSYS_FEN_GRP1, APBPeriph_USB);

	/* USB Power Sequence */
	/* USB digital pad en,dp/dm sharing GPIO PAD */
	reg = readl(hp_base + REG_HSYS_USB_CTRL);
	reg &= ~(HSYS_BIT_USB2_DIGOTGPADEN | HSYS_BIT_USB_OTGMODE | HSYS_BIT_USB2_DIGPADEN);
#if IS_ENABLED(CONFIG_USB_DWC2_DUAL_ROLE)
	reg |= (HSYS_BIT_USB_OTGMODE | HSYS_BIT_OTG_ANA_EN);
#endif
	writel(reg, hp_base + REG_HSYS_USB_CTRL);

	/* USB PWC_UALV_EN,  PWC_UAHV_EN */
	reg = readl(hp_base + REG_HSYS_USB_CTRL);
	reg |= (HSYS_BIT_PWC_UALV_EN | HSYS_BIT_PWC_UAHV_EN);
	writel(reg, hp_base + REG_HSYS_USB_CTRL);
	udelay(2);

	/* USB PWC_UABG_EN */
	reg = readl(hp_base + REG_HSYS_USB_CTRL);
	reg |= HSYS_BIT_PWC_UABG_EN;
	writel(reg, hp_base + REG_HSYS_USB_CTRL);
	udelay(10);

	/* USB ISO_USBD_EN = 0 => disable isolation output signal from PD_USBD*/
	reg = readl(hp_base + REG_HSYS_USB_CTRL);
	reg &= ~HSYS_BIT_ISO_USBA_EN;
	writel(reg, hp_base + REG_HSYS_USB_CTRL);
	udelay(10);

	/* USBPHY_EN = 1 */
	reg = readl(addon_base + USB_OTG_ADDON_REG_CTRL);
	reg &= ~USB_OTG_ADDON_REG_CTRL_BIT_HS_IP_GAP_OPT_MASK;
	reg |= (0x3U << USB_OTG_ADDON_REG_CTRL_BIT_HS_IP_GAP_OPT_POS); // Inter-packet gap 343ns, spec 399ns
	reg |= (USB_OTG_ADDON_REG_CTRL_BIT_USB_DPHY_FEN | USB_OTG_ADDON_REG_CTRL_BIT_USB_APHY_EN | USB_OTG_ADDON_REG_CTRL_BIT_LS_HST_UTMI_EN);
	writel(reg, addon_base + USB_OTG_ADDON_REG_CTRL);
	udelay(34);

	/* Wait UPLL_CKRDY */
	do {
		/* 1ms timeout expected, 10ms for safe */
		udelay(10);
		if (++count > 1000U) {
			dev_err(p->dev, "USB PHY init timeout\n");
			return -ETIMEDOUT;
		}
	} while (!(readl(addon_base + USB_OTG_ADDON_REG_CTRL) & USB_OTG_ADDON_REG_CTRL_BIT_UPLL_CKRDY));

	/* USBOTG_EN = 1 => enable USBOTG */
	reg = readl(addon_base + USB_OTG_ADDON_REG_CTRL);
	reg |= USB_OTG_ADDON_REG_CTRL_BIT_USB_OTG_RST;
	writel(reg, addon_base + USB_OTG_ADDON_REG_CTRL);

	return 0;
}

static int rtk_usbphy_phy_exit(struct phy *p)
{
	u32 reg = 0;
	struct rtk_usb_phy_t *uphy = dev_get_priv(p->dev);
	fdt_addr_t addon_base = uphy->addon_base;
	fdt_addr_t clk_base = uphy->clk_base;
	fdt_addr_t hp_base = uphy->hp_base;

	/* USBOTG_EN = 0 => disable USBOTG */
	reg = readl(addon_base + USB_OTG_ADDON_REG_CTRL);
	reg &= (~USB_OTG_ADDON_REG_CTRL_BIT_USB_OTG_RST);
	writel(reg, addon_base + USB_OTG_ADDON_REG_CTRL);

	/* USBPHY_EN = 0 */
	reg = readl(addon_base + USB_OTG_ADDON_REG_CTRL);
	reg &= (~(USB_OTG_ADDON_REG_CTRL_BIT_USB_DPHY_FEN | USB_OTG_ADDON_REG_CTRL_BIT_USB_APHY_EN));
	writel(reg, addon_base + USB_OTG_ADDON_REG_CTRL);

	/* USB ISO_USBD_EN = 1 => enable isolation output signal from PD_USBD*/
	reg = readl(hp_base + REG_HSYS_USB_CTRL);
	reg |= HSYS_BIT_ISO_USBA_EN;
	writel(reg, hp_base + REG_HSYS_USB_CTRL);

	/* USB PWC_UABG_EN = 0 */
	reg = readl(hp_base + REG_HSYS_USB_CTRL);
	reg &= ~HSYS_BIT_PWC_UABG_EN;
	writel(reg, hp_base + REG_HSYS_USB_CTRL);

	/* PWC_UPHV_EN  = 0 => disable USBPHY analog 3.3V power */
	/* PWC_UPLV_EN = 0 => disable USBPHY analog 1.2V power */
	reg = readl(hp_base + REG_HSYS_USB_CTRL);
	reg &= ~(HSYS_BIT_PWC_UALV_EN | HSYS_BIT_PWC_UAHV_EN);
	writel(reg, hp_base + REG_HSYS_USB_CTRL);

	/* USB digital pad disable*/
	reg = readl(hp_base + REG_HSYS_USB_CTRL);
	reg |= (HSYS_BIT_USB2_DIGOTGPADEN | HSYS_BIT_USB2_DIGPADEN);
#if IS_ENABLED(CONFIG_USB_DWC2_DUAL_ROLE)
	reg &= ~(HSYS_BIT_USB_OTGMODE | HSYS_BIT_OTG_ANA_EN);
#endif
	writel(reg, hp_base + REG_HSYS_USB_CTRL);

	clrbits_le32(clk_base + REG_LSYS_CKE_GRP1, APBPeriph_USB_CLOCK);
	clrbits_le32(clk_base + REG_LSYS_FEN_GRP1, APBPeriph_USB);

	reg = readl(clk_base + REG_LSYS_AIP_CTRL1);
	reg &= ~LSYS_BIT_BG_ON_USB2;
	writel(reg, clk_base + REG_LSYS_AIP_CTRL1);

	return 0;
}

static int rtk_usbphy_of_xlate(struct phy *p, struct ofnode_phandle_args *args)
{
	return 0;
}

static int rtk_usbphy_phy_power_on(struct phy *p)
{
	return 0;
}
static int rtk_usbphy_phy_power_off(struct phy *p)
{
	return 0;
}

static int rtk_usbphy_probe(struct udevice *dev)
{
	struct rtk_usb_phy_t *uphy = dev_get_priv(dev);
	int ret = 0;

	uphy->addon_base = devfdt_get_addr_index(dev, 0);
	uphy->hp_base = devfdt_get_addr_index(dev, 1);
	uphy->clk_base = devfdt_get_addr_index(dev, 2);

	return ret;
}

static const struct phy_ops rtk_usbphy_ops = {
	.init = rtk_usbphy_phy_init,
	.exit = rtk_usbphy_phy_exit,
	.power_on = rtk_usbphy_phy_power_on,
	.power_off = rtk_usbphy_phy_power_off,
	.of_xlate = rtk_usbphy_of_xlate,
};

static const struct udevice_id rtk_usbphy_of_match[] = {
	{ .compatible = "realtek,amebad2-otg-phy", },
	{ },
};

U_BOOT_DRIVER(rtk_usb_phy_driver) = {
	.name = "realtek-amebad2-otg-phy",
	.id = UCLASS_PHY,
	.of_match = rtk_usbphy_of_match,
	.ops = &rtk_usbphy_ops,
	.probe = rtk_usbphy_probe,
	.priv_auto_alloc_size = sizeof(struct rtk_usb_phy_t),
};

