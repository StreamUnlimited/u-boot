/*
 * Realtek Semiconductor Corp.
 *
 * Copyright 2012  Jethro Hsu (jethro@realtek.com)
 * Copyright 2012  Tony Wu (tonywu@realtek.com)
 */

#include <common.h>
#include <flash.h>

flash_info_t flash_info[CONFIG_SYS_MAX_FLASH_BANKS];	/* info for FLASH chips */

/*-----------------------------------------------------------------------
 * flash_init()
 *
 * sets up flash_info and returns size of FLASH (bytes)
 */
unsigned long flash_init(void)
{
	printf ("Skipping flash_init\n");
	/* FIXME: Currently a dummy size value is returned.
	 *        Return a actual size in the future flash porting.
	 */
	return (0x10000);
}

int write_buff(flash_info_t *info, uchar *src, ulong addr, ulong cnt)
{
	printf("write_buff not implemented\n");
	return (-1);
}

int flash_erase(flash_info_t *info, int s_first, int s_last)
{
	printf("flash_erase not implemented\n");
	return (-1);
}

void flash_print_info(flash_info_t *info)
{
	printf("flash_print_info not implemented\n");
}
