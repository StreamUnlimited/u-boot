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

struct adc_map_entry {
	u8 adc_code;
	u16 adc_value_min;
	u16 adc_value_max;
};

/*
 * This is based on the table from:
 * 	https://extern.streamunlimited.com:8443/display/Stream800/StreamVienna+Hardware+Configuration
 */
static const struct adc_map_entry adc_map[] = {
	{ 0x01, 0x0000, 0x0088 },
	{ 0x02, 0x0088, 0x0111 },
	{ 0x03, 0x0111, 0x0199 },
	{ 0x04, 0x0199, 0x0222 },
	{ 0x05, 0x0222, 0x02AA },
	{ 0x06, 0x02AA, 0x0333 },
	{ 0x07, 0x0333, 0x03BB },
	{ 0x08, 0x03BB, 0x0444 },
	{ 0x09, 0x0444, 0x04CC },
	{ 0x0A, 0x04CC, 0x0555 },
	{ 0x0B, 0x0555, 0x05DD },
	{ 0x0C, 0x05DD, 0x0666 },
	{ 0x0D, 0x0666, 0x06EE },
	{ 0x0E, 0x06EE, 0x0777 },
	{ 0x0F, 0x0777, 0x07FF },
	{ 0x10, 0x07FF, 0x0888 },
	{ 0x11, 0x0888, 0x0910 },
	{ 0x12, 0x0910, 0x0999 },
	{ 0x13, 0x0999, 0x0A21 },
	{ 0x14, 0x0A21, 0x0AAA },
	{ 0x15, 0x0AAA, 0x0B32 },
	{ 0x16, 0x0B32, 0x0BBB },
	{ 0x17, 0x0BBB, 0x0C43 },
	{ 0x18, 0x0C43, 0x0CCC },
	{ 0x19, 0x0CCC, 0x0D54 },
	{ 0x1A, 0x0D54, 0x0DDD },
	{ 0x1B, 0x0DDD, 0x0E65 },
	{ 0x1C, 0x0E65, 0x0EEE },
	{ 0x1D, 0x0EEE, 0x0F76 },
	{ 0x1E, 0x0F76, 0x0FFF },
};

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
};

static const char *carrier_names[] = {
	"unknown",
	"factory tester",
	"StreamKit",
};

static const char *canonical_carrier_names[] = {
	"unknown",
	"factory",
	"streamkit"
};

static  const char *daughter_names[] = {
	"unknown",
	"empty",
	"highend",
	"voice",
};

static const char *canonical_daughter_names[] = {
	"unknown",
	"empty",
	"highend",
	"voice",
};

extern struct sue_carrier_ops generic_board_ops;

/*
 * All board except the DemoClients are handled by the generic_board_ops,
 * currently the difference between demo_client_ops and generic_board_ops
 * is, that the demo_client_ops also check for the old FWUP GPIO in addition
 * to the NPB_IN pin.
 */
static const struct sue_carrier_ops *sue_carrier_ops[] = {
	&generic_board_ops,		/* unknown */
	&generic_board_ops,		/* StreamKit Prime (legacy) */
	&generic_board_ops,		/* Factory tester */
	&generic_board_ops,		/* StreamKit */
};

struct carrier_map_entry {
	enum sue_carrier carrier;
	u8 carrier_version;
	u8 msb_code;
	u8 lsb_code;
	u8 flags;
};

/*
 * NOTE: currently the demo client has the same resistors for all revisions,
 * so we just set it to zero here.
 *
 * NOTE: on the stream810 with the basic interface the LSB will always read
 * 0x01, so we always assume it's a normal demo client.
 */
static const struct carrier_map_entry carrier_map[] = {
	{ SUE_CARRIER_FACTORY_TESTER,		0, 0x19, 0x01, 0 },
	{ SUE_CARRIER_FACTORY_TESTER,		0, 0x06, 0x18, 0 },
	{ SUE_CARRIER_FACTORY_TESTER,		1, 0x03, 0x01, 0 },
	{ SUE_CARRIER_FACTORY_TESTER,		1, 0x03, 0x18, 0 },
};

struct daughter_map_entry {
	enum sue_daughter daughter;
	u8 daughter_version;
	u8 code;
};

static const struct daughter_map_entry daughter_map[] = {
	{ SUE_DAUGHTER_EMPTY,	0, 0x01 },
	{ SUE_DAUGHTER_HE,	0, 0x11 },
	{ SUE_DAUGHTER_VOICE,	0, 0x13 },
	{ SUE_DAUGHTER_VOICE,	0, 0x14 },
};

static int get_adc_code(u16 adc_value)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(adc_map); i++) {
		if (adc_value >= adc_map[i].adc_value_min && adc_value < adc_map[i].adc_value_max)
			return adc_map[i].adc_code;
	}

	return -EINVAL;
}

