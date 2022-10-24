/*
 * Realtek Semiconductor Corp.
 *
 * taroko-cm.S:
 *	Taroko coherence manager setup
 *
 * Copyright 2006-2015  PSP Software Group
 */
#include <asm/smp-boot.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>
#include <config.h>
#include <asm/mmcr.h>

	.text

	.globl mips_cm_setup
	.ent mips_cm_setup
	.set push
	.set noreorder
mips_cm_setup:
	/* change MMCR from non-coherent to coherent */
	li	t2, MMCR_BASE

	li	t1, 0x5 << MMCR_SHF(SMMU_CCA, KSEG0)
	lw	t3, MMCR_SMMU_CCA_OFS(t2)
	or	t3, t1
	sw	t3, MMCR_SMMU_CCA_OFS(t2)

	/* Set all cores to coherent state */
	lw	t0, MMCR_CORE_OFS(t2)
	andi	t0, 0xf << MMCR_SHF(CORE, NUMCORES)
	srl	t0, MMCR_SHF(CORE, NUMCORES)

	li	t1, 0x1 << MMCR_SHF(CORESTATE, COHERENT)

	lw	t3, MMCR_CORESTATE0_OFS(t2)
	or	t3, t1
	sw	t3, MMCR_CORESTATE0_OFS(t2)
	addi	t0, -1
	beqz	t0, 1f
	 nop

	lw	t3, MMCR_CORESTATE1_OFS(t2)
	or	t3, t1
	sw	t3, MMCR_CORESTATE1_OFS(t2)
	addi	t0, -1
	beqz	t0, 1f
	 nop

	lw	t3, MMCR_CORESTATE2_OFS(t2)
	or	t3, t1
	sw	t3, MMCR_CORESTATE2_OFS(t2)
	addi	t0, -1
	beqz	t0, 1f
	 nop

	lw	t3, MMCR_CORESTATE3_OFS(t2)
	or	t3, t1
	sw	t3, MMCR_CORESTATE3_OFS(t2)
1:
	jr	ra
	 nop
	.set pop
	.end mips_cm_setup
