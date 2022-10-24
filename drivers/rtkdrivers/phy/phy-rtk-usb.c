// SPDX-License-Identifier: GPL-2.0+

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


/* USB PHY registers */
#define USB_OTG_PHY_REG_E0								0xE0U
#define USB_OTG_PHY_REG_E1								0xE1U
#define USB_OTG_PHY_REG_E2								0xE2U
#define USB_OTG_PHY_REG_E4								0xE4U
#define USB_OTG_PHY_REG_E6								0xE6U
#define USB_OTG_PHY_REG_E7								0xE7U
#define USB_OTG_PHY_REG_F1								0xF1U
#define USB_OTG_PHY_REG_F4								0xF4U
#define USB_OTG_PHY_REG_F6								0xF6U

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
#define USB_OTG_ADDON_REG_CTRL							0UL // 0x30004UL
#define USB_OTG_ADDON_REG_VND_STS_OUT					0x18//0x3001CUL

#define USB_OTG_ADDON_REG_CTRL_BIT_UPLL_CKRDY			BIT(5)  /* 1: USB PHY clock ready */
#define USB_OTG_ADDON_REG_CTRL_BIT_USBOTG_EN			BIT(8)  /* 1: Enable USB OTG */
#define USB_OTG_ADDON_REG_CTRL_BIT_USBPHY_EN			BIT(9)  /* 1: Enable USB APHY & DPHY */
#define USB_OTG_ADDON_REG_CTRL_BIT_PORETB_TOP			BIT(14) /* 1: Enable USB APHY */

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
#define USB_OTG_GPVNDCTL							0x0034

#define USB_OTG_GPVNDCTL_REGDATA_Pos             (0U)
#define USB_OTG_GPVNDCTL_REGDATA_Msk             (0xFFUL << USB_OTG_GPVNDCTL_REGDATA_Pos) /*!< 0x000000FF */
#define USB_OTG_GPVNDCTL_REGDATA                 USB_OTG_GPVNDCTL_REGDATA_Msk /*!< Register Data */
#define USB_OTG_GPVNDCTL_VCTRL_Pos               (8U)
#define USB_OTG_GPVNDCTL_VCTRL_Msk               (0xFFUL << USB_OTG_GPVNDCTL_VCTRL_Pos) /*!< 0x0000FF00 */
#define USB_OTG_GPVNDCTL_VCTRL                   USB_OTG_GPVNDCTL_VCTRL_Msk /*!< UTMI+ Vendor Control Register Address */
#define USB_OTG_GPVNDCTL_VSTSDONE_Pos            (27U)
#define USB_OTG_GPVNDCTL_VSTSDONE_Msk            (0x1UL << USB_OTG_GPVNDCTL_VSTSDONE_Pos) /*!< 0x02000000 */
#define USB_OTG_GPVNDCTL_VSTSDONE                USB_OTG_GPVNDCTL_VSTSDONE_Msk /*!< VStatus Done */

/* USB PHY defines */
#define PHY_HIGH_ADDR(x)			(((x) & 0xF0) >> 4)
#define PHY_LOW_ADDR(x)				((x) & 0x0F)
#define PHY_DATA_MASK				0xFF

#define usleep_range(a, b) udelay((b))

struct rtk_usbphy {
	struct phy phy;
	fdt_addr_t clk_base;
	fdt_addr_t base;
	fdt_addr_t otg_base;
};


static int rtk_usbphy_phy_init(struct phy *phy)
{
	u32 reg;
	u32 count = 0;
	struct rtk_usbphy *usbphyc = dev_get_priv(phy->dev);
	fdt_addr_t otg_base = usbphyc->otg_base;
	fdt_addr_t base = usbphyc->base;

	/* USB Power Sequence */
	/* USB digital pad en,dp/dm sharing GPIO PAD */
	reg = readl(base + REG_HSYS_USB_CTRL);
	reg &= ~(HSYS_BIT_USB2_DIGOTGPADEN | HSYS_BIT_USB_OTGMODE | HSYS_BIT_USB2_DIGPADEN);
#if IS_ENABLED(CONFIG_USB_DWC2_DUAL_ROLE)
	reg |= (HSYS_BIT_USB_OTGMODE | HSYS_BIT_OTG_ANA_EN);
#endif
	writel(reg, base + REG_HSYS_USB_CTRL);

	/* USB PWC_UALV_EN,  PWC_UAHV_EN */
	reg = readl(base + REG_HSYS_USB_CTRL);
	reg |= (HSYS_BIT_PWC_UALV_EN | HSYS_BIT_PWC_UAHV_EN);
	writel(reg, base + REG_HSYS_USB_CTRL);
	usleep_range(2, 3);

	/* USB PWC_UABG_EN */
	reg = readl(base + REG_HSYS_USB_CTRL);
	reg |= HSYS_BIT_PWC_UABG_EN;
	writel(reg, base + REG_HSYS_USB_CTRL);
	usleep_range(10, 15);

	/* USB ISO_USBD_EN = 0 => disable isolation output signal from PD_USBD*/
	reg = readl(base + REG_HSYS_USB_CTRL);
	reg &= ~HSYS_BIT_ISO_USBA_EN;
	writel(reg, base + REG_HSYS_USB_CTRL);
	usleep_range(10, 15);

	/* USBPHY_EN = 1 */
	reg = readl(otg_base + USB_OTG_ADDON_REG_CTRL);
	reg |= (USB_OTG_ADDON_REG_CTRL_BIT_USBPHY_EN | USB_OTG_ADDON_REG_CTRL_BIT_PORETB_TOP);
	writel(reg, otg_base + USB_OTG_ADDON_REG_CTRL);
	usleep_range(34, 40);

	/* Wait UPLL_CKRDY */
	do {
		/* 1ms timeout expected, 10ms for safe */
		usleep_range(10, 15);
		if (++count > 1000U) {
			debug("USB PHY init timeout\n");
			return -ETIMEDOUT;
		}
	} while (!(readl(otg_base + USB_OTG_ADDON_REG_CTRL) & USB_OTG_ADDON_REG_CTRL_BIT_UPLL_CKRDY));

	/* USBOTG_EN = 1 => enable USBOTG */
	reg = readl(otg_base + USB_OTG_ADDON_REG_CTRL);
	reg |= USB_OTG_ADDON_REG_CTRL_BIT_USBOTG_EN;
	writel(reg, otg_base + USB_OTG_ADDON_REG_CTRL);

	return 0;
}

