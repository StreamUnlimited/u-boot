
/*
 * board/amlogic/axg_s400_v1/axg_s400_v1.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#include <common.h>
#include <malloc.h>
#include <errno.h>
#include <environment.h>
#include <fdt_support.h>
#include <linux/libfdt.h>
#include <asm/arch/secure_apb.h>
#ifdef CONFIG_SYS_I2C_AML
#include <aml_i2c.h>
#endif
#ifdef CONFIG_AML_VPU
#include <vpu.h>
#endif
#include <vpp.h>
#ifdef CONFIG_AML_V2_FACTORY_BURN
#include <amlogic/aml_v2_burning.h>
#endif// #ifdef CONFIG_AML_V2_FACTORY_BURN
#ifdef CONFIG_AML_HDMITX20
#include <amlogic/hdmi.h>
#endif
#ifdef CONFIG_AML_LCD
#include <amlogic/aml_lcd.h>
#endif
#include <asm/arch/eth_setup.h>
#include <phy.h>
#include <asm/cpu_id.h>
#include <linux/mtd/partitions.h>
#include <linux/sizes.h>
#include <asm/arch/clock.h>
#include <asm/arch/board_id.h>

#ifdef CONFIG_AXP152_POWER
#include <axp152.h>
#endif

#ifdef CONFIG_AXP313_POWER
#include <axp313.h>
#endif

#include <asm/reboot.h>

#include "../common/fwupdate.h"
#include "../common/device_interface.h"
#include "../common/flags_a113d.h"

static struct sue_device_info __attribute__((section (".data"))) current_device;

DECLARE_GLOBAL_DATA_PTR;

//new static eth setup
struct eth_board_socket*  eth_board_skt;


int serial_set_pin_port(unsigned long port_base)
{
    //UART in "Always On Module"
    //GPIOAO_0==tx,GPIOAO_1==rx
    //setbits_le32(P_AO_RTI_PIN_MUX_REG,3<<11);
    return 0;
}

int dram_init(void)
{
	gd->ram_size = PHYS_SDRAM_1_SIZE;
	return 0;
}

/* secondary_boot_func
 * this function should be write with asm, here, is is only for compiling pass
 * */
void secondary_boot_func(void)
{
}
void internalPhyConfig(struct phy_device *phydev)
{
	/*Enable Analog and DSP register Bank access by*/
	phy_write(phydev, MDIO_DEVAD_NONE, 0x14, 0x0000);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x14, 0x0400);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x14, 0x0000);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x14, 0x0400);
	/*Write Analog register 23*/
	phy_write(phydev, MDIO_DEVAD_NONE, 0x17, 0x8E0D);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x14, 0x4417);
	/*Enable fractional PLL*/
	phy_write(phydev, MDIO_DEVAD_NONE, 0x17, 0x0005);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x14, 0x5C1B);
	//Programme fraction FR_PLL_DIV1
	phy_write(phydev, MDIO_DEVAD_NONE, 0x17, 0x029A);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x14, 0x5C1D);
	//## programme fraction FR_PLL_DiV1
	phy_write(phydev, MDIO_DEVAD_NONE, 0x17, 0xAAAA);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x14, 0x5C1C);
}

