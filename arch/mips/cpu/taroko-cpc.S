/*
 * Realtek Semiconductor Corp.
 *
 * taroko-cpc.S
 *
 * Copyright 2006-2015  PSP Software Group
 */
#include <config.h>

#include <asm/smp-boot.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>
#include <asm/mmcr.h>

	.text

	.globl mips_cpc_setup
	.ent mips_cpc_setup
	.set push
	.set noreorder
mips_cpc_setup:

	/* Set all cores to online state */
	la	t2, MMCR_BASE

	lw	t0, MMCR_CORE_OFS(t2)
	andi	t0, 0xf << MMCR_SHF(CORE, NUMCORES)
	srl	t0, MMCR_SHF(CORE, NUMCORES)
	addi	t0, -1

	li	t1, 0x1 << MMCR_SHF(CORESTATE, ONLINE)

	lw	t3, MMCR_CORESTATE1_OFS(t2)
	addi	t0, -1
	or	t3, t1
	sw	t3, MMCR_CORESTATE1_OFS(t2)
	beqz	t0, 1f
	 nop

	lw	t3, MMCR_CORESTATE2_OFS(t2)
	addi	t0, -1
	or	t3, t1
	sw	t3, MMCR_CORESTATE2_OFS(t2)
	beqz	t0, 1f
	 nop

	lw	t3, MMCR_CORESTATE3_OFS(t2)
	addi	t0, -1
	or	t3, t1
	sw	t3, MMCR_CORESTATE3_OFS(t2)
	beqz	t0, 1f
	 nop
1:
	jr	ra
	 nop
	.set pop
	.end mips_cpc_setup
