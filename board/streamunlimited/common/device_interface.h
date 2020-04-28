#ifndef __DEVICE_INTERFACE_H__
#define __DEVICE_INTERFACE_H__

#define SUE_CARRIER_FLAGS_HAS_DAUGHTER		(1 << 0)	/* indicates that the carrier LSB is a configuration for a daughter board */

enum sue_module {
	SUE_MODULE_UNKNOWN,
	SUE_MODULE_S1832,
};

enum sue_carrier {
	SUE_CARRIER_UNKNOWN,
	SUE_CARRIER_STREAMKIT_PRIME,
	SUE_CARRIER_FACTORY_TESTER,
	SUE_CARRIER_MAX,
};

enum sue_daughter {
	SUE_DAUGHTER_UNKNOWN,
	SUE_DAUGHTER_EMPTY,
	SUE_DAUGHTER_HE,
	SUE_DAUGHTER_VOICE,
};

enum sue_reset_cause {
	SUE_RESET_CAUSE_UNKNOWN,
	SUE_RESET_CAUSE_POR,
	SUE_RESET_CAUSE_SOFTWARE,
	SUE_RESET_CAUSE_WDOG,
};

struct sue_device_info {
	enum sue_reset_cause reset_cause;

	u16 module_code;
	enum sue_module module;
	u8 module_version;

	u16 carrier_msb_adc_value, carrier_lsb_adc_value;
	int carrier_msb_code, carrier_lsb_code;
	enum sue_carrier carrier;
	u8 carrier_version;

	u8 carrier_flags;
	enum sue_daughter daughter;
	u8 daughter_version;

	const struct sue_carrier_ops *carrier_ops;

	/*
	 * If this is set to a value other than -1, the board.c file
	 * will patch the devicetree before booting the kernel. The
	 * patching will set the phy address of the fec2 MAC accordingly.
	 */
	int fec2_phy_addr;
};

struct sue_carrier_ops {
	int (*init)(const struct sue_device_info *device);
	int (*late_init)(const struct sue_device_info *device);
	int (*get_usb_update_request)(const struct sue_device_info *device);
};

int sue_carrier_init(const struct sue_device_info *device);
int sue_carrier_late_init(const struct sue_device_info *device);
int sue_carrier_get_usb_update_request(const struct sue_device_info *device);

int sue_device_detect(struct sue_device_info *device);
int sue_carrier_ops_init(struct sue_device_info *device);
int sue_print_device_info(const struct sue_device_info *device);

const char *sue_device_get_canonical_module_name(const struct sue_device_info *device);
const char *sue_device_get_canonical_carrier_name(const struct sue_device_info *device);
const char *sue_device_get_canonical_daughter_name(const struct sue_device_info *device);

#endif /* __DEVICE_INTERFACE_H__ */