#ifdef CONFIG_CMD_NET
static void setup_net_chip(void)
{
	eth_aml_reg0_t eth_reg0;
	/* GPIOX_{8-10,12-15} function 4 for Ethernet */
	u32 val = readl(P_PERIPHS_PIN_MUX_5);
	val &= ~0xffff0fff;
	val |=  0x44440444;
	writel(val, P_PERIPHS_PIN_MUX_5);

	/* GPIOX_{21-22} function 4 for Ethernet */
	val = readl(P_PERIPHS_PIN_MUX_6);
	val &= ~0x0ff00000;
	val |=  0x04400000;
	writel(val, P_PERIPHS_PIN_MUX_6);

	eth_reg0.d32 = 0;

	eth_reg0.b.phy_intf_sel = 4; /* RMII */
	eth_reg0.b.rx_clk_rmii_invert = 0;
	eth_reg0.b.rgmii_tx_clk_src = 0;
	eth_reg0.b.rgmii_tx_clk_phase = 0;
	eth_reg0.b.rgmii_tx_clk_ratio = 0;
	eth_reg0.b.phy_ref_clk_enable = 1;
	eth_reg0.b.clk_rmii_i_invert = 1;
	eth_reg0.b.clk_en = 1;
	eth_reg0.b.adj_enable = 0;
	eth_reg0.b.adj_setup = 0;
	eth_reg0.b.adj_delay = 0;
	eth_reg0.b.adj_skew = 0;
	eth_reg0.b.cali_start = 0;
	eth_reg0.b.cali_rise = 0;
	eth_reg0.b.cali_sel = 0;
	eth_reg0.b.rgmii_rx_reuse = 0;
	eth_reg0.b.eth_urgent = 0;
	setbits_le32(P_PREG_ETH_REG0, eth_reg0.d32);// rmii mode
	setbits_le32(HHI_GCLK_MPEG1,1<<3); /* CLKID_ETH_CORE in HHI_GCLK_MPEG1 */
	/* power on memory */
	clrbits_le32(HHI_MEM_PD_REG0, (1 << 3) | (1<<2));

	udelay(1000);
	/* hardware reset ethernet phy : GPIOAO_7 connect phyreset pin*/
	clrbits_le32(P_AO_GPIO_O_EN_N, 1 << 7);

}

extern struct eth_board_socket* eth_board_setup(char *name);
extern int designware_initialize(ulong base_addr, u32 interface);

int board_eth_init(bd_t *bis)
{
	setup_net_chip();
	udelay(1000);
	designware_initialize(ETH_BASE, PHY_INTERFACE_MODE_RMII);
	return 0;
}
#endif

#if CONFIG_AML_SD_EMMC
#include <mmc.h>
#include <asm/arch/sd_emmc.h>
static int  sd_emmc_init(unsigned port)
{
    switch (port)
	{
		case SDIO_PORT_A:
			break;
		case SDIO_PORT_B:
			//todo add card detect
			//setbits_le32(P_PREG_PAD_GPIO5_EN_N,1<<29);//CARD_6
			break;
		case SDIO_PORT_C:
			//enable pull up
			//clrbits_le32(P_PAD_PULL_UP_REG3, 0xff<<0);
			break;
		default:
			break;
	}

	return cpu_sd_emmc_init(port);
}

extern unsigned sd_debug_board_1bit_flag;
static int  sd_emmc_detect(unsigned port)
{
	int ret;
    switch (port) {

	case SDIO_PORT_A:
		break;
	case SDIO_PORT_B:
			/*axg skt using GPIOX_6 as CD, no debug board anymore*/
			clrbits_le32(P_PERIPHS_PIN_MUX_4, 0xF000000);
			setbits_le32(P_PREG_PAD_GPIO2_EN_N, 1 << 6);
			ret = readl(P_PREG_PAD_GPIO2_I) & (1 << 6) ? 0 : 1;
			printf("%s\n", ret ? "card in" : "card out");
		#if 0 /* no default card on board. */
			if ((readl(P_PERIPHS_PIN_MUX_6) & (3 << 8))) { //if uart pinmux set, debug board in
				if (!(readl(P_PREG_PAD_GPIO2_I) & (1 << 24))) {
					printf("sdio debug board detected, sd card with 1bit mode\n");
					sd_debug_board_1bit_flag = 1;
				}
				else{
					printf("sdio debug board detected, no sd card in\n");
					sd_debug_board_1bit_flag = 0;
					return 1;
				}
			}
		#endif
		break;
	default:
		break;
	}
	return 0;
}

static void sd_emmc_pwr_prepare(unsigned port)
{
	cpu_sd_emmc_pwr_prepare(port);
}