static int fill_device_info(struct sue_device_info *device, u16 module_code,
			u16 carrier_msb_adc_value, u16 carrier_lsb_adc_value)
{
	int i;
	int carrier_msb_code, carrier_lsb_code;

	device->module_code = module_code;

	for (i = 0; i < ARRAY_SIZE(module_map); i++) {
		if (module_map[i].module_code == module_code) {
			device->module = module_map[i].module;
			device->module_version = module_map[i].module_version;
			break;
		}
	}

#if defined(CONFIG_SUE_ADC_CARRIER_DETECT)
	/* NOTE: We fill these raw values just for debugging purposes */
	device->carrier_msb_adc_value = carrier_msb_adc_value;
	device->carrier_lsb_adc_value = carrier_lsb_adc_value;

	carrier_msb_code = get_adc_code(carrier_msb_adc_value);
	if (carrier_msb_code < 0)
		return carrier_msb_code;
	device->carrier_msb_code = carrier_msb_code;

	carrier_lsb_code = get_adc_code(carrier_lsb_adc_value);
	if (carrier_lsb_code < 0)
		return carrier_lsb_code;
	device->carrier_lsb_code = carrier_lsb_code;

	for (i = 0; i < ARRAY_SIZE(carrier_map); i++) {
		if (carrier_map[i].flags & SUE_CARRIER_FLAGS_HAS_DAUGHTER) {
			if (carrier_map[i].msb_code == carrier_msb_code) {
				int j;

				device->carrier = carrier_map[i].carrier;
				device->carrier_version = carrier_map[i].carrier_version;
				device->carrier_flags = carrier_map[i].flags;

				for (j = 0; j < ARRAY_SIZE(daughter_map); j++) {
					if (daughter_map[j].code == carrier_lsb_code) {
						device->daughter = daughter_map[j].daughter;
						device->daughter_version = daughter_map[j].daughter_version;
					}
				}

				break;
			}
		} else {
			if (carrier_map[i].msb_code == carrier_msb_code && carrier_map[i].lsb_code == carrier_lsb_code) {
				device->carrier = carrier_map[i].carrier;
				device->carrier_version = carrier_map[i].carrier_version;
				device->carrier_flags = carrier_map[i].flags;
				break;
			}
		}
	}
#else

	/* Without ADC detection we set the carrier board based on the configuration */
#if defined(CONFIG_TARGET_STREAM195X_STREAMKIT)
	device->carrier = SUE_CARRIER_STREAMKIT;
	device->carrier_version = 0;
	device->carrier_flags = 0;
#else
	#error "Make sure a valid Stream195x carrier board is selected"
#endif

#endif /* defined(CONFIG_SUE_ADC_CARRIER_DETECT) */

	device->carrier_ops = NULL;

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
	u16 carrier_msb_adc_value, carrier_lsb_adc_value;

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

#if defined(CONFIG_SUE_ADC_CARRIER_DETECT)
	printf("ERROR: implement ADC readout for carrier board detection\n");
	carrier_msb_adc_value = 0;
	carrier_lsb_adc_value = 0;
#endif

	ret = fill_device_info(device, module_code, carrier_msb_adc_value, carrier_lsb_adc_value);

	return ret;
}

int sue_print_device_info(const struct sue_device_info *device)
{
#if defined(CONFIG_SUE_ADC_CARRIER_DETECT)
	printf("ADC values: Carrier board: 0x%04x 0x%04x\n", device->carrier_msb_adc_value, device->carrier_lsb_adc_value);
	printf("Codes     : Module: 0x%02x, Carrier board: 0x%02x 0x%02x\n",
			device->module_code, device->carrier_msb_code, device->carrier_lsb_code);
#endif

	printf("Module    : %s (L%d)\n", module_names[device->module], device->module_version);
	printf("Carrier   : %s (L%d)\n", carrier_names[device->carrier], device->carrier_version);
	if (device->carrier_flags & SUE_CARRIER_FLAGS_HAS_DAUGHTER) {
		printf("Daughter  : %s (L%d)\n", daughter_names[device->daughter], device->daughter_version);
	}

	return 0;
}

const char *sue_device_get_canonical_module_name(const struct sue_device_info *device)
{
	return canonical_module_names[device->module];
}

const char *sue_device_get_canonical_carrier_name(const struct sue_device_info *device)
{
	return canonical_carrier_names[device->carrier];
}

const char *sue_device_get_canonical_daughter_name(const struct sue_device_info *device)
{
	return canonical_daughter_names[device->daughter];
}

int sue_carrier_ops_init(struct sue_device_info *device)
{
	if (device->carrier < ARRAY_SIZE(sue_carrier_ops)) {
		device->carrier_ops = sue_carrier_ops[device->carrier];
	} else {
		printf("WARNING: carrier index out of range\n");
		device->carrier_ops = NULL;
	}

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
