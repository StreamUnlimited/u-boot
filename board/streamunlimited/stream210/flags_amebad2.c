// SPDX-License-Identifier: GPL-2.0-or-later

#include <common.h>
#include <asm/io.h>

#define BACKUP2_REG_ADDR	0x420080E8

int flag_write(u8 index, bool val)
{
	u32 reg;

	if (index > 7)
		return -EINVAL;

	reg = readl(BACKUP2_REG_ADDR);
	reg &= ~(1 << index);
	if (val)
		reg |= (1 << index);
	writel(reg, BACKUP2_REG_ADDR);

	return 0;
}

int flag_read(u8 index, bool *val)
{
	u32 reg;

	if (index > 7)
		return -EINVAL;

	reg = readl(BACKUP2_REG_ADDR);
	*val = ((reg & (1 << index)) != 0) ? true : false;

	return 0;
}

int flags_clear(void)
{
	u32 reg;

	reg = readl(BACKUP2_REG_ADDR);
	reg &= ~(0xFF);
	writel(reg, BACKUP2_REG_ADDR);

	return 0;
}

int bootcnt_write(u8 data)
{
	u32 reg;

	reg = readl(BACKUP2_REG_ADDR);
	reg &= ~(0xFF << 8);
	reg |= (data << 8);
	writel(reg, BACKUP2_REG_ADDR);

	return 0;
}

int bootcnt_read(u8 *data)
{
	u32 reg;

	reg = readl(BACKUP2_REG_ADDR);
	*data = (reg >> 8) & 0xFF;

	return 0;
}