static void sd_emmc_pwr_on(unsigned port)
{
    switch (port)
	{
		case SDIO_PORT_A:
			break;
		case SDIO_PORT_B:
//            clrbits_le32(P_PREG_PAD_GPIO5_O,(1<<31)); //CARD_8
//            clrbits_le32(P_PREG_PAD_GPIO5_EN_N,(1<<31));
			/// @todo NOT FINISH
			break;
		case SDIO_PORT_C:
			break;
		default:
			break;
	}
	return;
}
static void sd_emmc_pwr_off(unsigned port)
{
	/// @todo NOT FINISH
    switch (port)
	{
		case SDIO_PORT_A:
			break;
		case SDIO_PORT_B:
//            setbits_le32(P_PREG_PAD_GPIO5_O,(1<<31)); //CARD_8
//            clrbits_le32(P_PREG_PAD_GPIO5_EN_N,(1<<31));
			break;
		case SDIO_PORT_C:
			break;
				default:
			break;
	}
	return;
}

// #define CONFIG_TSD      1
static void board_mmc_register(unsigned port)
{
	struct aml_card_sd_info *aml_priv=cpu_sd_emmc_get(port);
    if (aml_priv == NULL)
		return;

	aml_priv->sd_emmc_init=sd_emmc_init;
	aml_priv->sd_emmc_detect=sd_emmc_detect;
	aml_priv->sd_emmc_pwr_off=sd_emmc_pwr_off;
	aml_priv->sd_emmc_pwr_on=sd_emmc_pwr_on;
	aml_priv->sd_emmc_pwr_prepare=sd_emmc_pwr_prepare;
	aml_priv->desc_buf = malloc(NEWSD_MAX_DESC_MUN*(sizeof(struct sd_emmc_desc_info)));

	if (NULL == aml_priv->desc_buf)
		printf(" desc_buf Dma alloc Fail!\n");
	else
		printf("aml_priv->desc_buf = 0x%p\n",aml_priv->desc_buf);

	sd_emmc_register(aml_priv);
}
int board_mmc_init(bd_t	*bis)
{
#ifdef CONFIG_VLSI_EMULATOR
	//board_mmc_register(SDIO_PORT_A);
#else
	//board_mmc_register(SDIO_PORT_B);
#endif
	board_mmc_register(SDIO_PORT_B);
	board_mmc_register(SDIO_PORT_C);
//	board_mmc_register(SDIO_PORT_B1);
	return 0;
}
#endif

#ifdef CONFIG_SYS_I2C_AML
#if 0
static void board_i2c_set_pinmux(void){
	/*********************************************/
	/*                | I2C_Master_AO        |I2C_Slave            |       */
	/*********************************************/
	/*                | I2C_SCK                | I2C_SCK_SLAVE  |      */
	/* GPIOAO_4  | [AO_PIN_MUX: 6]     | [AO_PIN_MUX: 2]   |     */
	/*********************************************/
	/*                | I2C_SDA                 | I2C_SDA_SLAVE  |     */
	/* GPIOAO_5  | [AO_PIN_MUX: 5]     | [AO_PIN_MUX: 1]   |     */
	/*********************************************/

	//disable all other pins which share with I2C_SDA_AO & I2C_SCK_AO
	clrbits_le32(P_AO_RTI_PIN_MUX_REG, ((1<<2)|(1<<24)|(1<<1)|(1<<23)));
	//enable I2C MASTER AO pins
	setbits_le32(P_AO_RTI_PIN_MUX_REG,
	(MESON_I2C_MASTER_AO_GPIOAO_4_BIT | MESON_I2C_MASTER_AO_GPIOAO_5_BIT));

	udelay(10);
};
#endif

struct aml_i2c_platform g_aml_i2c_plat = {
};
/* multi i2c bus */
struct aml_i2c_platform g_aml_i2c_ports[] = {
	{
		.wait_count         = 1000000,
		.wait_ack_interval  = 5,
		.wait_read_interval = 5,
		.wait_xfer_interval = 5,
		.master_no          = AML_I2C_MASTER_AO,
		.use_pio            = 0,
		.master_i2c_speed   = CONFIG_SYS_I2C_SPEED,
		.master_ao_pinmux = {
			.scl_reg    = (unsigned long)MESON_I2C_MASTER_AO_GPIOAO_10_REG,
			.scl_bit    = MESON_I2C_MASTER_AO_GPIOAO_10_BIT,
			.sda_reg    = (unsigned long)MESON_I2C_MASTER_AO_GPIOAO_11_REG,
			.sda_bit    = MESON_I2C_MASTER_AO_GPIOAO_11_BIT,
		}
	},

