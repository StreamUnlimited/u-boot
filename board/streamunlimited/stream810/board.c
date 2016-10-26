/*
 * Copyright (C) 2015-2016 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/arch/clock.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/mx7-pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>
#include <asm/imx-common/iomux-v3.h>
#include <asm/imx-common/boot_mode.h>
#include <asm/io.h>
#include <linux/sizes.h>
#include <linux/mtd/mtd.h>
#include <common.h>
#include <miiphy.h>
#include <netdev.h>
#include <i2c.h>
#include <asm/imx-common/mxc_i2c.h>
#include <axp152.h>
#include <asm/arch/imx-regs.h>

#include "../common/fwupdate.h"
#include "../common/partitions.h"
#include "../common/device_interface.h"
#include "../common/flags_mx7.h"

static struct sue_device_info __attribute__((section (".data"))) current_device;

DECLARE_GLOBAL_DATA_PTR;

#define WIFI_PDN_GPIO		IMX_GPIO_NR(5, 2)
#define PHY_NRESET_GPIO		IMX_GPIO_NR(4, 22)

#define UART_PAD_CTRL		(PAD_CTL_DSE_3P3V_49OHM | PAD_CTL_PUS_PU100KOHM | PAD_CTL_HYS)
#define ENET_PAD_CTRL		(PAD_CTL_PUS_PU100KOHM | PAD_CTL_DSE_3P3V_49OHM)
#define ENET_RX_PAD_CTRL	(PAD_CTL_PUS_PU100KOHM | PAD_CTL_DSE_3P3V_49OHM)
#define I2C_PAD_CTRL		(PAD_CTL_DSE_3P3V_32OHM | PAD_CTL_SRE_SLOW | \
				 PAD_CTL_HYS | PAD_CTL_PUE | PAD_CTL_PUS_PU100KOHM)
#define NAND_PAD_CTRL		(PAD_CTL_DSE_3P3V_49OHM | PAD_CTL_SRE_SLOW | PAD_CTL_HYS)
#define NAND_PAD_READY0_CTRL	(PAD_CTL_DSE_3P3V_49OHM | PAD_CTL_PUS_PU5KOHM)

#ifdef CONFIG_SYS_I2C_MXC
/* I2C4 */
struct i2c_pads_info i2c_pad_info4 = {
	.scl = {
		.i2c_mode = MX7D_PAD_ENET1_RGMII_TD2__I2C4_SCL | MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gpio_mode = MX7D_PAD_ENET1_RGMII_TD2__GPIO7_IO8 | MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gp = IMX_GPIO_NR(7, 8),
	},
	.sda = {
		.i2c_mode = MX7D_PAD_ENET1_RGMII_TD3__I2C4_SDA | MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gpio_mode = MX7D_PAD_ENET1_RGMII_TD3__GPIO7_IO9 | MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gp = IMX_GPIO_NR(7, 9),
	},
};
#endif

