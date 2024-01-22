/*
 * Realtek Semiconductor Corp.
 *
 * bsp.h
 * Board Support Package header file
 *
 * Tony Wu (tonywu@realtek.com.tw)
 * Dec. 07, 2007
 */

#ifndef  _BSPCHIP_H_
#define  _BSPCHIP_H_

/*
 *****************************************************************************************
 * DEFINITIONS AND TYPES
 *****************************************************************************************
 */

/* Register Macro */
#ifndef REG32
#define REG32(reg)      (*(volatile u32 *)(reg))
#endif
#ifndef REG16
#define REG16(reg)      (*(volatile u16 *)(reg))
#endif
#ifndef REG8
#define REG8(reg)       (*(volatile u8  *)(reg))
#endif

/*
 *****************************************************************************************
 * System control registers
 *****************************************************************************************
 */
#define OTPC_REG_BASE				0x42000000
#define SYSTEM_CTRL_BASE_LP			0x42008000
#define REG_LSYS_OTP_SYSCFG0			0x0230
#define REG_LSYS_SCAN_CTRL			0x0274
#define REG_LSYS_SYSTEM_CFG0			0x027C
#define SEC_OTP_SYSCFG0				0x0100
#define SEC_CFG2				0x0368


#define SYSCFG_OTP_BOOTNOR			2
#define SYSCFG_OTP_BOOTNAND			1

#define LSYS_GET_BOOT_SELECT(x)			((u32)(((x >> 20) & 0x00000003)))
#define LSYS_GET_PTRP_BOOTSEL(x)		((u32)(((x >> 20) & 0x00000001)))

#define LSYS_MASK_CHIP_INFO_EN			((u32)0x0000000F << 28)
#define LSYS_CHIP_INFO_EN(x)			((u32)((x) & 0x0000000F) << 28)
#define LSYS_GET_RL_VER(x)			((u32)(((x >> 16) & 0x0000000F)))

#define SEC_BIT_LOGIC_SECURE_BOOT_EN		((u32)0x00000001 << 26)
#define SEC_BIT_SECURE_BOOT_EN			((u32)0x00000001 << 3)


/*
 *****************************************************************************************
 * Timer
 *****************************************************************************************
 */
#if 0
/* DWC timer */
#define TIMER0_IOBASE		0x1FB01000	/* Timer0 register base address mapping */
#define TIMER0_FREQ		25000000	/* The frequency of timer module        */

/* Register offset from TIMERx_IOBASE */
#define TIMER_TLCR1		0x00
#define TIMER_TCV		0x04
#define TIMER_TCR		0x08
#define TIMER_EOIA		0x0c
#endif
/*
 *****************************************************************************************
 * Time stamp generator
 *****************************************************************************************
 */

#define BSP_USE_CORESIGHT_TIMESTAMP	0

#define TSGEN_BASE      		0x02000000
#define TSGEN_CNTFID0   		(TSGEN_BASE + 0x20)
#define TSGEN_FREQ      		0x017d7840      /* 25 MHz */

#define TSGEN_CNTCR     		(TSGEN_BASE + 0x0)
#define TSGEN_ENABLE    		0x1

/*
 *****************************************************************************************
 * CCI Bus
 *****************************************************************************************
 */

#define BSP_USE_CCI_BUS			0

#define BSP_CCI_BASE			0xf00c0000
#define BSP_CCI_STATUS			(BSP_CCI_BASE + 0xc)

#define BSP_CCI_SLAVE0			(BSP_CCI_BASE + 0x4000)
#define BSP_CCI_SLAVE1			(BSP_CCI_BASE + 0x5000)

#define CCI_ENABLE_SNOOP_REQ		0x1
#define CCI_ENABLE_DVM_REQ		0x2
#define CCI_ENABLE_REQ			(CCI_ENABLE_SNOOP_REQ)

#endif /*  _BSPCHIP_H_ */
