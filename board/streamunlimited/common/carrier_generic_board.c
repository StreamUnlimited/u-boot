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

#define FWUP_TIMEOUT	2200	/* in ms */
#define FWUP_GPIO IMX_GPIO_NR(5, 27)
#define FWUP_GPIO_NAME "BUTTON_SENSE"

static iomux_v3_cfg_t const fwup_pads[] = {
	IMX8MM_PAD_UART3_TXD_GPIO5_IO27 | MUX_PAD_CTRL(PAD_CTL_PUE | PAD_CTL_PE),
};

static int fwup_request = 0;
static int fwup_initialized = 0;

static int generic_board_init(const struct sue_device_info *device)
{
	imx_iomux_v3_setup_multiple_pads(fwup_pads, ARRAY_SIZE(fwup_pads));

	if (gpio_request(FWUP_GPIO, FWUP_GPIO_NAME)) {
		printf("fwup failed to request gpio: %s\n", FWUP_GPIO_NAME);
		return 1;
	}

	if (gpio_direction_input(FWUP_GPIO)) {
		printf("fwup failed to set direction for gpio: %s\n", FWUP_GPIO_NAME);
		return 1;
	}

	fwup_initialized = 1;
	return 0;
}

static int generic_board_late_init(const struct sue_device_info *device)
{
	int totaldelay = 0;

	if (!fwup_initialized) {
		printf("fwup not initialized\n");
		return 1;
	}

	/*
	 * Block until FWUP_GPIO is released. If it was held for longer than
	 * FWUP_TIMEOUT, an update was requested by the user/host controller.
	 */
	while (gpio_get_value(FWUP_GPIO)) {
		if (totaldelay > FWUP_TIMEOUT && !fwup_request) {
			printf("fwup request set, release NPB_IN now\n");
			fwup_request = 1;
		}

		mdelay(100);
		totaldelay += 100;
	}

	return 0;
}

static int generic_board_get_usb_update_request(const struct sue_device_info *device)
{
	return fwup_request;
}

struct sue_carrier_ops generic_board_ops = {
	.init = generic_board_init,
	.late_init = generic_board_late_init,
	.get_usb_update_request = generic_board_get_usb_update_request,
};
