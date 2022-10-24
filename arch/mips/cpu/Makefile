# SPDX-License-Identifier: GPL-2.0+

extra-y	= start.o

obj-y += time.o
obj-y += interrupts.o
obj-y += cpu.o

obj-$(CONFIG_MIPS_CM)	+= cm_init.o

ifeq ($(CONFIG_MIPS_CMP),y)
obj-y += taroko-cmp.o taroko-cm.o taroko-cpc.o
endif
