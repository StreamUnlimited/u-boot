#include <common.h>
#include <asm/gpio.h>
#include <asm/arch/mx7-pins.h>

#include "device_interface.h"

#define FWUP_GPIO IMX_GPIO_NR(6, 20)

#define ALT_FWUP_TIMEOUT	3000	/* in ms */
#define ALT_FWUP_GPIO IMX_GPIO_NR(3, 21)

static int alt_fwup_request = 0;

static const iomux_v3_cfg_t fwupdate_pads[] = {
	MX7D_PAD_SAI2_TX_BCLK__GPIO6_IO20 | MUX_PAD_CTRL(PAD_CTL_PUE | PAD_CTL_PUS_PU5KOHM),
	MX7D_PAD_LCD_DATA16__GPIO3_IO21 | MUX_PAD_CTRL(PAD_CTL_PUS_PU47KOHM),
};

static int demo_client_init(const struct sue_device_info *device)
{
	imx_iomux_v3_setup_multiple_pads(fwupdate_pads, ARRAY_SIZE(fwupdate_pads));
	gpio_direction_input(FWUP_GPIO);
	gpio_direction_input(ALT_FWUP_GPIO);

	return 0;
}

static int demo_client_late_init(const struct sue_device_info *device)
{
	int totaldelay = 0;

	while (gpio_get_value(ALT_FWUP_GPIO)) {
		if (totaldelay > ALT_FWUP_TIMEOUT) {
			printf("Alt fwup request set\n");
			alt_fwup_request = 1;
			break;
		}

		mdelay(100);
		totaldelay += 100;
	}

	return 0;
}

static int demo_client_get_usb_update_request(const struct sue_device_info *device)
{
	return alt_fwup_request || !gpio_get_value(FWUP_GPIO);
}

struct sue_carrier_ops demo_client_ops = {
	.init = demo_client_init,
	.late_init = demo_client_late_init,
	.get_usb_update_request = demo_client_get_usb_update_request,
};
