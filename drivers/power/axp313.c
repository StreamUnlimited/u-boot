/*
 * AXP313A PMIC u-boot driver.
 *
 * (C) Copyright 2022 StreamUnlimited
 * Author: Wei Chen <wei.chen@streamunlimited.com>
 *
 * Functions and registers to access AXP313A power management chip. *
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <i2c.h>
#include <axp313.h>
#include <asm-generic/gpio.h>
#include <asm/arch/gpio.h>

#define CONFIG_WIFI_VEER_GPIO  PIN_GPIOZ_10
#define CONFIG_WIFI_VEER_GPIO_NAME "GPIOZ_10"

enum axp313_reg {
        AXP313A_POWER_ON_SOURCE_INDIVATION			 = 0x00,
        AXP313A_POWER_OFF_SOURCE_INDIVATION			 = 0x01,
        AXP313A_VERSION						 = 0x03,
        AXP313A_OUTPUT_POWER_ON_OFF_CTL				 = 0x10,
        AXP313A_DCDC_DVM_PWM_CTL				 = 0x12,
        AXP313A_DC1OUT_VOL					 = 0x13,
        AXP313A_DC2OUT_VOL          				 = 0x14,
        AXP313A_DC3OUT_VOL          				 = 0x15,
        AXP313A_ALDO1OUT_VOL					 = 0x16,
        AXP313A_DLDO1OUT_VOL					 = 0x17,
        AXP313A_POWER_DOMN_SEQUENCE				 = 0x1A,
        AXP313A_PWROK_VOFF_SERT					 = 0x1B,
        AXP313A_POWER_WAKEUP_CTL				 = 0x1C,
        AXP313A_OUTPUT_MONITOR_CONTROL				 = 0x1D,
        AXP313A_POK_SET						 = 0x1E,
        AXP313A_IRQ_ENABLE					 = 0x20,
        AXP313A_IRQ_STATUS					 = 0x21,
};

#define AXP313_POWEROFF			(1 << 7)

static u8 axp313_addr;

static int axp313_write(enum axp313_reg reg, u8 val)
{
	int ret;
	ret = i2c_write(axp313_addr, reg, 1, &val, 1);
	if (ret)
		printf("axp313_write failed, ret= %d\n", ret);
#ifdef DEBUG
	else
		printf("axp313_write reg=0x%x , val=0x%x\n", reg,(unsigned char)(val & 0xFF));
#endif
	return ret;
}

static int axp313_read(enum axp313_reg reg, u8 *val)
{
	int ret;
	ret = i2c_read(axp313_addr, reg, 1, val, 1);
	if (ret)
		printf("axp313_read failed, ret= %d\n", ret);
#ifdef DEBUG
	else
		printf("axp313_read reg=0x%x , val=0x%x\n", reg,(unsigned char)(*val & 0xFF));
#endif
	return ret;
}

static u8 axp313_mvolt_to_target(int mvolt, int min, int max, int div)
{
	if (mvolt < min)
		mvolt = min;
	else if (mvolt > max)
		mvolt = max;

	return (mvolt - min) / div;
}

int axp313_set_aldo1(enum axp313_aldo_voltage aldo_val)
{
	int ret;
	u8 current;

	ret = axp313_read(AXP313A_ALDO1OUT_VOL, &current);
	if (ret)
		return ret;
	current &= 0xC0;
	current |= aldo_val & 0x1F;
	return axp313_write(AXP313A_ALDO1OUT_VOL, current);
}

int axp313_set_dldo1(enum axp313_aldo_voltage dldo_val)
{
	int ret;
	u8 current, power_ctl;

	ret = axp313_read(AXP313A_OUTPUT_POWER_ON_OFF_CTL, &power_ctl);
	if (ret)
		return ret;
	power_ctl |= (1 << 4);
	ret = axp313_write(AXP313A_OUTPUT_POWER_ON_OFF_CTL, power_ctl);
	if (ret)
		return ret;

	ret = axp313_read(AXP313A_DLDO1OUT_VOL, &current);
	if (ret)
		return ret;
	current &= 0xC0;
	current |= dldo_val & 0x1F;
	return axp313_write(AXP313A_DLDO1OUT_VOL, current);
}

int axp313_set_dcdc1(int mvolt)
{
	u8 target = axp313_mvolt_to_target(mvolt, 500, 1200, 10);

	return axp313_write(AXP313A_DC1OUT_VOL, target);
}

int axp313_set_dcdc2(int mvolt)
{
	int rc;
	u8 current, target;

	target = axp313_mvolt_to_target(mvolt, 500, 1200, 10);

	/* Do we really need to be this gentle? It has built-in voltage slope */
	while ((rc = axp313_read(AXP313A_DC2OUT_VOL, &current)) == 0 &&
	       current != target) {
		if (current < target)
			current++;
		else
			current--;
		rc = axp313_write(AXP313A_DC2OUT_VOL, current);
		if (rc)
			break;
	}
	return rc;
}

