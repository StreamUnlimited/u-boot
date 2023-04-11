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
#define REG_LSYS_AIP_CTRL1                              0UL // 0x025C
#define LSYS_BIT_BG_PWR                                 ((u32)0x00000001 << 8)          /* 1: power on ddrphy bandgap 0: shutdown bg */
#define LSYS_BIT_BG_ON_USB2                             ((u32)0x00000001 << 5)          /* 1: Bandgap USB2 current enable */
#define LSYS_MASK_BG_ALL                                ((u32)0x00000003 << 0)          /* 0x3  Bandgap enable mode */
#define LSYS_BG_ALL(x)                                  ((u32)(((x) & 0x00000003) << 0))
#define LSYS_GET_BG_ALL(x)                              ((u32)(((x >> 0) & 0x00000003)))

#define usleep_range(a, b)								udelay((b))

struct rtk_usbphy {
	struct phy phy;
	fdt_addr_t clk_base;
	fdt_addr_t hp_base;
	fdt_addr_t addon_base;
};

static int rtk_load_vcontrol(uintptr_t dwc, u8 addr)
{
	u32 pvndctl = 0x0A300000;
	u32 count = 0;

	pvndctl |= (PHY_LOW_ADDR(addr) << USB_OTG_GPVNDCTL_VCTRL_Pos);
	writel(pvndctl, dwc + USB_OTG_GPVNDCTL);
	do {
		/* 1us timeout expected, 1ms for safe */
		usleep_range(1, 2);
		if (++count > 1000) {
			pr_err("[USBPHY] Vcontrol timeout\n");
			return -ETIMEDOUT;
		}

		pvndctl = readl(dwc + USB_OTG_GPVNDCTL);
	} while ((pvndctl & USB_OTG_GPVNDCTL_VSTSDONE) == 0);

	return 0;
}

static int rtk_phy_write(struct phy *phy, uintptr_t dwc, u8 addr, u8 val)
{
	u32 tmp;
	int ret = 0;
	struct rtk_usbphy *usbphyc = dev_get_priv(phy->dev);
	fdt_addr_t addon_base = usbphyc->addon_base;

	tmp = readl(addon_base + USB_OTG_ADDON_REG_VND_STS_OUT);
	tmp &= (~PHY_DATA_MASK);
	tmp |= val;
	writel(tmp, addon_base + USB_OTG_ADDON_REG_VND_STS_OUT);

	//low addr
	ret = rtk_load_vcontrol(dwc, PHY_LOW_ADDR(addr));
	if (ret == 0) {
		//high addr
		ret = rtk_load_vcontrol(dwc, PHY_HIGH_ADDR(addr));
	}

	return ret;
}

static int rtk_phy_read(struct phy *phy, uintptr_t dwc, u8 addr, u8 *val)
{
	u32 pvndctl;
	int ret = 0;
	u8 addr_read;
	if (addr >= 0xE0) {
		addr_read = addr - 0x20;
	} else {
		addr_read = addr;
	}

	//low addr
	ret = rtk_load_vcontrol(dwc, PHY_LOW_ADDR(addr_read));
	if (ret == 0) {
		//high addr
		ret = rtk_load_vcontrol(dwc, PHY_HIGH_ADDR(addr_read));
		if (ret == 0) {
			pvndctl = readl(dwc + USB_OTG_GPVNDCTL);
			*val = (pvndctl & USB_OTG_GPVNDCTL_REGDATA_Msk) >> USB_OTG_GPVNDCTL_REGDATA_Pos;
		}
	}

	return ret;
}

static int rtk_phy_page_set(struct phy *phy, uintptr_t dwc, u8 page)
{
	int ret;
	u8 reg;

	ret = rtk_phy_read(phy, dwc, USB_OTG_PHY_REG_F4, &reg);
	if (ret != 0) {
		return ret;
	}

	reg &= (~USB_OTG_PHY_REG_F4_BIT_PAGE_SEL_MASK);
	reg |= (page << USB_OTG_PHY_REG_F4_BIT_PAGE_SEL_POS) & USB_OTG_PHY_REG_F4_BIT_PAGE_SEL_MASK;
	ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_F4, reg);

	return ret;
}

