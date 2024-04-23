// SPDX-License-Identifier: GPL-2.0-only

#include <asm/io.h>
#include <common.h>

/* Registers Definitions --------------------------------------------------------*/
/**************************************************************************//**
 * @defgroup WDG_Register_Definitions WDG Register Definitions
 * @{
 *****************************************************************************/
#define WDG_MKEYR			0x000
#define WDG_CR				0x004
#define WDG_RLR				0x008
#define WDG_WINR			0x00C

/**************************************************************************//**
 * @defgroup WDG_REG
 * @{
 *****************************************************************************/
#define WDG_BIT_ENABLE			((u32)0x00000001 << 16)
#define WDG_BIT_CLEAR			((u32)0x00000001 << 24)
#define WDG_BIT_RST_MODE		((u32)0x00000001 << 30)
#define WDG_BIT_ISR_CLEAR		((u32)0x00000001 << 31)
/** @} */
/** @} */

/**************************************************************************//**
 * @defgroup WDG_MKEYR
 * @brief WDG Magic Key register
 * @{
 *****************************************************************************/
#define WDG_MASK_MKEY			((u32)0x0000FFFF << 0)          /*!<R/WPD 0h  0x6969: enable access to register WDG_CR/WDG_RLR/WDG_WINR 0x5A5A: reload WDG counter 0x3C3C: enable WDG function */
#define WDG_MKEY(x)			((u32)(((x) & 0x0000FFFF) << 0))
#define WDG_GET_MKEY(x)			((u32)(((x >> 0) & 0x0000FFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup WDG_CR
 * @brief WDG Control regsietr
 * @{
 *****************************************************************************/
#define WDG_BIT_RVU			((u32)0x00000001 << 31)          /*!<R 0h  Watchdog counter update by reload value */
#define WDG_BIT_EVU			((u32)0x00000001 << 30)          /*!<R 0h  Watchdog early interrupt function update */
#define WDG_BIT_LPEN			((u32)0x00000001 << 24)          /*!<R/WE 0h  Low power enable 0: WDG will gating when system goes into sleep mode 1: WDG keep running when system goes into sleep mode */
#define WDG_BIT_EIC			((u32)0x00000001 << 17)          /*!<WA0 0h  Write '1' clear the early interrupt */
#define WDG_BIT_EIE			((u32)0x00000001 << 16)          /*!<R/WE 0h  Watchdog early interrupt enable */
#define WDG_MASK_early_int_cnt		((u32)0x0000FFFF << 0)          /*!<R/WE 0h  Early interrupt trigger threshold */
#define WDG_early_int_cnt(x)		((u32)(((x) & 0x0000FFFF) << 0))
#define WDG_GET_early_int_cnt(x)	((u32)(((x >> 0) & 0x0000FFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup WDG_RLR
 * @brief WDG Relaod register
 * @{
 *****************************************************************************/
#define WDG_MASK_PRER			((u32)0x000000FF << 16)          /*!<R/WE 63h  Prescaler counter, configuration only allowed before wdg enable WDG: 0x63 System wdg: 0x1F */
#define WDG_PRER(x)			((u32)(((x) & 0x000000FF) << 16))
#define WDG_GET_PRER(x)			((u32)(((x >> 16) & 0x000000FF)))
#define WDG_MASK_RELOAD			((u32)0x0000FFFF << 0)          /*!<R/WE FFFh  Reload value for watchdog counter */
#define WDG_RELOAD(x)			((u32)(((x) & 0x0000FFFF) << 0))
#define WDG_GET_RELOAD(x)		((u32)(((x >> 0) & 0x0000FFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup WDG_WINR
 * @brief WDG window Register
 * @{
 *****************************************************************************/
#define WDG_MASK_WINDOW			((u32)0x0000FFFF << 0)          /*!<R/WE FFFFh  Watchdog feed protect window register */
#define WDG_WINDOW(x)			((u32)(((x) & 0x0000FFFF) << 0))
#define WDG_GET_WINDOW(x)		((u32)(((x >> 0) & 0x0000FFFF)))
/** @} */

/** @defgroup WDG_magic_key_define
  * @{
  */
#define WDG_ACCESS_EN			0x00006969
#define WDG_FUNC_EN			0x00003C3C
#define WDG_REFRESH			0x00005A5A


#define WDG4_BASE			0x410004C0


static void rtk_wdg_wait_busy_check(void)
{
	u32 times = 0;

	while (readl(WDG4_BASE + WDG_CR) & (WDG_BIT_RVU | WDG_BIT_EVU)) {
		times++;
		if (times > 1000) {
			printf("Check busy timeout CR = 0x%08X\n", readl(WDG4_BASE + WDG_CR) & (WDG_BIT_RVU | WDG_BIT_EVU));
			break;
		}
	}
}

void hw_watchdog_reset(void)
{
	rtk_wdg_wait_busy_check();
	writel(WDG_REFRESH, WDG4_BASE + WDG_MKEYR);
}

void hw_watchdog_init(void)
{
	rtk_wdg_wait_busy_check();

	// Enable access
	writel(WDG_ACCESS_EN, WDG4_BASE + WDG_MKEYR);
	writel(0, WDG4_BASE + WDG_CR);

	// Set prescaler to 0x1f which will create a 1 kHz tick and set the reload
	// value directly to the timeout value in milliseconds.
	writel(WDG_PRER(0x1f) | WDG_RELOAD(CONFIG_WATCHDOG_TIMEOUT_MSECS), WDG4_BASE + WDG_RLR);

	// Disable window function
	writel(WDG_WINDOW(0xFFFF), WDG4_BASE + WDG_WINR);

	// Disable access
	writel(0xFFFF, WDG4_BASE + WDG_MKEYR);

	// Start the watchdog
	writel(WDG_FUNC_EN, WDG4_BASE + WDG_MKEYR);
	printf("Watchdog enabled with %u ms timeout\n", CONFIG_WATCHDOG_TIMEOUT_MSECS);
}