	/* sign of end */
	{.master_i2c_speed=0},
};

static void board_i2c_init(void)
{
	extern void aml_i2c_set_ports(struct aml_i2c_platform *i2c_plat);
	aml_i2c_set_ports(g_aml_i2c_ports);

	udelay(10);
}
#endif

#if defined(CONFIG_BOARD_EARLY_INIT_F)
int board_early_init_f(void){
	return 0;
}
#endif

#ifdef CONFIG_USB_XHCI_AMLOGIC_GXL
#include <asm/arch/usb-new.h>
#include <asm/arch/gpio.h>
#define CONFIG_GXL_USB_U2_PORT_NUM	4
#define CONFIG_GXL_USB_U3_PORT_NUM	0

static void gpio_set_vbus_power(char is_power_on)
{
	if (is_power_on)
		clrbits_le32(P_AO_GPIO_O_EN_N, (1<<12));
	else
		setbits_le32(P_AO_GPIO_O_EN_N, (1<<12));
}

struct amlogic_usb_config g_usb_config_GXL_skt={
	CONFIG_GXL_XHCI_BASE,
	USB_ID_MODE_HARDWARE,
	gpio_set_vbus_power,
	CONFIG_GXL_USB_PHY2_BASE,
	CONFIG_GXL_USB_PHY3_BASE,
	CONFIG_GXL_USB_U2_PORT_NUM,
	CONFIG_GXL_USB_U3_PORT_NUM,
};
#endif /*CONFIG_USB_XHCI_AMLOGIC*/

#ifdef CONFIG_AML_HDMITX20
static void hdmi_tx_set_hdmi_5v(void)
{
}
#endif

/*
 * mtd nand partition table, only care the size!
 * offset will be calculated by nand driver.
 */
#ifdef CONFIG_AML_MTD
static struct mtd_partition normal_partition_info[] = {
#ifdef CONFIG_DISCRETE_BOOTLOADER
    /* MUST NOT CHANGE this part unless u know what you are doing!
     * inherent parition for descrete bootloader to store fip
     * size is determind by TPL_SIZE_PER_COPY*TPL_COPY_NUM
     * name must be same with TPL_PART_NAME
     */
    {
        .name = "tpl",
        .offset = 0,
        .size = 0,
    },
#endif
    {
        .name = "environment",
        .offset = 0,
        .size = 512*SZ_1K,
    },
    {
        .name = "constants",
        .offset = 0,
        .size = 512*SZ_1K,
    },
    {
        .name = "swufit",
        .offset = 0,
        .size = 32*SZ_1M,
    },
    {
        .name = "fit",
        .offset = 0,
        .size = 12*SZ_1M,
    },

	/* last partition get the rest capacity */
    {
        .name = "data",
        .offset = MTDPART_OFS_APPEND,
        .size = MTDPART_SIZ_FULL,
    },
};
struct mtd_partition *get_aml_mtd_partition(void)
{
	return normal_partition_info;
}
int get_aml_partition_count(void)
{
	return ARRAY_SIZE(normal_partition_info);
}
#endif /* CONFIG_AML_MTD */
void power_save_pre(void)
{
	/*Close MIPI clock*/
	clrbits_le32(P_HHI_MIPI_CNTL0, 0xffffffff);
	clrbits_le32(P_HHI_MIPI_CNTL1, 0xffffffff);
	clrbits_le32(P_HHI_MIPI_CNTL2, 0xffdfffff);
}

