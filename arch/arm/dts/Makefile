# SPDX-License-Identifier: GPL-2.0+

dtb-$(CONFIG_TARGET_RTL8730E) += \
	rtl8730e-uboot-nor.dtb \
	rtl8730e-uboot-nand.dtb \

targets += $(dtb-y)

# Add any required device tree compiler flags here
DTC_FLAGS +=

PHONY += dtbs
dtbs: $(addprefix $(obj)/, $(dtb-y))
	@:

clean-files := *.dtb *.dtbo *_HS
