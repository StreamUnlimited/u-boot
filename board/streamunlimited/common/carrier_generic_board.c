#include <common.h>
#include <asm/gpio.h>

#include "device_interface.h"

#define FWUP_TIMEOUT	2200	/* in ms */
#define FWUP_GPIO GPIOAO_3
#define FWUP_GPIO_NAME "GPIOAO_3"

static int fwup_request = 0;
static int fwup_initialized = 0;

static int generic_board_init(const struct sue_device_info *device)
{
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
	int gpio_was_pressed = 0;

	while (fwup_initialized && gpio_get_value(FWUP_GPIO)) {
		gpio_was_pressed = 1;

		if (totaldelay > FWUP_TIMEOUT && !fwup_request) {
			printf("fwup request set, release NPB_IN now\n");
			fwup_request = 1;
		}

		mdelay(100);
		totaldelay += 100;
	}

	if (gpio_was_pressed)
		mdelay(100);

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