#ifdef CONFIG_AXP152_POWER
int board_axp152_init(void)
{
	int ret = 0;

	/*
	 * Set the DCDC workmode of the DCDC2 (VDDCPU/VDD_ARM) to PWM only mode. The reason is
	 * that per default the board boots in 1.2 GHz mode which requires the PWM only mode.
	 * The automode leads to stability issues on 1.2 GHz and up. The workmode for lower
	 * operating points will be handled by the kernel.
	 */
	ret |= axp152_set_dcdc_workmode(AXP152_DCDC2, AXP152_DCDC_WORKMODE_PWM);

	/*
	 * Also reset DCDC2 to 1.1 V because if the CPU was reset but the AXP not, e.g. warm reset
	 * the DCDC2 voltage might be at 0.85 V from when the Linux governor set a low operating
	 * point before an external reset. This is also the default value of the AXP152 after power on.
	 */
	ret |= axp152_set_dcdc2(1100);

	/* Set VDDQ to 1.35V */
	ret |= axp152_set_dcdc3(1350);

	/* Set VDDIO_AO to 3.1V */
	ret |= axp152_set_ldo2(3300);

	/* Set VDDIO to 3.3V */
	ret |= axp152_set_aldo1(AXP152_ALDO_3V3);

	/* Set VDD_EE to 0.95V */
	ret |= axp152_set_dcdc4(950);

	/* Enable 3V3_OUT */
	ret |= axp152_set_ldo0(AXP152_LDO0_3V3, AXP152_LDO0_CUR_900MA);

	/*
	 * Always cycle WIFI_VRF otherwise the chip might be in some inconsistent state
	 *
	 * After doing the measurements with the HW department, we have to wait for 50 ms
	 * before the power rails are cleanly at 0.
	 */
	ret |= axp152_set_gpio2_low();
	udelay(50000);
	ret |= axp152_set_gpio2_ldo(3300);

	printf("AXP152 init done: %d\n", ret);

	return ret;
}
#endif

#ifdef CONFIG_AXP313_POWER
int board_axp313_init(void)
{
	int ret = 0;

	/*
	 * Set the DCDC workmode of the DCDC2 (VDDCPU/VDD_ARM) to PWM only mode. The reason is
	 * that per default the board boots in 1.2 GHz mode which requires the PWM only mode.
	 * The automode leads to stability issues on 1.2 GHz and up. The workmode for lower
	 * operating points will be handled by the kernel.
	 */
	ret |= axp313_set_dcdc_workmode(AXP313_DCDC1, AXP313_DCDC_WORKMODE_PWM);

	/*
	 * Also reset DCDC3 to 1.8 V ,This is also the default value of the AXP313 after power on.
	 */
	ret |= axp313_set_dcdc3(1800);

	/* Set DCDC1 to 1.１V */
	ret |= axp313_set_dcdc1(1100);

	/* Set VDD_EE to 0.95V */
	ret |= axp313_set_dcdc2(950);

	/* Set ALDO to 3.1V */
	ret |= axp313_set_aldo1(AXP313_ALDO_3V3);

	/* Set DLDO to 3.3V */
	ret |= axp313_set_dldo1(AXP313_ALDO_3V3);

	/* Set power off sequence 1BH bi3 to 1 first open last off */
	ret |= axp313_set_poweroff_first_open_last_off();

	/*
	 * Always cycle WIFI_VRF otherwise the chip might be in some inconsistent state
	 *
	 * After doing the measurements with the HW department, we have to wait for 50 ms
	 * before the power rails are cleanly at 0.
	 * AX313A, it use PIN_GPIOZ_10(WIFI_VREF_ON) to control WIFI_VREF
	 */
	amlogic_wifi_vrf_disable();
	udelay(50000);
	amlogic_wifi_vrf_enable();

	printf("AXP313 init done: %d\n", ret);

	return ret;
}
#endif

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