static const iomux_v3_cfg_t const wifi_pads[] = {
	/* WLAN_PDN */
	MX7D_PAD_SD1_RESET_B__GPIO5_IO2 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

int dram_init(void)
{
#ifdef CONFIG_ARMV7_TEE
	gd->ram_size = PHYS_SDRAM_SIZE - CONFIG_TEE_RAM_SIZE;
#else
	gd->ram_size = PHYS_SDRAM_SIZE;
#endif

	return 0;
}

#ifdef CONFIG_SYS_USE_NAND
static iomux_v3_cfg_t const gpmi_pads[] = {
	MX7D_PAD_SD3_DATA0__NAND_DATA00 	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	MX7D_PAD_SD3_DATA1__NAND_DATA01 	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	MX7D_PAD_SD3_DATA2__NAND_DATA02 	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	MX7D_PAD_SD3_DATA3__NAND_DATA03 	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	MX7D_PAD_SD3_DATA4__NAND_DATA04 	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	MX7D_PAD_SD3_DATA5__NAND_DATA05 	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	MX7D_PAD_SD3_DATA6__NAND_DATA06 	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	MX7D_PAD_SD3_DATA7__NAND_DATA07 	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	MX7D_PAD_SD3_CLK__NAND_CLE		| MUX_PAD_CTRL(NAND_PAD_CTRL),
	MX7D_PAD_SD3_CMD__NAND_ALE		| MUX_PAD_CTRL(NAND_PAD_CTRL),
	MX7D_PAD_SD3_STROBE__NAND_RE_B		| MUX_PAD_CTRL(NAND_PAD_CTRL),
	MX7D_PAD_SD3_RESET_B__NAND_WE_B		| MUX_PAD_CTRL(NAND_PAD_CTRL),
	MX7D_PAD_SAI1_MCLK__NAND_WP_B		| MUX_PAD_CTRL(NAND_PAD_CTRL),
	MX7D_PAD_SAI1_RX_BCLK__NAND_CE3_B	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	MX7D_PAD_SAI1_RX_SYNC__NAND_CE2_B	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	MX7D_PAD_SAI1_RX_DATA__NAND_CE1_B	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	MX7D_PAD_SAI1_TX_BCLK__NAND_CE0_B	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	MX7D_PAD_SAI1_TX_SYNC__NAND_DQS		| MUX_PAD_CTRL(NAND_PAD_CTRL),
	MX7D_PAD_SAI1_TX_DATA__NAND_READY_B	| MUX_PAD_CTRL(NAND_PAD_READY0_CTRL),
};

static void setup_gpmi_nand(void)
{
	imx_iomux_v3_setup_multiple_pads(gpmi_pads, ARRAY_SIZE(gpmi_pads));

	set_clk_nand();
}
#endif

static iomux_v3_cfg_t const uart1_pads[] = {
	MX7D_PAD_UART1_TX_DATA__UART1_DCE_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX7D_PAD_UART1_RX_DATA__UART1_DCE_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

static void setup_iomux_uart(void)
{
	imx_iomux_v3_setup_multiple_pads(uart1_pads, ARRAY_SIZE(uart1_pads));
}

#ifdef CONFIG_FEC_MXC
/* Currently incompleted since on the demo client fec1 is not used */
static const iomux_v3_cfg_t fec1_pads_s810[] = {
	MX7D_PAD_ENET1_TX_CLK__CCM_ENET_REF_CLK1 | MUX_PAD_CTRL(ENET_PAD_CTRL) | MUX_MODE_SION,
	MX7D_PAD_LCD_DATA15__GPIO3_IO20	| MUX_PAD_CTRL(NO_PAD_CTRL),
};

static const iomux_v3_cfg_t fec2_pads_s810[] = {
	MX7D_PAD_GPIO1_IO14__ENET2_MDIO | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX7D_PAD_GPIO1_IO15__ENET2_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL),

	MX7D_PAD_EPDC_GDRL__ENET2_RGMII_TX_CTL | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX7D_PAD_EPDC_SDCE2__ENET2_RGMII_TD0 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX7D_PAD_EPDC_SDCE3__ENET2_RGMII_TD1 | MUX_PAD_CTRL(ENET_PAD_CTRL),

	MX7D_PAD_EPDC_SDCE0__ENET2_RGMII_RX_CTL | MUX_PAD_CTRL(ENET_RX_PAD_CTRL),
	MX7D_PAD_EPDC_SDCLK__ENET2_RGMII_RD0 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL),
	MX7D_PAD_EPDC_SDLE__ENET2_RGMII_RD1 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL),
	MX7D_PAD_EPDC_SDCE1__ENET2_RX_ER | MUX_PAD_CTRL(ENET_RX_PAD_CTRL),

	MX7D_PAD_EPDC_BDR0__CCM_ENET_REF_CLK2 | MUX_PAD_CTRL(ENET_PAD_CTRL) | MUX_MODE_SION,

	/* NRES_ETH pin */
	MX7D_PAD_ECSPI2_MISO__GPIO4_IO22 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static void setup_iomux_fec(void)
{
	imx_iomux_v3_setup_multiple_pads(fec1_pads_s810, ARRAY_SIZE(fec1_pads_s810));
	imx_iomux_v3_setup_multiple_pads(fec2_pads_s810, ARRAY_SIZE(fec2_pads_s810));
}

