// SPDX-License-Identifier: GPL-2.0-or-later

#include <common.h>
#include <config.h>
#include <command.h>
#include <asm/io.h>
#include <asm/global_data.h>
#include <cpu_func.h>
#include <sue_secureboot.h>
#include <linux/compat.h>
#include <mtd.h>
#include <usb.h>

#include "realtek/bspchip.h"

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

void reset_cpu(ulong addr)
{
}

bool is_sue_secureboot(void)
{
	u32 reg = 0;

	// Check if logical secureboot fuse is set
	reg = REG32(OTPC_REG_BASE + SEC_OTP_SYSCFG0);
	if (reg & SEC_BIT_LOGIC_SECURE_BOOT_EN) {
		return true;
	}

	// Check if physical secureboot fuse is set.
	// NOTE: It is set by being cleared.
	reg = REG32(OTPC_REG_BASE + SEC_CFG2);
	if (!(reg & SEC_BIT_SECURE_BOOT_EN)) {
		return true;
	}

	return false;
}

int board_init(void)
{
	return 0;
}

#ifndef CONFIG_SYS_DCACHE_OFF
void enable_caches(void)
{
	icache_enable();
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
	int rl_ver;

	rl_ver = rtk_misc_get_rl_version();
	printf("SoC Version: %d\n", rl_ver);

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

#if defined(CONFIG_BOARD_EARLY_INIT_R)
int board_early_init_r(void)
{
	return 0;
}
#endif



#define GPIO_B_DR_REG	0x4200D400
#define GPIO_B_DDR_REG	0x4200D404

int board_usb_init(int index, enum usb_init_type init)
{
	// There is no Realtek GPIO driver in the U-Boot,
	// so we just set PB28 manually to high or low depending
	// on the USB mode we want by writing to the DR and DDR
	// register accordingly.

	if (init == USB_INIT_DEVICE) {
		printf("Setting USB to device mode\n");
		clrsetbits_le32(GPIO_B_DR_REG, BIT(28), 0);
		clrsetbits_le32(GPIO_B_DDR_REG, BIT(28), BIT(28));
	} else {
		printf("Setting USB to host mode\n");
		clrsetbits_le32(GPIO_B_DR_REG, BIT(28), BIT(28));
		clrsetbits_le32(GPIO_B_DDR_REG, BIT(28), BIT(28));
	}

	return 0;
}



// Returns true if we are booted in the factory and should
// enable fastboot. This is done by checking if the first 4
// bytes in the boot_b NAND partition contain the magic.
#define FACTORY_FLAG_MTD_PART "boot_b"
#define FACTORY_FLAG_MAGIC 0x91199119
#define FACTORY_FLAG_COMMAND "fastboot usb 0"

static bool check_boot_factory(void)
{
	int ret;
	uint32_t magic;
	struct mtd_info *mtd;
	struct mtd_oob_ops io_op = {};

	mtd_probe_devices();
	mtd = get_mtd_device_nm(FACTORY_FLAG_MTD_PART);
	if (IS_ERR(mtd) || !mtd) {
		printf("MTD device '%s' not found!\n", FACTORY_FLAG_MTD_PART);
		return false;
	}

	io_op.mode = MTD_OPS_AUTO_OOB;
	io_op.len = 4;
	io_op.ooblen = 0;
	io_op.datbuf = (u8 *)&magic;
	io_op.oobbuf = NULL;

	ret = mtd_read_oob(mtd, 0, &io_op);
	if (ret)
		return false;

	return magic == FACTORY_FLAG_MAGIC;
}

int board_late_init(void)
{
	// Currently hardcoded to stream210_l0, later on
	// we might do module detection if really needed
	env_set("module_config", "stream210_l0");

	if (is_sue_secureboot()) {
		env_set_ulong("secure_board", 1);

		// On a locked module it makes not sense to have a boot
		// delay so set it to zero to save 2 seconds of boot time.
		env_set_ulong("bootdelay", 0);
	} else {
		env_set_ulong("secure_board", 0);
	}


	if (check_boot_factory() && !is_sue_secureboot()) {
		// Set the USB port to device mode as we will most likely run
		// fastboot.
		board_usb_init(0, USB_INIT_DEVICE);

		printf("Factory magic flag set, starting factory command: %s\n", FACTORY_FLAG_COMMAND);
		run_command(FACTORY_FLAG_COMMAND, 0);
	}

	// Set the USB port (back) to host mode. If we were in fastboot mode then most
	// of the time we don't arrive here under normal circumstances. We only arrive
	// here if the fastboot was aborted (e.g. by Ctrl-C), so in that case switch
	// back to host mode.
	board_usb_init(0, USB_INIT_HOST);

	return 0;
}

