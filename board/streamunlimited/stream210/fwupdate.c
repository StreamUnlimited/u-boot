// SPDX-License-Identifier: GPL-2.0-or-later

#include <common.h>
#include <asm/io.h>
#include <env.h>
#include <command.h>

#include "fwupdate.h"
#include "flags_amebad2.h"

#define FWUP_MAX_BOOT_CNT	9UL

static int fwupdate_getUpdateFlag(bool *val)
{
	return flag_read(FWUP_FLAG_UPDATE_INDEX, val);
}

static int fwupdate_setUpdateFlag(bool val)
{
	return flag_write(FWUP_FLAG_UPDATE_INDEX, val);
}

static int fwupdate_getFailFlag(bool *val)
{
	return flag_read(FWUP_FLAG_FAIL_INDEX, val);
}

static int fwupdate_setFailFlag(bool val)
{
	return flag_write(FWUP_FLAG_FAIL_INDEX, val);
}

static int fwupdate_getBootCount(uint8_t *pBootCnt)
{
	return bootcnt_read(pBootCnt);
}

static int fwupdate_setBootCount(uint8_t bootCnt)
{
	return bootcnt_write(bootCnt);
}

static int fwupdate_getUsbUpdateReq(void)
{
	printf("TODO: Implement fwupdate_getUsbUpdateReq()\n");
	return 0;
}

#if defined(CONFIG_BOOTCOUNT_FWUPDATE)
void bootcount_store(ulong a)
{
	if (fwupdate_setBootCount((uint8_t)a))
		printf("ERROR: fwupdate_setBootCount() failed!\n");
	else
		printf("BOOTCOUNT is %ld\n", a);
}

ulong bootcount_load(void)
{
	uint8_t bootcount = 0xFF;

	if (fwupdate_getBootCount(&bootcount))
		printf("ERROR: getBootCount() failed!\n");

	return bootcount;
}
#endif /* CONFIG_BOOTCOUNT_FWUPDATE */

int fwupdate_init()
{
	return 0;
}

/*
 * Returns 0 on success or 1 on error.
 */
static int do_fwup_writeflag(char *flag, bool val)
{
	if (!strcmp(flag, "update")) {
		if (fwupdate_setUpdateFlag(val))
			return 1;
	} else if (!strcmp(flag, "fail")) {
		if (fwupdate_setFailFlag(val))
			return 1;
	} else {
		printf("ERROR: unknown flag '%s' specified\n", flag);
		return 1;
	}

	return 0;
}

static int do_fwup_flags(void)
{
	bool failFlag = 0;
	bool updateFlag = 0;
	u8 bootCount = 0;

	fwupdate_getFailFlag(&failFlag);
	fwupdate_getUpdateFlag(&updateFlag);
	fwupdate_getBootCount(&bootCount);

	printf("INFO: flags: bootcount: %u, fail: %u, update: %u\n", bootCount, failFlag, updateFlag);

	return 0;
}

static int do_fwup_update(void)
{
	bool update;

	if (fwupdate_getUpdateFlag(&update)) {
		/*
		 * NOTE: because this command returns 1 if no update is requested
		 * and it also returns 1 if the above call lead to an error, we try
		 * to let the user at least know that an error occured.
		 */
		printf("ERROR: failed to get update flag\n");
		return 1;
	}

	return update ? 0 : 1;
}

static int do_fwup_fail(void)
{
	bool fail;

	if (fwupdate_getFailFlag(&fail)) {
		printf("ERROR: failed to get fail flag\n");
		return 1;
	}

	return fail ? 0 : 1;
}

static int do_fwup_incbootcnt(void)
{
	uint8_t bootcnt;

	if (fwupdate_getBootCount(&bootcnt))
		return 1;

	if (fwupdate_setBootCount(bootcnt + 1))
		return 1;

	return 0;
}

/*
 * Returns 1 (error) if the bootcount has reached the bootlimit
 */
static int do_fwup_bootcnt(void)
{
	uint8_t bootcnt;
	ulong bootmax;

	if (fwupdate_getBootCount(&bootcnt))
		return 1;

	bootmax = env_get_ulong("bootlimit", 10, FWUP_MAX_BOOT_CNT);

	return (bootcnt < bootmax) ? 0 : 1;
}

static int do_fwup(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	char *cmd = NULL;

	/* at least two arguments please */
	if (argc < 2)
		goto usage;

	cmd = argv[1];

	/*
	 * Syntax is:
	 *   0    1     2
	 *   fwup clear/set flag
	 */
	if (!strcmp(cmd, "clear")) {
		if (argc != 3)
			goto usage;

		return do_fwup_writeflag(argv[2], false);
	}

	if (!strcmp(cmd, "set")) {
		if (argc != 3)
			goto usage;

		return do_fwup_writeflag(argv[2], true);
	}

	if (!strcmp(cmd, "flags")) {
		return do_fwup_flags();
	}


	/*
	 * Syntax is:
	 *   0    1
	 *   fwup update
	 */
	if (!strcmp(cmd, "update")) {
		return do_fwup_update();
	}

	/*
	 * Syntax is:
	 *   0    1
	 *   fwup usb_update_req
	 */
	if (!strcmp(cmd, "usb_update_req")) {
		/*
		 * fwupdate_getUsbUpdateReq() return 1 if we want to
		 * request an update and 0 otherwise. So we want the
		 * command in u-boot to fail if no update request is
		 * set and to succeed if an update request is pending.
		 * Thats why we need to invert the result here.
		 */
		return !fwupdate_getUsbUpdateReq();
	}

	/*
	 * Syntax is:
	 *   0    1
	 *   fwup fail
	 */
	if (!strcmp(cmd, "fail")) {
		return do_fwup_fail();
	}

	/*
	 * Syntax is:
	 *   0    1
	 *   fwup incbootcnt
	 */
	if (!strcmp(cmd, "incbootcnt")) {
		return do_fwup_incbootcnt();
	}

	/*
	 * Syntax is:
	 *   0    1
	 *   fwup bootcnt
	 */
	if (strcmp(cmd, "bootcnt") == 0) {
		return do_fwup_bootcnt();
	}

usage:
	return cmd_usage(cmdtp);
}

U_BOOT_CMD(fwup, CONFIG_SYS_MAXARGS, 1, do_fwup,
		"Streamunlimited firmware update",
		"clear flag - clears the user requested flag\n"
		"fwup flags      - print current flags\n"
		"fwup set flag   - sets the user requested flag\n"
		"fwup update     - checks if update flag is set\n"
		"fwup usb_update_req - checks if USB update request active\n"
		"fwup fail       - checks if fail flag is set\n"
		"fwup incbootcnt - increments boot count\n"
		"fwup bootcnt    - checks if boot count is less than maximum allowed\n"
		);