int axp313_set_dcdc3(int mvolt)
{
	u8 target = axp313_mvolt_to_target(mvolt, 1220, 1840, 20);
	// DCDC3 register AXP313A_DC3OUT_VOL 0x15 ,1.22V, value 0x1D
	target += 0x47;
	return axp313_write(AXP313A_DC3OUT_VOL, target);
}

int axp313_set_dcdc_workmode(enum axp313_dcdc_regulator id, int mode)
{
	u8 reg;
	int ret;

	if (id < AXP313_DCDC1 || id > AXP313_DCDC3)
		return -EINVAL;

	ret = axp313_read(AXP313A_DCDC_DVM_PWM_CTL, &reg);
	if (ret)
		return ret;

	reg = reg & ~(1 << (AXP313_DCDC3 - id));
	reg |= mode << (AXP313_DCDC3 - id);

	return axp313_write(AXP313A_DCDC_DVM_PWM_CTL, reg);
}

int axp313_init(unsigned char i2c_addr)
{
	u8 ver;
	int rc;

	axp313_addr = i2c_addr;

	rc = axp313_read(AXP313A_VERSION, &ver);
	if (rc)
		return rc;

	if (ver != 0x4B)
		return -1;

	return 0;
}

int axp313_set_poweroff_first_open_last_off(void)
{
	u8 reg;
	int ret;

	ret = axp313_read(AXP313A_PWROK_VOFF_SERT, &reg);
	if (ret)
		return ret;
	reg |= (1 << 3);//set power off sequence=1 0x1B bit 3
	reg |= (1 << 2);//set power off regulaters delay=1 0x1B bit 2
	return axp313_write(AXP313A_PWROK_VOFF_SERT, reg);
}

/* axp313_set_offlevel, there is one input parameter long_offlevel.
 * if the long_offlevel is true, then the OFFLEVEL of PMIC AXP313A will be set to
 * 10s, otherwise if long_offlevel is false, the OFFLEVEL of PMIC AXP313A will be
 * set to 6s, the OFFLEVEL can only be set to 6s or 10s according to the specification
 * of AXP313A. It is controlled by the reg 0x1E bit 1, the default value is 6s.
 */
int axp313_set_offlevel(bool long_offlevel)
{
	u8 reg;
	int ret = 0;
	ret = axp313_read(AXP313A_POK_SET, &reg);
	if (ret)
		return ret;

	if (long_offlevel)
		reg |= (1 << 1); //set OFFLEVEL=10s reg 0x1E bit 1 = 1
	else
		reg &= ~(1 << 1); //set OFFLEVEL=6s reg 0x1E bit 1 = 0

	ret = axp313_write(AXP313A_POK_SET, reg);
	if (ret)
		return ret;

	return 0;
}

void amlogic_wifi_vrf_enable(void)
{
	int ret;

	ret = gpio_request(CONFIG_WIFI_VEER_GPIO,CONFIG_WIFI_VEER_GPIO_NAME);

	if (ret && ret != -EBUSY) {
		printf("gpio: requesting pin %u failed\n",
			CONFIG_WIFI_VEER_GPIO);
		return;
	}
	gpio_direction_output(CONFIG_WIFI_VEER_GPIO, 1);
}

void amlogic_wifi_vrf_disable(void)
{
	int ret;

	ret = gpio_request(CONFIG_WIFI_VEER_GPIO,CONFIG_WIFI_VEER_GPIO_NAME);
	if (ret && ret != -EBUSY) {
		printf("gpio: requesting pin %u failed\n",
			CONFIG_WIFI_VEER_GPIO);
		return;
	}
	gpio_direction_output(CONFIG_WIFI_VEER_GPIO, 0);
}
