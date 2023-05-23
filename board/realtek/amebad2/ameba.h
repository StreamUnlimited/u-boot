/*
 * Realtek Semiconductor Corp.
 *
 * Copyright 2012  Jethro Hsu (jethro@realtek.com)
 * Copyright 2012  Tony Wu (tonywu@realtek.com)
 */

#ifndef _AMEBA_H_
#define _AMEBA_H_

/* valid baudrates */
#define CONFIG_SYS_BAUDRATE_TABLE	{ 9600, 19200, 38400, 57600, 115200 , 1500000}

#define CONFIG_TIMESTAMP				/* Print image info with timestamp */
#undef  CONFIG_BOOTARGS

#define CONFIG_EXTRA_ENV_SETTINGS					\
	"addmisc=setenv bootargs ${bootargs} "				\
		"console=ttyS0,${baudrate} "				\
		"panic=1\0"						\
	"bootfile=/vmlinux.elf\0"					\
	"fdt_high=0xffffffff\0"						\
	"load=tftp 80500000 ${u-boot}\0"				\
	""

#define KERNEL_ADDR			__stringify(CONFIG_KERNEL_ADDR)
#define RAMDISK_ADDR			__stringify(CONFIG_RAMDISK_ADDR)
#define FDT_ADDR			__stringify(CONFIG_FDT_ADDR)
#define FIT_ADDR			__stringify(CONFIG_FIT_ADDR)
#define IMG_FLASH_BASE			__stringify(CONFIG_IMG_FLASH_ADDR)
#define IMG_FLASH_SIZE			__stringify(CONFIG_IMG_FLASH_SIZE)
#define FDT_FLASH_BASE			__stringify(CONFIG_FDT_FLASH_ADDR)
#define FDT_FLASH_SIZE			__stringify(CONFIG_FDT_FLASH_SIZE)
#define RECOVERY_IMG_FLASH_SIZE		__stringify(CONFIG_RECOVERY_IMG_FLASH_SIZE)
#define RECOVERY_FDT_FLASH_SIZE		__stringify(CONFIG_RECOVERY_FDT_FLASH_SIZE)
#define RECOVERY_IMG_FLASH_BASE		__stringify(CONFIG_RECOVERY_IMG_FLASH_ADDR)
#define RECOVERY_FDT_FLASH_BASE		__stringify(CONFIG_RECOVERY_FDT_FLASH_ADDR)
#define AP_MISC_ADDR			__stringify(CONFIG_BOOT_OPTION_MISC_FLASH_ADDR)
#define AP_MISC_SIZE			__stringify(CONFIG_BOOT_OPTION_MISC_FLASH_SIZE)

#if defined(CONFIG_CMD_SF)
#if defined(CONFIG_FIT)
#define _BOOTCOMMAND(a, b, c)		"sf probe 0:2;"			\
					"sf read "a" "b" "c";"		\
					"bootm "a""

#define CONFIG_BOOTCOMMAND		_BOOTCOMMAND(			\
						FIT_ADDR,		\
						IMG_FLASH_BASE,		\
						IMG_FLASH_SIZE)
#else
#define _BOOTCOMMAND(a, b, c, d, e, f, g, h, i, j, k, l, m)		"sf probe 0:2;"		\
						"sf opt_read "a" "b" "c" "d" "e" "f" "g" "h" "i" "j" "k" "l";"\
						"bootm "c" "m" "d

#define CONFIG_BOOTCOMMAND		_BOOTCOMMAND(			\
						AP_MISC_ADDR,		\
						AP_MISC_SIZE,		\
						KERNEL_ADDR,		\
						FDT_ADDR,				\
						IMG_FLASH_BASE,		\
						IMG_FLASH_SIZE,		\
						FDT_FLASH_BASE,		\
						FDT_FLASH_SIZE,		\
						RECOVERY_IMG_FLASH_BASE,		\
						RECOVERY_IMG_FLASH_SIZE,		\
						RECOVERY_FDT_FLASH_BASE,		\
						RECOVERY_FDT_FLASH_SIZE,		\
						"-")
#endif
#elif defined(CONFIG_MTD_SPI_NAND)
#if defined(CONFIG_FIT)
#error "FIT for NAND not supported yet"
#else
#define _BOOTCOMMAND(a, b, c, d, e, f, g)	"mtd opt_read "a" "b" "c" "d" "e" "f" "g";"\
											"bootm "b" "a" "c";"

#define CONFIG_BOOTCOMMAND		_BOOTCOMMAND(			\
						"-",		\
						KERNEL_ADDR,		\
						FDT_ADDR,		\
						IMG_FLASH_SIZE,		\
						FDT_FLASH_SIZE,		\
						RECOVERY_IMG_FLASH_SIZE,\
						RECOVERY_FDT_FLASH_SIZE)
#endif
#else
#if defined(CONFIG_FIT)
#define _BOOTCOMMAND(a, b, c)		"bootm "a""
#define CONFIG_BOOTCOMMAND		_BOOTCOMMAND(			\
						FIT_ADDR,		\
						"-",		\
						"-")
#else
#define _BOOTCOMMAND(a, b, c)		"bootm "a" "b" "c
#define CONFIG_BOOTCOMMAND		_BOOTCOMMAND(			\
						KERNEL_ADDR,		\
						RAMDISK_ADDR,		\
						FDT_ADDR)
#endif
#endif

/*
 * Miscellaneous configurable options
 */
#undef CONFIG_SYS_LONGHELP					/* undef to save memory      */
#define CONFIG_SYS_CBSIZE		1024		/* Console I/O Buffer Size   */

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

#define CONFIG_ENV_IS_NOWHERE		1

/* Address and size of Primary Environment Sector	*/
#define CONFIG_ENV_ADDR			0x60070000
#define CONFIG_NET_MULTI
#define CONFIG_MEMSIZE_IN_BYTES

/*-----------------------------------------------------------------------
 * uboot Configuration
 */

/* Support bootm-ing different OSes */
#define CONFIG_BOOTM_LINUX	1

#define CONFIG_GZIP		1
#define CONFIG_ZLIB		1
#define CONFIG_PARTITIONS	1

#endif /* _AMEBA_H_ */
