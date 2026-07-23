/*
 * (C) Copyright 2019 StreamUnlimited Engineering GmbH
 * Martin Pietryka <martin.pietryka@streamunlimited.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/gpio.h>
#include <asm/arch/imx8mm_pins.h>
#include <fuse.h>
#include "device_interface.h"

/*
 * These names are more human friendly and can be used for printing.
 */
static const char *module_names[] = {
	[SUE_MODULE_UNKNOWN]	=	"unknown",
	[SUE_MODULE_S1955M]		=	"stream1955M eMMC/DDR4 4GB",
	[SUE_MODULE_S1955Q]		=	"stream1955Q eMMC/DDR4 8GB",
	[SUE_MODULE_S1955I]		=	"stream1955I eMMC/DDR4 8GB",
	[SUE_MODULE_S1955P]		=	"stream1955P eMMC/DDR4 4GB",
	[SUE_MODULE_S1955N]		=	"stream1955N eMMC/DDR4 8GB",
	[SUE_MODULE_S1955J]		=	"stream1955J eMMC/DDR4 4GB",
	[SUE_MODULE_S1955K]		=	"stream1955K eMMC/DDR4 8GB",
	[SUE_MODULE_S1955O]		=	"stream1955O eMMC/DDR4 8GB",
	[SUE_MODULE_S1955IE]	=	"stream1955IE eMMC/DDR4 8GB",
	[SUE_MODULE_S1955KE]	=	"stream1955KE eMMC/DDR4 8GB",
	[SUE_MODULE_S1977IE]	=	"stream1977IE eMMC/DDR4 8GB",
	[SUE_MODULE_S1977KE]	=	"stream1977KE eMMC/DDR4 8GB",
};

/*
 * These names can be used where no spaces or other special charactar are allowed,
 * e.g. for fit configurations.
 */
static const char *canonical_module_names[] = {
	[SUE_MODULE_UNKNOWN]	=	"unknown",
	[SUE_MODULE_S1955M]		=	"stream195x",
	[SUE_MODULE_S1955Q]		=	"stream195x",
	[SUE_MODULE_S1955I]		=	"stream195x",
	[SUE_MODULE_S1955P]		=	"stream195x",
	[SUE_MODULE_S1955N]		=	"stream195x",
	[SUE_MODULE_S1955J]		=	"stream195x",
	[SUE_MODULE_S1955K]		=	"stream195x",
	[SUE_MODULE_S1955O]		=	"stream195x",
	[SUE_MODULE_S1955IE]	=	"stream195x",
	[SUE_MODULE_S1955KE]	=	"stream195x",
	[SUE_MODULE_S1977IE]	=	"stream195x",	// Keep it the same as for 195x for now so DTS is loaded right
	[SUE_MODULE_S1977KE]	=	"stream195x",	// Keep it the same as for 195x for now so DTS is loaded right
};

struct module_map_entry {
	enum sue_module module;
	u8 module_version;
	u16 module_code;
	enum sue_audio_3d_soc_version audio_3d_version;
};

static const struct module_map_entry module_map[] = {
	{ SUE_MODULE_S1955M,	0,	0b000,	AUDIO_3D_SOC_VERSION_A },
	{ SUE_MODULE_S1955Q,	0,	0b100,	AUDIO_3D_SOC_VERSION_A },
	{ SUE_MODULE_S1955I,	0,	0b110,	AUDIO_3D_SOC_VERSION_A },
	{ SUE_MODULE_S1955P,	0,	0b000,	AUDIO_3D_SOC_VERSION_D },
	{ SUE_MODULE_S1955N,	0,	0b100,	AUDIO_3D_SOC_VERSION_D },
	{ SUE_MODULE_S1955J,	0,	0b010,	AUDIO_3D_SOC_VERSION_D },
	{ SUE_MODULE_S1955K,	0,	0b110,	AUDIO_3D_SOC_VERSION_D },
	{ SUE_MODULE_S1955O,	0,	0b100,	AUDIO_3D_SOC_VERSION_C },
	{ SUE_MODULE_S1955IE,	0,	0b111,	AUDIO_3D_SOC_VERSION_A },
	{ SUE_MODULE_S1955KE,	0,	0b111,	AUDIO_3D_SOC_VERSION_D },
	{ SUE_MODULE_S1977IE,	0,	0b101,	AUDIO_3D_SOC_VERSION_A },
	{ SUE_MODULE_S1977KE,	0,	0b101,	AUDIO_3D_SOC_VERSION_D },
};

extern struct sue_carrier_ops generic_board_ops;

static int fill_device_info(struct sue_device_info *device, u16 module_code, enum sue_audio_3d_soc_version audio_3d_version)
{
	int i;

	device->module = SUE_MODULE_UNKNOWN;
	device->module_code = module_code;
	device->module_version = 0;

	for (i = 0; i < ARRAY_SIZE(module_map); i++) {
		if (module_map[i].module_code == module_code && module_map[i].audio_3d_version == audio_3d_version) {
			device->module = module_map[i].module;
			device->module_version = module_map[i].module_version;
			return 0;
		}
	}

	printf("ERROR: Unable to fill device info! No match for module code: 0x%x, audio_3d_version: 0x%x\n", module_code, audio_3d_version);

	return -ENOENT;
}

/*
 * These GPIOs are used as the bits for a module code, the first entry
 * represents LSB.
 */
static const unsigned int s195x_module_code_gpios[] = {
	IMX_GPIO_NR(3, 8),
	IMX_GPIO_NR(3, 7),
	IMX_GPIO_NR(3, 6),
};

static iomux_v3_cfg_t const s195x_module_code_pads[] = {
	IMX8MM_PAD_NAND_DATA00_GPIO3_IO6 | MUX_PAD_CTRL(NO_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA01_GPIO3_IO7 | MUX_PAD_CTRL(NO_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA02_GPIO3_IO8 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

int sue_device_detect(struct sue_device_info *device)
{
	int ret, i;
	u16 module_code = 0;
	u32 audio_3d_version_fuse = 0;
	enum sue_audio_3d_soc_version audio_3d_version;

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

	/*
	 * Read 3D Audio fuse to form Audio 3D version
	 */
	ret = fuse_read(1, 2, &audio_3d_version_fuse);
	if (ret) {
		printf("ERROR: Unable to read 3D Audio fuse configuration\n");
		return -EIO;
	}

	audio_3d_version_fuse &= AUDIO_3D_FUSE_MASK;

	switch (audio_3d_version_fuse)
	{
	case AUDIO_3D_SOC_VERSION_A:
	case AUDIO_3D_SOC_VERSION_D:
	case AUDIO_3D_SOC_VERSION_C:
		audio_3d_version = (enum sue_audio_3d_soc_version) audio_3d_version_fuse;
		break;
	default:
		printf("WARNING: Unknown 3D Audio fuse configuration (0x%X), defaulting to VERSION_A\n", audio_3d_version_fuse);
		audio_3d_version = AUDIO_3D_SOC_VERSION_A;
		break;
	}

	ret = fill_device_info(device, module_code, audio_3d_version);

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