static int reset_cause(void)
{
	uint32_t reboot_mode_val;
	reboot_mode_val = ((readl(AO_SEC_SD_CFG15) >> 12) & 0xf);

	switch (reboot_mode_val)
	{
		case AMLOGIC_COLD_BOOT:
			return SUE_RESET_CAUSE_POR;
		case AMLOGIC_NORMAL_BOOT:
		case AMLOGIC_FACTORY_RESET_REBOOT:
		case AMLOGIC_UPDATE_REBOOT:
		case AMLOGIC_FASTBOOT_REBOOT:
		case AMLOGIC_BOOTLOADER_REBOOT:
		case AMLOGIC_SUSPEND_REBOOT:
		case AMLOGIC_HIBERNATE_REBOOT:
		case AMLOGIC_SHUTDOWN_REBOOT:
		case AMLOGIC_CRASH_REBOOT:
		case AMLOGIC_KERNEL_PANIC:
		case AMLOGIC_RPMBP_REBOOT:
			return SUE_RESET_CAUSE_SOFTWARE;
		case AMLOGIC_WATCHDOG_REBOOT:
			return SUE_RESET_CAUSE_WDOG;
		default:
			return SUE_RESET_CAUSE_UNKNOWN;
	}
}

int board_init(void)
{
	int ret;

	sue_device_detect(&current_device);

	sue_print_device_info(&current_device);

#ifdef CONFIG_SYS_I2C_AML
	board_i2c_init();
#endif

	if (current_device.module_version < 4) {
		/* for Stream1832 L1, L2 and L3 we first try to find the AXP152 at addr 0x32 */
		ret = axp152_init(0x32);
		if (!ret) {
			board_axp152_init();
		} else {
			/* if this fails we try to use 0x30, which was a hardware bug on older modules */
			ret = axp152_init(0x30);
			if (!ret) {
				board_axp152_init();
			} else {
				printf("Error, i2c failed to communicate with PMIC AXP152, i2c slave address 0x30 : %d\n", ret);
			}
		}
	} else {
		/* for Stream1832 L4 and higher we try to find the AXP313 only at addr 0x36 */
		ret = axp313_init(0x36);
		if (!ret) {
			board_axp313_init();
		} else {
			printf("Error, i2c failed to communicate with PMIC AXP313A,i2c slave address 0x36 : %d\n", ret);
		}
	}
#ifdef CONFIG_AML_V2_FACTORY_BURN
	aml_try_factory_usb_burning(0, gd->bd);
#endif// #ifdef CONFIG_AML_V2_FACTORY_BURN

#ifdef CONFIG_USB_XHCI_AMLOGIC_GXL
	board_usb_init(&g_usb_config_GXL_skt,BOARD_USB_MODE_HOST);
#endif /*CONFIG_USB_XHCI_AMLOGIC*/

#ifdef CONFIG_AML_NAND
	extern int amlnf_init(unsigned char flag);
	amlnf_init(0);
#endif

	current_device.reset_cause = reset_cause();
	handle_reset_cause(current_device.reset_cause);

	if (get_cpu_id().package_id == MESON_CPU_PACKAGE_ID_A113X)
		power_save_pre();

	sue_carrier_ops_init(&current_device);
	sue_carrier_init(&current_device);

	return 0;
}