int board_eth_init(bd_t *bis)
{
	int ret;

	setup_iomux_fec();

	ret = fecmxc_initialize_multi(bis, CONFIG_FEC_ENET_DEV, CONFIG_FEC_MXC_PHYADDR, IMX_FEC_BASE);
	if (ret)
		printf("%s: fecmxc_initialize_multi failed\n", __func__);

	return ret;
}

int board_phy_config(struct phy_device *phydev)
{
	/* Reset the PHY */
	gpio_direction_output(PHY_NRESET_GPIO, 0);
	mdelay(5);
	gpio_set_value(PHY_NRESET_GPIO, 1);

	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}

static int setup_fec(int fec_id)
{
	struct iomuxc_gpr_base_regs *const iomuxc_gpr_regs = (struct iomuxc_gpr_base_regs *)IOMUXC_GPR_BASE_ADDR;
	int ret;

	if (fec_id == 0) {
		/* FEC1 */
		clrsetbits_le32(&iomuxc_gpr_regs->gpr[1],
				(IOMUXC_GPR_GPR1_GPR_ENET1_TX_CLK_SEL_MASK | IOMUXC_GPR_GPR1_GPR_ENET1_CLK_DIR_MASK),
				(IOMUXC_GPR_GPR1_GPR_ENET1_CLK_DIR_MASK));
	} else if (fec_id == 1) {
		/* FEC2 */
		clrsetbits_le32(&iomuxc_gpr_regs->gpr[1],
				(IOMUXC_GPR_GPR1_GPR_ENET2_TX_CLK_SEL_MASK | IOMUXC_GPR_GPR1_GPR_ENET2_CLK_DIR_MASK),
				(IOMUXC_GPR_GPR1_GPR_ENET2_CLK_DIR_MASK));
	} else {
		return -EINVAL;
	}

	ret = set_clk_enet(ENET_50MHz);
	if (ret) {
		printf("%s: set_clk_enet() failed\n", __func__);
		return ret;
	}

	return 0;
}
#endif

int board_early_init_f(void)
{
	sue_device_detect(&current_device);

	setup_iomux_uart();

#ifdef CONFIG_SYS_I2C_MXC
	setup_i2c(3, CONFIG_SYS_I2C_SPEED, 0x7f, &i2c_pad_info4);
#endif
	return 0;
}

static enum sue_reset_cause get_reset_cause(void)
{
	u8 reg, flag;

	i2c_read(AXP152_ADDR, AXP152_GPIO0, 1, &reg, 1);
	if (reg & (1 << 2)) {
		/*
		 * If the bit is set we have a cold boot since, the
		 * bit is set per default on the AXP152. So we clear the bit
		 * and return a cold boot
		 */
		reg &= ~(1 << 2);
		i2c_write(AXP152_ADDR, AXP152_GPIO0, 1, &reg, 1);
		return SUE_RESET_CAUSE_POR;
	} else {
		/*
		 * Otherwise it was either a watchdog reset or a software reset.
		 * If it was a software reset, then before the actual reset, the
		 * kernel or U-Boot has set the `FWUP_FLAG_SWRESET_REQ` flag,
		 * so we can differentiate between those two.
		 */
		flag_read(FWUP_FLAG_SWRESET_REQ, &flag);

		if (flag) {
			flag_write(FWUP_FLAG_SWRESET_REQ, 0);
			return SUE_RESET_CAUSE_SOFTWARE;
		} else {
			return SUE_RESET_CAUSE_WDOG;
		}
	}
	return SUE_RESET_CAUSE_UNKNOWN;
}

/*
 * Before we do the reset we need to set a flag in the SNVS registers
 * so we can differentiate between a reset requested by the user/system
 * or a reset done by the HW watchdog.
 */
void reset_misc(void)
{
	flag_write(FWUP_FLAG_SWRESET_REQ, 1);
}

