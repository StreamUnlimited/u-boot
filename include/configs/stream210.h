// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef _STREAM210_H_
#define _STREAM210_H_

#include <sue_fwupdate_common.h>

/* valid baudrates */
#define CONFIG_SYS_BAUDRATE_TABLE	{ 9600, 19200, 38400, 57600, 115200 , 1500000}

#define CONFIG_TIMESTAMP				/* Print image info with timestamp */

#undef CONFIG_MTDPARTS_DEFAULT
#define CONFIG_MTDPARTS_DEFAULT "mtdparts=spi-nand0:"			\
	"0x00120000@0x00000000(boot_a),"				\
	"0x00120000@0x00120000(fip_a),"					\
	"0x00120000@0x00240000(boot_b),"				\
	"0x00120000@0x00360000(fip_b),"					\
	"0x00080000@0x00480000(environment),"				\
	"0x00080000@0x00500000(constants),"				\
	"0x01200000@0x00580000(swufit),"				\
	"0x00700000@0x01780000(fit),"					\
	"0x0E180000@0x01E80000(data)"


#define CONST_ENV_LOADING "load_const=mtd read constants ${loadaddr} 0 0x40000\0"

/* Space separated list of variables that can be **safely** imported from the
 * constants partition into U-Boot environment. */
#define ALLOWED_CONST_VARIABLES "carrierboard"

/* The environment (const partition) written by fw_printenv is in binary mode */
#define CONST_ENV_IMPORTING "import_const=env import -b ${loadaddr} - " ALLOWED_CONST_VARIABLES "\0"

#define CONFIG_EXTRA_ENV_SETTINGS					\
	"fastbootaddr="__stringify(CONFIG_FASTBOOT_BUF_ADDR)"\0"	\
	"loadaddr="__stringify(CONFIG_FIT_ADDR)"\0"			\
	"setfitconfig=run load_const; run import_const; setenv fit_config ${module_config}_${carrierboard}\0" \
	"mfg_run=run kernel_common_args; bootm ${fastbootaddr}#default_factory@1\0" \
	SUE_FWUPDATE_EXTRA_ENV_SETTINGS \
	CONST_ENV_LOADING \
	CONST_ENV_IMPORTING

#define CONFIG_BOOTCOMMAND	"run setfitconfig; " SUE_FWUPDATE_BOOTCOMMAND

/*
 * Miscellaneous configurable options
 */
#define CONFIG_SYS_CBSIZE		2048		/* Console I/O Buffer Size   */

/* Print Buffer Size */
#define CONFIG_SYS_PBSIZE 		(CONFIG_SYS_CBSIZE+sizeof(CONFIG_SYS_PROMPT)+16)
#define CONFIG_SYS_MAXARGS		64			/* max number of command args */

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN           (CONFIG_ENV_SIZE + (2 << 20))
#define CONFIG_SYS_BOOTPARAMS_LEN	(32 << 10)
#define CONFIG_SYS_HZ_CLOCK		27000000

#define CONFIG_SYS_SDRAM_BASE		0x60800000     /* Cached addr, should be 1MB-aligned */

#define CONFIG_SYS_INIT_SP_ADDR		0x609c0000     /* stack */
#define CONFIG_SYS_LOAD_ADDR		0x60800000     /* default load address */

#define CONFIG_SYS_MEMTEST_START	//0x60200000
#define CONFIG_SYS_MEMTEST_END		//0x60230000

/*-----------------------------------------------------------------------
 * Machine type: machine id for RLXARM is 100
 */
#define CONFIG_MACH_TYPE		100

/*-----------------------------------------------------------------------
 * FLASH and environment organization
 */
#define CONFIG_SYS_MAX_FLASH_BANKS	1		/* max number of memory banks */
#define CONFIG_SYS_MAX_FLASH_SECT	(128)		/* max number of sectors on one chip */

/* The following #defines are needed to get flash environment right */
#define CONFIG_SYS_MONITOR_BASE		CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_MONITOR_LEN		(128 << 10)

#define CONFIG_SYS_INIT_SP_OFFSET	0x2000000

/* We boot from this flash, selected with dip switch */
#define CONFIG_SYS_FLASH_BASE		0xbfc00000

/* timeout values are in ticks */
#define CONFIG_SYS_FLASH_ERASE_TOUT	(2 * CONFIG_SYS_HZ) /* Timeout for Flash Erase */
#define CONFIG_SYS_FLASH_WRITE_TOUT	(2 * CONFIG_SYS_HZ) /* Timeout for Flash Write */

#define CONFIG_NET_MULTI
#define CONFIG_MEMSIZE_IN_BYTES

/*-----------------------------------------------------------------------
 * uboot Configuration
 */

#define CONFIG_GZIP		1
#define CONFIG_ZLIB		1
#define CONFIG_PARTITIONS	1

#define CONFIG_SYS_DEVICE_NULLDEV 1

#endif /* _AMEBA_H_ */
