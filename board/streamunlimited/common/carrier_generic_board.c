#include <common.h>
#include <asm/gpio.h>
#include <asm/arch/mx7-pins.h>

#include "device_interface.h"

/*
 * This currently handles only the NPB_IN to detect if we
 * want to trigger a software upgrade.
 */

#define FWUP_TIMEOUT	3000	/* in ms */
#define FWUP_GPIO IMX_GPIO_NR(3, 21)

static int fwup_request = 0;

static const iomux_v3_cfg_t fwupdate_pads[] = {
	MX7D_PAD_LCD_DATA16__GPIO3_IO21 | MUX_PAD_CTRL(PAD_CTL_PUS_PU47KOHM),
};

static int generic_board_init(const struct sue_device_info *device)
{
	imx_iomux_v3_setup_multiple_pads(fwupdate_pads, ARRAY_SIZE(fwupdate_pads));
	gpio_direction_input(FWUP_GPIO);

	return 0;
}

static int generic_board_late_init(const struct sue_device_info *device)
{
	int totaldelay = 0;
	int gpio_was_pressed = 0;

	while (gpio_get_value(FWUP_GPIO)) {
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
