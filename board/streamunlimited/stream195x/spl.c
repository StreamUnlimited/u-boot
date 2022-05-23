/*
 * (C) Copyright 2019 StreamUnlimited Engineering GmbH
 * Martin Pietryka <martin.pietryka@streamunlimited.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <spl.h>
#include <asm/io.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/arch/imx8mm_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/clock.h>
#include <asm/mach-imx/gpio.h>
#include <asm/arch/imx8m_ddr.h>
#include <asm-generic/gpio.h>
#ifdef CONFIG_TARGET_STREAM195X_EMMC
#include <asm/mach-imx/mxc_i2c.h>
#include <fsl_esdhc.h>
#include <i2c.h>
#include <mmc.h>
#endif
#include <usb.h>

#include "spl_anti_rollback.h"

DECLARE_GLOBAL_DATA_PTR;

#define UART_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL1 | PAD_CTL_PE | PAD_CTL_PUE)
static iomux_v3_cfg_t const uart_pads[] = {
	IMX8MM_PAD_UART1_RXD_UART1_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	IMX8MM_PAD_UART1_TXD_UART1_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

#ifdef CONFIG_TARGET_STREAM195X_EMMC
#define I2C_PAD_CTRL   (PAD_CTL_DSE6 | PAD_CTL_HYS | PAD_CTL_PUE | PAD_CTL_PE)
#define PC MUX_PAD_CTRL(I2C_PAD_CTRL)
struct i2c_pads_info i2c_pad_info1 = {
	.scl = {
		.i2c_mode = IMX8MM_PAD_I2C1_SCL_I2C1_SCL | PC,
		.gpio_mode = IMX8MM_PAD_I2C1_SCL_GPIO5_IO14 | PC,
		.gp = IMX_GPIO_NR(5, 14),
	},
	.sda = {
		.i2c_mode = IMX8MM_PAD_I2C1_SDA_I2C1_SDA | PC,
		.gpio_mode = IMX8MM_PAD_I2C1_SDA_GPIO5_IO15 | PC,
		.gp = IMX_GPIO_NR(5, 15),
	},
};

#define USDHC_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_HYS | PAD_CTL_PUE |PAD_CTL_PE | \
			 PAD_CTL_FSEL2)
#define USDHC_GPIO_PAD_CTRL (PAD_CTL_HYS | PAD_CTL_DSE1)

static iomux_v3_cfg_t const usdhc3_pads[] = {
	IMX8MM_PAD_NAND_WE_B_USDHC3_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_WP_B_USDHC3_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA04_USDHC3_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA05_USDHC3_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA06_USDHC3_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA07_USDHC3_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_RE_B_USDHC3_DATA4 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_CE2_B_USDHC3_DATA5 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_CE3_B_USDHC3_DATA6 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_CLE_USDHC3_DATA7 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
};

static struct fsl_esdhc_cfg usdhc_cfg = {USDHC3_BASE_ADDR, 0, 1,};

int board_mmc_init(bd_t *bis)
{
	int ret;

	usdhc_cfg.sdhc_clk = mxc_get_clock(MXC_ESDHC3_CLK);
	imx_iomux_v3_setup_multiple_pads(
			usdhc3_pads, ARRAY_SIZE(usdhc3_pads));

	ret = fsl_esdhc_initialize(bis, &usdhc_cfg);
	return ret;

}

int board_mmc_getcd(struct mmc *mmc)
{
// no cd, return 1
	return 1;
}
#endif

#ifdef CONFIG_NAND_MXS
#define NAND_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL2 | PAD_CTL_HYS)
#define NAND_PAD_READY0_CTRL (PAD_CTL_DSE6 | PAD_CTL_FSEL2 | PAD_CTL_PUE)
static iomux_v3_cfg_t const gpmi_pads[] = {
	IMX8MM_PAD_NAND_ALE_RAWNAND_ALE | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_CE0_B_RAWNAND_CE0_B | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_CLE_RAWNAND_CLE | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA00_RAWNAND_DATA00 | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA01_RAWNAND_DATA01 | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA02_RAWNAND_DATA02 | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA03_RAWNAND_DATA03 | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA04_RAWNAND_DATA04 | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA05_RAWNAND_DATA05	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA06_RAWNAND_DATA06	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA07_RAWNAND_DATA07	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DQS_RAWNAND_DQS | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_RE_B_RAWNAND_RE_B | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_READY_B_RAWNAND_READY_B | MUX_PAD_CTRL(NAND_PAD_READY0_CTRL),
	IMX8MM_PAD_NAND_WE_B_RAWNAND_WE_B | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_WP_B_RAWNAND_WP_B | MUX_PAD_CTRL(NAND_PAD_CTRL),
};
#endif

#ifdef CONFIG_TARGET_STREAM195X_NAND
#define MODULE_ID0_GPIO IMX_GPIO_NR(3, 2)
static iomux_v3_cfg_t const module_id0_pads[] = {
	IMX8MM_PAD_NAND_CE1_B_GPIO3_IO2 | MUX_PAD_CTRL(NO_PAD_CTRL),
};
#else
#define MODULE_ID0_GPIO IMX_GPIO_NR(3, 6)
static iomux_v3_cfg_t const module_id0_pads[] = {
	IMX8MM_PAD_NAND_DATA00_GPIO3_IO6 | MUX_PAD_CTRL(NO_PAD_CTRL),
};
#endif

extern struct dram_timing_info ddr3l_1x4Gb_dram_timing;
extern struct dram_timing_info ddr3l_2x2Gb_dram_timing;
extern struct dram_timing_info ddr4_1x4Gb_timing;
extern struct dram_timing_info ddr4_1x8Gb_timing;

void spl_dram_init(void)
{
	imx_iomux_v3_setup_multiple_pads(module_id0_pads, ARRAY_SIZE(module_id0_pads));
	gpio_request(MODULE_ID0_GPIO, "module_id0");
	gpio_direction_input(MODULE_ID0_GPIO);

#ifdef CONFIG_TARGET_STREAM195X_NAND
	if (gpio_get_value(MODULE_ID0_GPIO)) {
		printf("calling ddr_init() with ddr3l_1x4Gb_dram_timing\n");
		ddr_init(&ddr3l_1x4Gb_dram_timing);
	} else {
		printf("calling ddr_init() with ddr3l_2x2Gb_dram_timing\n");
		ddr_init(&ddr3l_2x2Gb_dram_timing);
	}
#else
	if (gpio_get_value(MODULE_ID0_GPIO)) {
		printf("calling ddr_init() with ddr4_1x8Gb_timing\n");
		ddr_init(&ddr4_1x8Gb_timing);
	} else {
		printf("calling ddr_init() with ddr4_1x4Gb_timing\n");
		ddr_init(&ddr4_1x4Gb_timing);
	}
#endif
}

void spl_board_init(void)
{
	/* Currently does nothing */
}