/*
 * This function prints the reset cause an does some thing like
 * clearing all flags if the reset cause was POR.
 */
static int handle_reset_cause(enum sue_reset_cause reset_cause)
{
	printf("Reset cause: ");
	switch(current_device.reset_cause) {
		case SUE_RESET_CAUSE_POR:
			flags_clear();
			bootcnt_write(0);
			printf("POR\n");
			break;
		case SUE_RESET_CAUSE_SOFTWARE:
			printf("SOFTWARE\n");
			break;
		case SUE_RESET_CAUSE_WDOG:
			printf("WDOG\n");
			break;
		default:
			printf("unknown\n");
			return -EINVAL;
			break;
	};

	return 0;
}

int board_init(void)
{
	u32 reg;

	/* address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM + 0x100;

	/*
	 * Disable SNVS zeroization, otherwise the flags in the SNVS_LPGPR register will not be written
	 * because we are probably getting some tamper events.
	 */
	reg = readl(SNVS_BASE_ADDR + SNVS_LPCR);
	reg |= (1 << 24);
	writel(reg, SNVS_BASE_ADDR + SNVS_LPCR);

	current_device.reset_cause = get_reset_cause();
	handle_reset_cause(current_device.reset_cause);

#ifdef CONFIG_FEC_MXC
	setup_fec(CONFIG_FEC_ENET_DEV);
#endif

#ifdef CONFIG_SYS_USE_NAND
	setup_gpmi_nand();
#endif

	/* We want to keep the WiFi in power down until the kernel puts it out of it. */
	imx_iomux_v3_setup_multiple_pads(wifi_pads, ARRAY_SIZE(wifi_pads));
	gpio_direction_output(WIFI_PDN_GPIO, 0);

	sue_carrier_ops_init(&current_device);
	sue_carrier_init(&current_device);

	return 0;
}

#ifdef CONFIG_POWER
int power_init_board(void)
{
	i2c_set_bus_num(3);

	axp152_init();
	axp152_set_ldo0(AXP152_LDO0_3V3, AXP152_LDO0_CURR_1500MA);
	axp152_set_aldo2(AXP152_ALDO_3V3);
	axp152_set_dcdc3(1350);
	axp152_set_power_output(0xFF);

	return 0;
}
#endif

static const iomux_v3_cfg_t wdog_pads[] = {
	MX7D_PAD_GPIO1_IO00__WDOG1_WDOG_B | MUX_PAD_CTRL(NO_PAD_CTRL),
};

int board_late_init(void)
{
	if (fwupdate_init(&current_device) < 0) {
		printf("ERROR: fwupdate_init() call failed!\n");
	}

	sue_setup_mtdparts();

	imx_iomux_v3_setup_multiple_pads(wdog_pads, ARRAY_SIZE(wdog_pads));
	set_wdog_reset((struct wdog_regs *)WDOG1_BASE_ADDR);

	sue_carrier_late_init(&current_device);

	return 0;
}

int checkboard(void)
{
	sue_print_device_info(&current_device);
	return 0;
}

#ifdef CONFIG_USB_EHCI_MX7
iomux_v3_cfg_t const usb_otg1_pads[] = {
	MX7D_PAD_GPIO1_IO05__USB_OTG1_PWR | MUX_PAD_CTRL(NO_PAD_CTRL),
};

iomux_v3_cfg_t const usb_otg2_pads[] = {
	MX7D_PAD_GPIO1_IO07__USB_OTG2_PWR | MUX_PAD_CTRL(NO_PAD_CTRL),
};

int board_ehci_hcd_init(int port)
{
	switch (port) {
	case 0:
		imx_iomux_v3_setup_multiple_pads(usb_otg1_pads,
						 ARRAY_SIZE(usb_otg1_pads));
		break;
	case 1:
		imx_iomux_v3_setup_multiple_pads(usb_otg2_pads,
						 ARRAY_SIZE(usb_otg2_pads));
		break;
	default:
		printf("MXC USB port %d not yet supported\n", port);
		return 1;
	}
	return 0;
}
#endif
