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


enum axp313_aldo_voltage {
	AXP313_ALDO_0V5	= 0x00,
	AXP313_ALDO_0V6	= 0x01,
	AXP313_ALDO_0V7	= 0x02,
	AXP313_ALDO_0V8	= 0x03,
	AXP313_ALDO_0V9	= 0x04,
	AXP313_ALDO_1V0	= 0x05,
	AXP313_ALDO_1V1	= 0x06,
	AXP313_ALDO_1V2	= 0x07,
	AXP313_ALDO_1V3	= 0x08,
	AXP313_ALDO_1V4	= 0x09,
	AXP313_ALDO_1V5	= 0x0A,
	AXP313_ALDO_1V6	= 0x0B,
	AXP313_ALDO_1V7	= 0x0C,
	AXP313_ALDO_1V8	= 0x0D,
	AXP313_ALDO_1V9	= 0x0E,
	AXP313_ALDO_2V0	= 0x0F,
	AXP313_ALDO_2V1	= 0x10,
	AXP313_ALDO_2V2	= 0x11,
	AXP313_ALDO_2V3	= 0x12,
	AXP313_ALDO_2V4	= 0x13,
	AXP313_ALDO_2V5	= 0x14,
	AXP313_ALDO_2V6	= 0x15,
	AXP313_ALDO_2V7	= 0x16,
	AXP313_ALDO_2V8	= 0x17,
	AXP313_ALDO_2V9	= 0x18,
	AXP313_ALDO_3V0	= 0x19,
	AXP313_ALDO_3V1	= 0x1A,
	AXP313_ALDO_3V2	= 0x1B,
	AXP313_ALDO_3V3	= 0x1C,
	AXP313_ALDO_3V4	= 0x1D,
	AXP313_ALDO_3V5	= 0x1E,
};

#define AXP313_DCDC_WORKMODE_AUTO	0
#define AXP313_DCDC_WORKMODE_PWM	1

enum axp313_dcdc_regulator {
	AXP313_DCDC1 = 0,
	AXP313_DCDC2,
	AXP313_DCDC3,
};

int axp313_set_dcdc_workmode(enum axp313_dcdc_regulator reg, int mode);
int axp313_set_dcdc1(int mvolt);
int axp313_set_dcdc2(int mvolt);
int axp313_set_dcdc3(int mvolt);
int axp313_set_aldo1(enum axp313_aldo_voltage aldo_val);
int axp313_set_dldo1(enum axp313_aldo_voltage aldo_val);
int axp313_init(unsigned char i2c_addr);
int axp313_set_poweroff_sequence(void);
int axp313_set_offlevel(bool long_offlevel);
