#include <common.h>
#include <asm/errno.h>
#include <asm/imx-common/adc.h>
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
	{ 0x1E, 0x0F76, 0x0FFF + 1 }, // Biggest possible ADC value needs to also be allowed
};

/*
 * These names are more human friendly and can be used for printing.
 */
static const char *module_names[] = {
	"unknown",
	"stream810 basic",
	"stream810 extended",
	"stream810 extended plus",
	"stream810 basic 4.4W",
	"stream810 extended 2.4EW",
	"stream810x3 extended",
	"stream810h3 extended plus",
	"stream810x3 extended 2.4EW",
};

/*
 * These names can be used where no spaces or other special charactar are allowed,
 * e.g. for fit configurations.
 */
static const char *canonical_module_names[] = {
	"unknown",
	"stream810b",
	"stream810x",
	"stream810h",
	"stream810b_44w",
	"stream810x_24ew",
	"stream810x3",
	"stream810h3",
	"stream810x3_24ew",
};

struct module_map_entry {
	enum sue_module module;
	u8 module_version;
	u8 msb_code;
	u8 lsb_code;
};

static const struct module_map_entry module_map[] = {
	{ SUE_MODULE_S810_BASIC,		2, 0x11, 0x0A },
	{ SUE_MODULE_S810_BASIC,		3, 0x11, 0x06 },
	{ SUE_MODULE_S810_EXTENDED,		2, 0x11, 0x0F },
	{ SUE_MODULE_S810_EXTENDED,		3, 0x11, 0x11 },
	{ SUE_MODULE_S810_EXTENDED_PLUS,	2, 0x11, 0x0C },
	{ SUE_MODULE_S810_EXTENDED_PLUS,	3, 0x11, 0x18 },
	{ SUE_MODULE_S810_BASIC_44W,		4, 0x11, 0x04 },
	{ SUE_MODULE_S810_EXTENDED_24EW,	4, 0x11, 0x03 },
	{ SUE_MODULE_S810X3_EXTENDED,		5, 0x11, 0x07 },
	{ SUE_MODULE_S810H3_EXTENDED_PLUS,	5, 0x11, 0x19 },
	{ SUE_MODULE_S810X3_EXTENDED_24EW,	5, 0x11, 0x1B },
};

static const char *carrier_names[] = {
	"unknown",
	"demo client",
	"highend demo client",
	"StreamKit Go",
	"StreamKit Prime",
	"factory tester",
};

static const char *canonical_carrier_names[] = {
	"unknown",
	"democlient",
	"hedemoclient",
	"streamkitgo",
	"streamkitprime",
	"factory",
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

extern struct sue_carrier_ops demo_client_ops;
extern struct sue_carrier_ops generic_board_ops;

/*
 * All board except the DemoClients are handled by the generic_board_ops,
 * currently the difference between demo_client_ops and generic_board_ops
 * is, that the demo_client_ops also check for the old FWUP GPIO in addition
 * to the NPB_IN pin.
 */
static const struct sue_carrier_ops *sue_carrier_ops[] = {
	&generic_board_ops,		/* unknown */
	&demo_client_ops,		/* demo client */
	&demo_client_ops,		/* highend demo client, it is also handled by the demo client board file */
	&generic_board_ops,		/* StreamKit Go */
	&generic_board_ops,		/* StreamKit Prime */
	&generic_board_ops,		/* Factory tester */
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
	{ SUE_CARRIER_DEMO_CLIENT,		0, 0x1E, 0x01, 0 },
	{ SUE_CARRIER_HE_DEMO_CLIENT,		0, 0x1E, 0x1E, 0 },

	{ SUE_CARRIER_STREAMKIT_GO,		2, 0x15, 0x01, 0 },
	{ SUE_CARRIER_STREAMKIT_GO,		2, 0x16, 0x01, 0 },

	{ SUE_CARRIER_STREAMKIT_PRIME,		0, 0x04, 0x00, SUE_CARRIER_FLAGS_HAS_DAUGHTER },

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

static int fill_device_info(struct sue_device_info *device, u16 module_msb_adc_value, u16 module_lsb_adc_value,
			u16 carrier_msb_adc_value, u16 carrier_lsb_adc_value)
{
	int i;
	int module_msb_code, module_lsb_code;
	int carrier_msb_code, carrier_lsb_code;

	/* NOTE: We fill these raw values just for debugging purposes */
	device->module_msb_adc_value = module_msb_adc_value;
	device->module_lsb_adc_value = module_lsb_adc_value;
	device->carrier_msb_adc_value = carrier_msb_adc_value;
	device->carrier_lsb_adc_value = carrier_lsb_adc_value;

	module_msb_code = get_adc_code(module_msb_adc_value);
	if (module_msb_code < 0)
		return module_msb_code;
	device->module_msb_code = module_msb_code;

	module_lsb_code = get_adc_code(module_lsb_adc_value);
	if (module_lsb_code < 0)
		return module_lsb_code;
	device->module_lsb_code = module_lsb_code;

	carrier_msb_code = get_adc_code(carrier_msb_adc_value);
	if (carrier_msb_code < 0)
		return carrier_msb_code;
	device->carrier_msb_code = carrier_msb_code;

	carrier_lsb_code = get_adc_code(carrier_lsb_adc_value);
	if (carrier_lsb_code < 0)
		return carrier_lsb_code;
	device->carrier_lsb_code = carrier_lsb_code;


	for (i = 0; i < ARRAY_SIZE(module_map); i++) {
		if (module_map[i].msb_code == module_msb_code && module_map[i].lsb_code == module_lsb_code) {
			device->module = module_map[i].module;
			device->module_version = module_map[i].module_version;
			break;
		}
	}

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

	device->carrier_ops = NULL;
	device->fec2_phy_addr = -1;

	return 0;
}

int sue_device_detect(struct sue_device_info *device)
{
	int ret;
	u16 module_msb_adc_value, module_lsb_adc_value;
	u16 carrier_msb_adc_value, carrier_lsb_adc_value;

	init_adc(1);
	init_adc(2);

	module_msb_adc_value = read_adc_channel(1, 0);
	module_lsb_adc_value = read_adc_channel(1, 2);

	carrier_msb_adc_value = read_adc_channel(1, 3);
	carrier_lsb_adc_value = read_adc_channel(2, 2);

	ret = fill_device_info(device, module_msb_adc_value, module_lsb_adc_value, carrier_msb_adc_value, carrier_lsb_adc_value);

	shutdown_adc(1);
	shutdown_adc(2);

	return ret;
}

int sue_print_device_info(const struct sue_device_info *device)
{
	printf("ADC values: Module: 0x%04x 0x%04x, Carrier board: 0x%04x 0x%04x\n",
			device->module_msb_adc_value, device->module_lsb_adc_value, device->carrier_msb_adc_value, device->carrier_lsb_adc_value);
	printf("Codes     : Module:   0x%02x   0x%02x, Carrier board:   0x%02x   0x%02x\n",
			device->module_msb_code, device->module_lsb_code, device->carrier_msb_code, device->carrier_lsb_code);

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
