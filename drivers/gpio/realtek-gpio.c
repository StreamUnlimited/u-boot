// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek GPIO support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <common.h>
#include <dm.h>
#include <dm/pinctrl.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <fdtdec.h>

#include "realtek-gpio.h"

DECLARE_GLOBAL_DATA_PTR;

#define REALTEK_GPIO_PINS_PER_BANK 32
#ifdef CONFIG_SOC_CPU_ARMA7
#define REALTEK_GPIOC_PINS_PER_BANK 7
#endif
#ifdef CONFIG_SOC_CPU_ARMA32
#define REALTEK_GPIOC_PINS_PER_BANK 8
#endif

struct realtek_gpio_bank {
	unsigned long reg_base;
	const char *bank_name;
};

static int realtek_gpio_get(struct udevice *dev, unsigned offset)
{
	struct realtek_gpio_bank *bank = dev_get_platdata(dev);
	int ret;

	ret = !!(readl(bank->reg_base + GPIO_EXT_PORT) & BIT(offset));

	return ret;
}

static void realtek_gpio_set_internal(struct udevice *chip, unsigned offset, int value)
{
	struct realtek_gpio_bank *bank = dev_get_platdata(chip);
	u32 reg_value;

	reg_value = readl(bank->reg_base + GPIO_DR);

	if (value == GPIO_PIN_LOW) {
		reg_value &= ~BIT(offset);
	} else {
		reg_value |= BIT(offset);
	}

	writel(reg_value, bank->reg_base + GPIO_DR);
}

static int realtek_gpio_set(struct udevice *chip, unsigned offset, int value)
{
	realtek_gpio_set_internal(chip, offset, value);

	return 0;
}

static int realtek_gpio_direction_input(struct udevice *chip, unsigned offset)
{
	struct realtek_gpio_bank *bank = dev_get_platdata(chip);
	u32 reg_value;

	reg_value = readl(bank->reg_base + GPIO_DDR);
	reg_value &= ~BIT(offset);
	writel(reg_value, bank->reg_base + GPIO_DDR);

	return 0;
}

static int realtek_gpio_direction_output(struct udevice *chip,
		unsigned offset, int value)
{
	struct realtek_gpio_bank *bank = dev_get_platdata(chip);
	u32 reg_value;

	realtek_gpio_set_internal(chip, offset, value);

	reg_value = readl(bank->reg_base + GPIO_DDR);
	reg_value |= BIT(offset);
	writel(reg_value, bank->reg_base + GPIO_DDR);

	return 0;
}

static const struct dm_gpio_ops gpio_realtek_ops = {
	.direction_input	= realtek_gpio_direction_input,
	.direction_output	= realtek_gpio_direction_output,
	.get_value		= realtek_gpio_get,
	.set_value		= realtek_gpio_set,
};

static int realtek_gpio_probe(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct realtek_gpio_bank *plat = dev_get_platdata(dev);

	uc_priv->gpio_count = REALTEK_GPIO_PINS_PER_BANK;
	uc_priv->bank_name = plat->bank_name;

	return 0;
}

#if CONFIG_IS_ENABLED(OF_CONTROL)
static int realtek_gpio_ofdata_to_platdata(struct udevice *dev)
{
	struct realtek_gpio_bank *plat = dev_get_platdata(dev);
	fdt_addr_t addr;

	addr = devfdt_get_addr(dev);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	plat->reg_base = addr;

	int bank  = fdtdec_get_int(gd->fdt_blob, dev_of_offset(dev),
		"rtk,gpio-bank", 0);
	switch (bank) {
		case 0:
			plat->bank_name  = "PA";
			break;
		case 1:
			plat->bank_name  = "PB";
			break;		
		case 2:
			plat->bank_name  = "PC";
			break;
		default:
			plat->bank_name  = "UNKNOWN";
	}

	return 0;
}
#endif

static const struct udevice_id realtek_ids[] = {
	{ .compatible = "realtek,ameba-gpio", },
	{ }
};

U_BOOT_DRIVER(gpio_realtek) = {
	.name	= "gpio_realtek",
	.id	= UCLASS_GPIO,
	.of_match = of_match_ptr(realtek_ids),
	.ofdata_to_platdata = of_match_ptr(realtek_gpio_ofdata_to_platdata),
	.platdata_auto_alloc_size = sizeof(struct realtek_gpio_bank),
	.ops	= &gpio_realtek_ops,
	.probe	= realtek_gpio_probe,
};
