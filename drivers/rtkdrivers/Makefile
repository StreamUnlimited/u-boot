# SPDX-License-Identifier: GPL-2.0-only
# drm/tegra depends on host1x, so if both drivers are built-in care must be
# taken to initialize them in the correct order. Link order is the only way
# to ensure this currently.

obj-$(CONFIG_SPIC_RTK_AMEBA_RXI312) 	+= spi/

obj-$(CONFIG_SERIAL_RTK_AMEBA) 			+= serial/

obj-$(CONFIG_USB_RTK_AMEBA_USB20PHY) 	+= phy/

obj-$(CONFIG_OTP_RTK_AMEBA) 			+= otp/