#ifdef CONFIG_BOARD_LATE_INIT
extern int is_dtb_encrypt(unsigned char *buffer);
int board_late_init(void){
	/*add board late init function here*/
	int ret;
	ret = run_command("store dtb read $dtb_mem_addr", 1);
	if (ret) {
		printf("%s(): [store dtb read $dtb_mem_addr] fail\n", __func__);
		#ifdef CONFIG_DTB_MEM_ADDR
		char cmd[64];
		printf("load dtb to %x\n", CONFIG_DTB_MEM_ADDR);
		sprintf(cmd, "store dtb read %x", CONFIG_DTB_MEM_ADDR);
		ret = run_command(cmd, 1);
		if (ret) {
			printf("%s(): %s fail\n", __func__, cmd);
		}
		#endif
	}

	/* load unifykey */
	//run_command("keyunify init 0x1234", 0);

#ifdef CONFIG_AML_VPU
	vpu_probe();
#endif
	//vpp_init();
#ifdef CONFIG_AML_HDMITX20
	hdmi_tx_set_hdmi_5v();
	hdmi_tx_init();
#endif
#ifdef CONFIG_AML_LCD
	lcd_probe();
#endif

#ifdef CONFIG_AML_V2_FACTORY_BURN
	/*aml_try_factory_sdcard_burning(0, gd->bd);*/
#endif// #ifdef CONFIG_AML_V2_FACTORY_BURN

	char buffer[64];

	if (fwupdate_init(&current_device) < 0) {
		printf("ERROR: fwupdate_init() call failed!\n");
	}

	if (current_device.carrier_flags & SUE_CARRIER_FLAGS_HAS_DAUGHTER) {
		snprintf(buffer, sizeof(buffer), "%s_l%d_%s_%s",
				sue_device_get_canonical_module_name(&current_device),
				current_device.module_version,
				sue_device_get_canonical_carrier_name(&current_device),
				sue_device_get_canonical_daughter_name(&current_device));
	} else {
		snprintf(buffer, sizeof(buffer), "%s_l%d_%s",
				sue_device_get_canonical_module_name(&current_device),
				current_device.module_version,
				sue_device_get_canonical_carrier_name(&current_device));
	}
	printf("Setting fit_config: %s\n", buffer);
	setenv("fit_config", buffer);

	// pass board 'secure' state (ie, locked secure fuses/..) to env
	const char *secure = is_dtb_encrypt(NULL) ? "1" : "0";
	printf("Setting secure_board: %s\n", secure);
	setenv("secure_board", secure);

	sue_carrier_late_init(&current_device);

	return 0;
}
#endif

phys_size_t get_effective_memsize(void)
{
	// >>16 -> MB, <<20 -> real size, so >>16<<20 = <<4
#if defined(CONFIG_SYS_MEM_TOP_HIDE)
	return (((readl(AO_SEC_GP_CFG0)) & 0xFFFF0000) << 4) - CONFIG_SYS_MEM_TOP_HIDE;
#else
	return (((readl(AO_SEC_GP_CFG0)) & 0xFFFF0000) << 4);
#endif
}

#ifdef CONFIG_MULTI_DTB
int checkhw(char * name)
{
	unsigned int ddr_size = 0;
	int i;

	for (i=0; i<CONFIG_NR_DRAM_BANKS; i++) {
		ddr_size += gd->bd->bi_dram[i].size;
	}

#if defined(CONFIG_SYS_MEM_TOP_HIDE)
	ddr_size += CONFIG_SYS_MEM_TOP_HIDE;
#endif
	printf("debug ddr size: 0x%x\n", ddr_size);

	sue_print_device_info(&current_device);

	strcpy(name, "axg");

	return 0;
}
#endif

#if defined(CONFIG_OF_LIBFDT) && defined(CONFIG_OF_BOARD_SETUP)
int ft_board_setup(void *blob, bd_t *bd)
{
	int node;
	int ret;
	fdt32_t tmp[4] = { 0x0, 0x0, 0x0, 0x0 };
	uint32_t ddr_size = get_effective_memsize();

	/*
	 * This patches the `linux,usable-memory` property in the device-tree,
	 * with this the kernel is able to boot on 2Gb as well as on 4Gb modules.
	 */
	node = fdt_path_offset(blob, "/memory@00000000");
	if (node < 0) {
		printf("WARN: Could not find the memory node %d\n", node);
		return -ENOENT;
	}

	tmp[3] = cpu_to_fdt32(ddr_size);

	ret = fdt_setprop(blob, node, "linux,usable-memory", &tmp, sizeof(tmp));
	if (ret < 0) {
		printf("WARN: Could not set `linux,usable-memory` property %d\n", ret);
		return ret;
	}

	return 0;
}
#endif /* defined(CONFIG_OF_LIBFDT) && defined(CONFIG_OF_BOARD_SETUP) */

const char * const _env_args_reserve_[] =
{
		"aml_dt",
		"firstboot",

		NULL//Keep NULL be last to tell END
};