static int rtk_usbphy_phy_exit(struct phy *phy)
{
	u32 reg = 0;
	struct rtk_usbphy *usbphyc = dev_get_priv(phy->dev);
	fdt_addr_t otg_base = usbphyc->otg_base;
	fdt_addr_t base = usbphyc->base;

	/* USBOTG_EN = 0 => disable USBOTG */
	reg = readl(otg_base + USB_OTG_ADDON_REG_CTRL);
	reg &= (~USB_OTG_ADDON_REG_CTRL_BIT_USBOTG_EN);
	writel(reg, otg_base + USB_OTG_ADDON_REG_CTRL);

	/* USBPHY_EN = 0 */
	reg = readl(otg_base + USB_OTG_ADDON_REG_CTRL);
	reg &= (~(USB_OTG_ADDON_REG_CTRL_BIT_USBPHY_EN | USB_OTG_ADDON_REG_CTRL_BIT_PORETB_TOP));
	writel(reg, otg_base + USB_OTG_ADDON_REG_CTRL);

	/* USB ISO_USBD_EN = 1 => enable isolation output signal from PD_USBD*/
	reg = readl(base + REG_HSYS_USB_CTRL);
	reg |= HSYS_BIT_ISO_USBA_EN;
	writel(reg, base + REG_HSYS_USB_CTRL);

	/* USB PWC_UABG_EN = 0 */
	reg = readl(base + REG_HSYS_USB_CTRL);
	reg &= ~HSYS_BIT_PWC_UABG_EN;
	writel(reg, base + REG_HSYS_USB_CTRL);

	/* PWC_UPHV_EN  = 0 => disable USBPHY analog 3.3V power */
	/* PWC_UPLV_EN = 0 => disable USBPHY analog 1.2V power */
	reg = readl(base + REG_HSYS_USB_CTRL);
	reg &= ~(HSYS_BIT_PWC_UALV_EN | HSYS_BIT_PWC_UAHV_EN);
	writel(reg, base + REG_HSYS_USB_CTRL);

	/* USB digital pad disable*/
	reg = readl(base + REG_HSYS_USB_CTRL);
	reg |= (HSYS_BIT_USB2_DIGOTGPADEN | HSYS_BIT_USB2_DIGPADEN);
#if IS_ENABLED(CONFIG_USB_DWC2_DUAL_ROLE)
	reg &= ~(HSYS_BIT_USB_OTGMODE | HSYS_BIT_OTG_ANA_EN);
#endif
	writel(reg, base + REG_HSYS_USB_CTRL);

	/* Disable clock (REG_LSYS_CKE_GRP1)*/
	clrbits_le32(usbphyc->clk_base, BIT(10));
	/* Disable function (REG_LSYS_FEN_GRP1)*/
	clrbits_le32(usbphyc->clk_base - 0xC, BIT(12));

	return 0;
}

static int rtk_usbphy_of_xlate(struct phy *phy,
				  struct ofnode_phandle_args *args)
{
	return 0;
}

static int rtk_usbphy_phy_power_on(struct phy *phy)
{
	return 0;
}
static int rtk_usbphy_phy_power_off(struct phy *phy)
{
	return 0;
}


static int rtk_usbphy_probe(struct udevice *dev)
{
	struct rtk_usbphy *usbphyc = dev_get_priv(dev);
	int ret = 0;

	usbphyc->otg_base = devfdt_get_addr_index(dev, 0);
	usbphyc->base = devfdt_get_addr_index(dev, 1);
	usbphyc->clk_base = devfdt_get_addr_index(dev, 2);

	/* Enable clock (REG_LSYS_CKE_GRP1)*/
	setbits_le32(usbphyc->clk_base + 0x18, BIT(10));
	/* Enable function (REG_LSYS_FEN_GRP1)*/
	setbits_le32(usbphyc->clk_base + 0xC, BIT(12));

	return ret;
}




static const struct phy_ops rtk_usbphy_phy_ops = {
	.init = rtk_usbphy_phy_init,
	.exit = rtk_usbphy_phy_exit,
	.power_on = rtk_usbphy_phy_power_on,
	.power_off = rtk_usbphy_phy_power_off,
	.of_xlate = rtk_usbphy_of_xlate,
};



static const struct udevice_id rtk_usbphy_of_match[] = {
	{ .compatible = "realtek,otg-phy", },
	{ },
};

U_BOOT_DRIVER(rtk_usb_phyc) = {
	.name = "rtk-usbphy",
	.id = UCLASS_PHY,
	.of_match = rtk_usbphy_of_match,
	.ops = &rtk_usbphy_phy_ops,
	.probe = rtk_usbphy_probe,
	.priv_auto_alloc_size = sizeof(struct rtk_usbphy),
};

