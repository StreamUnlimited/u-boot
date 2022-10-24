/* Memory sub-system initialization code */

#include <config.h>
#include <linux/linkage.h>
#include "bspchip.h"


	.text
	.globl s_init
/************************************************************************
 *  bsp_boot_init_plat
 ************************************************************************/

ENTRY(lowlevel_init)
#if BSP_USE_CCI_BUS == 1
	ldr	r0, =BSP_CCI_SLAVE0
	ldr	r1, =CCI_ENABLE_REQ
	str	r1, [r0]

	ldr	r0, =BSP_CCI_STATUS
1:	ldr	r1, [r0]
	tst	r1, #0x1
	bne	1b

	ldr     r0, =BSP_CCI_SLAVE1
        ldr     r1, =CCI_ENABLE_REQ
        str     r1, [r0]

        ldr     r0, =BSP_CCI_STATUS
2:      ldr     r1, [r0]
        tst     r1, #0x1
        bne     2b
#endif

/* amebad2 platform is a7, no need use ifdef */
//#ifdef CONFIG_SOC_CPU_ARMA7
	/* Enable SMP bit for core 0 */
	mrc     p15, 0, r0, c1, c0, 1   @ read ACTLR
	orr     r0, r0, #1 << 6         @ set SMP bit (#6)
	mcr     p15, 0, r0, c1, c0, 1   @ write ACTLR
	dsb
	isb
//#endif

#ifdef CONFIG_ARCH_CPU_ARMA53
	/* Enable SMP bit for core 0 */
	mrrc	p15, 1, r0, r1, c15
	tst	r0, #(1 << 6)
	orreq	r0, #(1 << 6)
	mcrreq	p15, 1, r0, r1, c15
#endif

#if BSP_USE_CORESIGHT_TIMESTAMP == 1
	/*
	 * Setup and initialize coresight timestamp
	 *
	 * ARMv7a generic timer, which is built in A7/A15, requires
	 * a reference clock counter source. In Ameba platform, we
	 * use Corsight timestamp module.
	 */
	ldr	r0, =TSGEN_CNTFID0
	ldr	r1, =TSGEN_FREQ
	str	r1, [r0]
	dsb
	ldr	r0, =TSGEN_CNTCR
	ldr	r1, =TSGEN_ENABLE
	str	r1, [r0]
	dsb
#endif
	mov	pc, lr
ENDPROC(lowlevel_init)
