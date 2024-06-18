// SPDX-License-Identifier: GPL-2.0+
/*
* Realtek Ameba board support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <common.h>
#include <config.h>
#include <command.h>
#include <asm/io.h>
#include <asm/global_data.h>
#include <netdev.h>
#include <dm.h>
#include <cpu_func.h>
#include <linux/compat.h>

#include "bspchip.h"

DECLARE_GLOBAL_DATA_PTR;

int rtk_misc_get_rl_version(void)
{
	u32 value;
	u32 value32;
	u32 reg = SYSTEM_CTRL_BASE_LP + REG_LSYS_SCAN_CTRL;

	/* Set LSYS_CHIP_INFO_EN register to get ChipInfo */
	value = REG32(reg);
	value &= ~(LSYS_MASK_CHIP_INFO_EN);
	value |= LSYS_CHIP_INFO_EN(0xA);
	REG32(reg) = value;

	/* Clear LSYS_CHIP_INFO_EN register */
	value32 = REG32(reg);
	value &= ~(LSYS_MASK_CHIP_INFO_EN);
	REG32(reg) = value;

	return (int)(LSYS_GET_RL_VER(value32));
}

EXPORT_SYMBOL(rtk_misc_get_rl_version);

void reset_misc(void)
{
	writel(0U, SYSTEM_CTRL_BASE_LP + REG_AON_SYSRST_MSK);
	writel(0U, SYSTEM_CTRL_BASE_LP + REG_LSYS_SYSRST_MSK0);
	writel(0U, SYSTEM_CTRL_BASE_LP + REG_LSYS_SYSRST_MSK1);
	writel(0U, SYSTEM_CTRL_BASE_LP + REG_LSYS_SYSRST_MSK2);

	writel(SYS_RESET_KEY, SYSTEM_CTRL_BASE_LP + REG_LSYS_SW_RST_TRIG);
	writel(LSYS_BIT_LPSYS_RST << AP_CPU_ID, SYSTEM_CTRL_BASE_LP + REG_LSYS_SW_RST_CTRL);
	writel(SYS_RESET_TRIG, SYSTEM_CTRL_BASE_LP + REG_LSYS_SW_RST_TRIG);
}

int syscfg_get_secure_enable(void)
{
	u32 value0 = REG32(OTPC_REG_BASE + SEC_OTP_SYSCFG0);
	u32 value1 = REG32(OTPC_REG_BASE + SEC_CFG2);

	if ((value0 & SEC_BIT_LOGIC_SECURE_BOOT_EN) || (!(value1 & SEC_BIT_SECURE_BOOT_EN))) {
		return 1;
	} else {
		return 0;
	}
}

static int syscfg_get_otp_boot_select(void)
{
	u32 reg = REG32(OTPC_REG_BASE + SEC_OTP_SYSCFG0);
	return LSYS_GET_BOOT_SELECT(reg);
}

static int syscfg_get_trp_boot_sel(void)
{
	u32 reg = REG32(SYSTEM_CTRL_BASE_LP + REG_LSYS_SYSTEM_CFG0);
	return LSYS_GET_PTRP_BOOTSEL(reg);
}

static int syscfg_is_boot_from_nor(void)
{
	int ret;
	u32 reg;

	/*Boot Select decided by Trap pin*/
	reg = syscfg_get_otp_boot_select();
	if (reg == SYSCFG_OTP_BOOTNOR) {
		ret = 1;
	} else if (reg == SYSCFG_OTP_BOOTNAND) {
		ret = 0;
	} else {
		/*Boot Select decided by Trap pin*/
		reg = syscfg_get_trp_boot_sel();
		if (reg != 0U) {
			ret = 1;
		} else {
			ret = 0;
		}
	}

	return ret;
}

void reset_cpu(ulong addr)
{
}


int board_init(void)
{
	return 0;
}

#ifndef CONFIG_SYS_DCACHE_OFF
void enable_caches(void)
{
	dcache_enable();
}
#endif

int dram_init(void)
{
	return fdtdec_setup_mem_size_base();
}

int dram_init_banksize(void)
{
	return fdtdec_setup_memory_banksize();
}

int checkboard(void)
{
	int is_boot_from_nor;
	int rl_ver;

	rl_ver = rtk_misc_get_rl_version();
	printf("SoC Version: %d\n", rl_ver);

	is_boot_from_nor = syscfg_is_boot_from_nor();
	if (is_boot_from_nor) {
#ifdef CONFIG_MTD_SPI_NAND
		printf("\nERROR: Wrong images type, expecting images for NOR, but current images are for NAND!\n");
		while (1);
#endif
	} else {
#ifdef CONFIG_DM_SPI_FLASH
		printf("\nERROR: Wrong images type, expecting images for NAND, but current images are for NOR!\n");
		while (1);
#endif
	}

	return 0;
}

#if defined(CONFIG_DISPLAY_CPUINFO)
/*
 * Print CPU information
 */
int print_cpuinfo(void)
{
	printf("CPU: ARM Cortex-A\n");

	return 0;
}
#endif

#if defined(CONFIG_DM_SPI_FLASH)
int init_dm_spi_flash_ameba(void)
{
	struct udevice *dev;
	const char flash_name[] = "flash@0";

	uclass_first_device(UCLASS_SPI_FLASH, &dev);
	if (!dev) {
		pr_debug("Ameba flash init failed\n");
		return 1;
	}

	while (strcmp(dev->name, flash_name) != 0) {
		uclass_next_device(&dev);
		if (!dev) {
			pr_debug("Ameba flash init failed\n");
			return 1;
		}
	}

	return 0;
}
#endif

#if defined(CONFIG_BOARD_EARLY_INIT_R)
/*
 * init spi flash
 */
int board_early_init_r(void)
{
#if defined(CONFIG_DM_SPI_FLASH)
	return init_dm_spi_flash_ameba();
#endif
	return 0;
}
#endif