#ifdef CONFIG_SPL_LOAD_FIT
int board_fit_config_name_match(const char *name)
{
	/*
	 * Since we do not care about mutliple U-Boots inside
	 * the fitImage, we just always return a match (0).
	 */
	return 0;
}
#endif

int board_usb_phy_mode(int port)
{
	/*
	 * When this function is called in the SPL we are booting
	 * over USB so the U-Boot will be loaded over USB as well,
	 * thus we return that we are in device mode in any case.
	 */
	return USB_INIT_DEVICE;
}

void board_boot_order(u32 *spl_boot_list)
{
#ifdef CONFIG_TARGET_STREAM195X_NAND
	spl_boot_list[0] = spl_boot_device();
#else
	/*
	 * The device returned by spl_boot_device() is correct only if usdhc2
	 * (SD card reader) is probed. In that case, usdhc2 is mmc dev0 and
	 * usdhc3 is mmc dev 1, in which case BOOT_DEVICE_MMC2 is correct.
	 * If usdhc2 is not probed, usdhc3 is mmc dev 0, in which case
	 * BOOT_DEVICE_MMC1 is the correct value for booting over eMMC.
	 */
	u32 bootdev = spl_boot_device();
	if (bootdev == BOOT_DEVICE_MMC2) {
		spl_boot_list[0] = BOOT_DEVICE_MMC1;
	} else {
		spl_boot_list[0] = bootdev;
	}
#endif
}

void board_init_f(ulong dummy)
{
	int ret;

	/* Clear global data */
	memset((void *)gd, 0, sizeof(gd_t));

	arch_cpu_init();

#ifdef CONFIG_NAND_MXS
	imx_iomux_v3_setup_multiple_pads(gpmi_pads, ARRAY_SIZE(gpmi_pads));
	init_nand_clk();
#endif

	timer_init();

	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));
	preloader_console_init();

	ret = spl_init();
	if (ret) {
		debug("spl_init() failed: %d\n", ret);
		hang();
	}

	enable_tzc380();

#ifdef CONFIG_TARGET_STREAM195X_EMMC
	setup_i2c(0, CONFIG_SYS_I2C_SPEED, 0x7f, &i2c_pad_info1);
	i2c_set_bus_num(0);
	{
		/* Enabling CLDO2 at 2.6V for the RAM to work */
		uint8_t val = 0x13;
		i2c_write(0x36, 0x2A, 1, &val, 1);

		i2c_read(0x36, 0x12, 1, &val, 1);
		val |= (1 << 3);
		i2c_write(0x36, 0x12, 1, &val, 1);
	}
	mdelay(10);
#endif

	/* DDR initialization */
	spl_dram_init();

	/* Anti-rollback protection */
	spl_anti_rollback_check();
}
