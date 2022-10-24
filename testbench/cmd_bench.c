/*
 * Realtek Semiconductor Corp.
 *
 * cmd_bench.c
 *
 * Copyright 2014 Jethro Hsu (jethro@realtek.com)
 */

#include <common.h>
#include <command.h>

DECLARE_GLOBAL_DATA_PTR;

struct testbench {
	u32 (*func) (u32 memlimit);	/* callback function */
	char *name;			/* testbench name */
};

extern u32 ca7_max_power(u32, u32);

static struct testbench testbench_table[] = {
	{ ca7_max_power, "ca7_max_power"},
	{ },
};

static int do_testbench(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	u32 rev;
	int i;

	/* Before jumping to test bench, disable cached and mmu */
#ifndef CONFIG_SYS_ICACHE_OFF
	icache_disable();
#endif
#ifndef CONFIG_SYS_DCACHE_OFF
	dcache_disable();
#endif

	for (i = 0; testbench_table[i].func; i++) {
		if (!(strcmp(argv[1], testbench_table[i].name))) {
			printf("excuting %s...\n", testbench_table[i].name);
			rev = testbench_table[i].func(gd->ram_size + CONFIG_SYS_SDRAM_BASE);
			printf("return value = %u\n", rev);
		}
	}

#ifndef CONFIG_SYS_ICACHE_OFF
	icache_disable();
#endif
#ifndef CONFIG_SYS_DCACHE_OFF
	dcache_disable();
#endif

	return 0;
}

U_BOOT_CMD(
	testbench,	CONFIG_SYS_MAXARGS,	1,	do_testbench,
	"do testbench prebuilt",
	"[args..]"
);
