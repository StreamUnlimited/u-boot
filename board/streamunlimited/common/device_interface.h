/*
 * (C) Copyright 2019 StreamUnlimited Engineering GmbH
 * Martin Pietryka <martin.pietryka@streamunlimited.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __DEVICE_INTERFACE_H__
#define __DEVICE_INTERFACE_H__

enum sue_module {
	SUE_MODULE_UNKNOWN,
	SUE_MODULE_S1955M,
	SUE_MODULE_S1955Q,
	SUE_MODULE_S1955I,
	SUE_MODULE_S1955P,
	SUE_MODULE_S1955N,
	SUE_MODULE_S1955J,
	SUE_MODULE_S1955K,
	SUE_MODULE_S1955O,
	SUE_MODULE_S1955IE,
	SUE_MODULE_S1955KE,
	SUE_MODULE_S1977IE,
	SUE_MODULE_S1977KE,
};

#define AUDIO_3D_FUSE_MASK		(0x0000000F)
#define AUDIO_3D_ENABLE			(0x00000008)
#define AUDIO_3D_AC4_ENABLE		(0x00000004)
#define AUDIO_3D_DATMOS_ENABLE	(0x00000002)
#define AUDIO_3D_DDTS_ENABLE	(0x00000001)

enum sue_audio_3d_soc_version {
	AUDIO_3D_SOC_VERSION_A = 0,
	AUDIO_3D_SOC_VERSION_C = (AUDIO_3D_ENABLE | AUDIO_3D_DATMOS_ENABLE),
	AUDIO_3D_SOC_VERSION_D = (AUDIO_3D_ENABLE | AUDIO_3D_DATMOS_ENABLE | AUDIO_3D_DDTS_ENABLE),
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

	const struct sue_carrier_ops *carrier_ops;
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

#endif /* __DEVICE_INTERFACE_H__ */
