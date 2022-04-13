/*
 * (C) Copyright 2019 StreamUnlimited Engineering GmbH
 * Martin Pietryka <martin.pietryka@streamunlimited.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/gpio.h>
#include <asm/arch/imx8mm_pins.h>
#include "device_interface.h"

/*
 * These names are more human friendly and can be used for printing.
 */
static const char *module_names[] = {
	"unknown",
#ifdef CONFIG_TARGET_STREAM195X_NAND
	"stream195x NAND/DDR3",
#else
	"stream195x eMMC/DDR4",
#endif
};

/*
 * These names can be used where no spaces or other special charactar are allowed,
 * e.g. for fit configurations.
 */
static const char *canonical_module_names[] = {
	"unknown",
#ifdef CONFIG_TARGET_STREAM195X_NAND
	"stream195xnand",
#else
	"stream195x",
#endif
};

struct module_map_entry {
	enum sue_module module;
	u8 module_version;
	u16 module_code;
};

static const struct module_map_entry module_map[] = {
	{ SUE_MODULE_S195X, 0, 0x00 },
	{ SUE_MODULE_S195X, 0, 0x01 },
};

extern struct sue_carrier_ops generic_board_ops;

static int fill_device_info(struct sue_device_info *device, u16 module_code)
{
	int i;

	device->module_code = module_code;

	for (i = 0; i < ARRAY_SIZE(module_map); i++) {
		if (module_map[i].module_code == module_code) {
			device->module = module_map[i].module;
			device->module_version = module_map[i].module_version;
			break;
		}
	}

	return 0;
}

/*
 * These GPIOs are used as the bits for a module code, the first entry
 * represents LSB.
 */
#if defined(CONFIG_TARGET_STREAM195X_NAND)
static const unsigned int s195x_module_code_gpios[] = {
	IMX_GPIO_NR(3, 3),
	IMX_GPIO_NR(3, 4),
};

static iomux_v3_cfg_t const s195x_module_code_pads[] = {
	IMX8MM_PAD_NAND_CE1_B_GPIO3_IO2 | MUX_PAD_CTRL(NO_PAD_CTRL),
	IMX8MM_PAD_NAND_CE2_B_GPIO3_IO3 | MUX_PAD_CTRL(NO_PAD_CTRL),
	IMX8MM_PAD_NAND_CE3_B_GPIO3_IO4 | MUX_PAD_CTRL(NO_PAD_CTRL),
};
#elif defined(CONFIG_TARGET_STREAM195X_EMMC)
static const unsigned int s195x_module_code_gpios[] = {
	IMX_GPIO_NR(3, 7),
	IMX_GPIO_NR(3, 8),
};

static iomux_v3_cfg_t const s195x_module_code_pads[] = {
	IMX8MM_PAD_NAND_DATA00_GPIO3_IO6 | MUX_PAD_CTRL(NO_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA01_GPIO3_IO7 | MUX_PAD_CTRL(NO_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA02_GPIO3_IO8 | MUX_PAD_CTRL(NO_PAD_CTRL),
};
#endif
int sue_device_detect(struct sue_device_info *device)
{
	int ret, i;
	u16 module_code = 0;

	/*
	 * Read GPIOs to form module code
	 */
	imx_iomux_v3_setup_multiple_pads(s195x_module_code_pads, ARRAY_SIZE(s195x_module_code_pads));

	for (i = 0; i < ARRAY_SIZE(s195x_module_code_gpios); i++) {
		gpio_request(s195x_module_code_gpios[i], "module detect");
		gpio_direction_input(s195x_module_code_gpios[i]);

		if (gpio_get_value(s195x_module_code_gpios[i]))
			module_code |= (1 << i);

		gpio_free(s195x_module_code_gpios[i]);
	}

	ret = fill_device_info(device, module_code);

	return ret;
}

int sue_print_device_info(const struct sue_device_info *device)
{
	printf("Module    : %s (L%d)\n", module_names[device->module], device->module_version);

	return 0;
}

const char *sue_device_get_canonical_module_name(const struct sue_device_info *device)
{
	return canonical_module_names[device->module];
}

int sue_carrier_ops_init(struct sue_device_info *device)
{
#if defined(CONFIG_TARGET_STREAM195X_STREAMKIT)
	device->carrier_ops = &generic_board_ops;
#else
	#error "Make sure a valid Stream195x carrier board is selected"
#endif

	return 0;
}

int sue_carrier_init(const struct sue_device_info *device)
{
	if (device == NULL || device->carrier_ops == NULL || device->carrier_ops->init == NULL)
		return -EIO;

	return device->carrier_ops->init(device);
}

int sue_carrier_late_init(const struct sue_device_info *device)
{
	if (device == NULL || device->carrier_ops == NULL || device->carrier_ops->late_init == NULL)
		return -EIO;

	return device->carrier_ops->late_init(device);
}

int sue_carrier_get_usb_update_request(const struct sue_device_info *device)
{
	if (device == NULL || device->carrier_ops == NULL || device->carrier_ops->get_usb_update_request == NULL)
		return -EIO;

	return device->carrier_ops->get_usb_update_request(device);
}