/**
  * @brief  USB PHY calibration
  *			Shall be called after soft disconnect
  * @param  None
  * @retval HAL status
  */
int rtk_phy_calibrate(struct phy *phy, uintptr_t dwc)
{
	int ret = 0;

	if (phy == NULL) {
		return -1;
	}

	/* 3ms + 2.5us from DD, 3ms already delayed after soft disconnect */
	usleep_range(3, 4);

	if (rtk_misc_get_rl_version() == RTK_CUT_VERSION_A) {
		/* Switch to page 1 */
		ret = rtk_phy_page_set(phy, dwc, USB_OTG_PHY_REG_F4_BIT_PAGE1);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to select page 1: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_E5, 0x0A);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P1_E5: %d\n", ret);
			return ret;
		}

		/* Switch to page 0 */
		ret = rtk_phy_page_set(phy, dwc, USB_OTG_PHY_REG_F4_BIT_PAGE0);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to select page 0: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_E1, 0x19);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P0_E1: %d\n", ret);
			return ret;
		}

		/* Switch to page 1 */
		ret = rtk_phy_page_set(phy, dwc, USB_OTG_PHY_REG_F4_BIT_PAGE1);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to select page 1: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_E6, 0xD8);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P1_E6: %d\n", ret);
			return ret;
		}

		/* Switch to page 0 */
		ret = rtk_phy_page_set(phy, dwc, USB_OTG_PHY_REG_F4_BIT_PAGE0);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to select page 0: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_E0, 0x9D);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P0_E0: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_E2, 0xDB);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P0_E2: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_E4, 0x6D);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P0_E4: %d\n", ret);
			return ret;
		}

		/* Switch to page 2 */
		ret = rtk_phy_page_set(phy, dwc, USB_OTG_PHY_REG_F4_BIT_PAGE2);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to select page 2: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_E7, 0x62);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P2_E7: %d\n", ret);
			return ret;
		}

		/* Switch to page 1 */
		ret = rtk_phy_page_set(phy, dwc, USB_OTG_PHY_REG_F4_BIT_PAGE1);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to select page 1: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_E0, 0x04);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P1_E0: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_E0, 0x00);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P1_E0: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_E0, 0x04);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P1_E0: %d\n", ret);
			return ret;
		}
	} else {
		/* Switch to page 1 */
		ret = rtk_phy_page_set(phy, dwc, USB_OTG_PHY_REG_F4_BIT_PAGE1);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to select page 1: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_E5, 0x0A);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P1_E5: %d\n", ret);
			return ret;
		}

		/* Switch to page 0 */
		ret = rtk_phy_page_set(phy, dwc, USB_OTG_PHY_REG_F4_BIT_PAGE0);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to select page 0: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_E1, 0x19);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P0_E1: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_F7, 0x3A);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P0_F7: %d\n", ret);
			return ret;
		}

		/* Switch to page 1 */
		ret = rtk_phy_page_set(phy, dwc, USB_OTG_PHY_REG_F4_BIT_PAGE1);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to select page 1: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_E6, 0xD8);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P1_E6: %d\n", ret);
			return ret;
		}

		/* Switch to page 0 */
		ret = rtk_phy_page_set(phy, dwc, USB_OTG_PHY_REG_F4_BIT_PAGE0);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to select page 0: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_E0, 0x9D);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P0_E0: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_E2, 0xDB);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P0_E2: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_E4, 0x86);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P0_E4: %d\n", ret);
			return ret;
		}

		/* Switch to page 2 */
		ret = rtk_phy_page_set(phy, dwc, USB_OTG_PHY_REG_F4_BIT_PAGE2);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to select page 2: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_E7, 0x62);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P2_E7: %d\n", ret);
			return ret;
		}

		/* Switch to page 1 */
		ret = rtk_phy_page_set(phy, dwc, USB_OTG_PHY_REG_F4_BIT_PAGE1);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to select page 1: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_E0, 0x04);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P1_E0: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_E0, 0x00);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P1_E0: %d\n", ret);
			return ret;
		}

		ret = rtk_phy_write(phy, dwc, USB_OTG_PHY_REG_E0, 0x04);
		if (ret != 0) {
			pr_err("[USBPHY] Fail to write USB_OTG_PHY_REG_P1_E0: %d\n", ret);
			return ret;
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(rtk_phy_calibrate);

static int rtk_usbphy_phy_init(struct phy *phy)
{
	u32 reg;
	u32 count = 0;
	struct rtk_usbphy *usbphyc = dev_get_priv(phy->dev);
	fdt_addr_t addon_base = usbphyc->addon_base;
	fdt_addr_t hp_base = usbphyc->hp_base;
	fdt_addr_t clk_base = usbphyc->clk_base;

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
	usleep_range(2, 3);

	/* USB PWC_UABG_EN */
	reg = readl(hp_base + REG_HSYS_USB_CTRL);
	reg |= HSYS_BIT_PWC_UABG_EN;
	writel(reg, hp_base + REG_HSYS_USB_CTRL);
	usleep_range(10, 15);

	/* USB ISO_USBD_EN = 0 => disable isolation output signal from PD_USBD*/
	reg = readl(hp_base + REG_HSYS_USB_CTRL);
	reg &= ~HSYS_BIT_ISO_USBA_EN;
	writel(reg, hp_base + REG_HSYS_USB_CTRL);
	usleep_range(10, 15);

	/* USBPHY_EN = 1 */
	reg = readl(addon_base + USB_OTG_ADDON_REG_CTRL);
	reg &= ~USB_OTG_ADDON_REG_CTRL_BIT_HS_IP_GAP_OPT_MASK;
	reg |= (0x3U << USB_OTG_ADDON_REG_CTRL_BIT_HS_IP_GAP_OPT_POS); // Inter-packet gap 343ns, spec 399ns
	reg |= (USB_OTG_ADDON_REG_CTRL_BIT_USB_DPHY_FEN | USB_OTG_ADDON_REG_CTRL_BIT_USB_APHY_EN | USB_OTG_ADDON_REG_CTRL_BIT_LS_HST_UTMI_EN);
	writel(reg, addon_base + USB_OTG_ADDON_REG_CTRL);
	usleep_range(34, 40);

	/* Wait UPLL_CKRDY */
	do {
		/* 1ms timeout expected, 10ms for safe */
		usleep_range(10, 15);
		if (++count > 1000U) {
			pr_err("[USBPHY] USB PHY init timeout\n");
			return -ETIMEDOUT;
		}
	} while (!(readl(addon_base + USB_OTG_ADDON_REG_CTRL) & USB_OTG_ADDON_REG_CTRL_BIT_UPLL_CKRDY));

	/* USBOTG_EN = 1 => enable USBOTG */
	reg = readl(addon_base + USB_OTG_ADDON_REG_CTRL);
	reg |= USB_OTG_ADDON_REG_CTRL_BIT_USB_OTG_RST;
	writel(reg, addon_base + USB_OTG_ADDON_REG_CTRL);

	return 0;
}

static int rtk_usbphy_phy_exit(struct phy *phy)
{
	u32 reg = 0;
	struct rtk_usbphy *usbphyc = dev_get_priv(phy->dev);
	fdt_addr_t addon_base = usbphyc->addon_base;
	fdt_addr_t clk_base = usbphyc->clk_base;
	fdt_addr_t hp_base = usbphyc->hp_base;

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

	usbphyc->addon_base = devfdt_get_addr_index(dev, 0);
	usbphyc->hp_base = devfdt_get_addr_index(dev, 1);
	usbphyc->clk_base = devfdt_get_addr_index(dev, 2);

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

