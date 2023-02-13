/*
 * fwupdate.c
 *
 * Copyright (C) 2016, StreamUnlimited Engineering GmbH, http://www.streamunlimited.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR /PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <common.h>
#include <asm/errno.h>
#include <asm/io.h>

#include "fwupdate.h"
#include "flags_a113d.h"

#define FWUP_MAX_BOOT_CNT	9UL

static const struct sue_device_info *current_device;

static int fwupdate_getUpdateFlag(uint8_t *pUpdateFlag)
{
	return flag_read(FWUP_FLAG_UPDATE_INDEX, pUpdateFlag);
}

static int fwupdate_setUpdateFlag(uint8_t updateFlag)
{
	return flag_write(FWUP_FLAG_UPDATE_INDEX, updateFlag);
}

static int fwupdate_getFailFlag(uint8_t* pFailFlag)
{
	return flag_read(FWUP_FLAG_FAIL_INDEX, pFailFlag);
}

static int fwupdate_setFailFlag(uint8_t failFlag)
{
	return flag_write(FWUP_FLAG_FAIL_INDEX, failFlag);
}

static int fwupdate_getBootCount(uint8_t* pBootCnt)
{
	return bootcnt_read(pBootCnt);
}

static int fwupdate_setBootCount(uint8_t bootCnt)
{
	return bootcnt_write(bootCnt);
}

static int fwupdate_getUsbUpdateReq(void)
{
	int ret;
	ret = sue_carrier_get_usb_update_request(current_device);

	if (ret < 0) {
		printf("ERROR: fwupdate_getUsbUpdateReq() failed!\n");
		return 0;
	}

	return ret;
}

#ifdef CONFIG_BOOTCOUNT_LIMIT
void bootcount_store(ulong a)
{
	int status = fwupdate_setBootCount((uint32_t)a);

	if (0 != status)
		printf("ERROR: fwupdate_setBootCount() failed!\n");
	else
		printf("BOOTCOUNT is %ld\n", a);
}

ulong bootcount_load(void)
{
	uint8_t bootcount = 0xFF;

	int status = fwupdate_getBootCount(&bootcount);

	if (0 != status)
		printf("ERROR: getBootCount() failed!\n");

	return bootcount;
}
#endif /* CONFIG_BOOTCOUNT_LIMIT */

int fwupdate_init(const struct sue_device_info *device_info)
{
	int status = 0;
	char* bootlimit;
	char* module_version;

	bootlimit = getenv("bootlimit");
	if (NULL == bootlimit) {
		char buf[16];
		snprintf(buf, sizeof(buf), "%lu", FWUP_MAX_BOOT_CNT);
		setenv("bootlimit", buf);
	}

	current_device = device_info;

	module_version = getenv("module_version");
	if (NULL == module_version) {
		char buf[8];
		snprintf(buf, sizeof(buf), "%u", current_device->module_version);
		setenv("module_version", buf);
	}

	return status;
}

static int do_fwup(cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{
	char*       cmd = NULL;
	char*       fwupflag = NULL;
	uint8_t     update;
	uint8_t     fail;
	uint8_t     bootcnt;
	char*       bootlimit;
	uint32_t    bootmax;

	/* at least two arguments please */
	if (argc < 2)
		goto usage;

	cmd = argv[1];

	/*
	 * Syntax is:
	 *   0    1     2
	 *   fwup clear flag
	 */
	if (strcmp(cmd, "clear") == 0) {
		if (argc != 3)
			goto usage;
		else
			fwupflag = argv[2];

		if (strcmp(fwupflag, "update") == 0) {
			if (0 != fwupdate_setUpdateFlag(0)) {
				return 1;
			}
		} else if (strcmp(fwupflag, "fail") == 0) {
			if (0 != fwupdate_setFailFlag(0)) {
				return 1;
			}
		}
		return 0;
	}

	if (strcmp(cmd, "flags") == 0) {
		uint8_t failFlag = 0;
		uint8_t updateFlag = 0;
		uint8_t bootCount = 0;

		fwupdate_getFailFlag(&failFlag);
		fwupdate_getUpdateFlag(&updateFlag);
		fwupdate_getBootCount(&bootCount);

		printf("INFO: flags: bootcount: %d, fail: %d, update: %d\n", bootCount, failFlag, updateFlag);
		return 0;
	}

	/*
	 * Syntax is:
	 *   0    1   2
	 *   fwup set flag
	 */
	if (strcmp(cmd, "set") == 0) {
		if (argc != 3)
			goto usage;
		else
			fwupflag = argv[2];

		if (strcmp(fwupflag, "update") == 0) {
			if (0 != fwupdate_setUpdateFlag(1)) {
				return 1;
			}
		} else if (strcmp(fwupflag, "fail") == 0) {
			if (0 != fwupdate_setFailFlag(1)) {
				return 1;
			}
		}
		return 0;
	}

	/*
	 * Syntax is:
	 *   0    1
	 *   fwup update
	 */
	if (strcmp(cmd, "update") == 0) {

		if (0 > fwupdate_getUpdateFlag(&update)) {
			return 1;
		} else {
			return (update ? 0 : 1);
		}
		return 0;
	}

	/*
	 * Syntax is:
	 *   0    1
	 *   fwup usb_update_req
	 */
	if (strcmp(cmd, "usb_update_req") == 0) {
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
	if (strcmp(cmd, "fail") == 0) {

		if (0 > fwupdate_getFailFlag(&fail)) {
			return 1;
		} else {
			return (fail ? 0 : 1);
		}
		return 0;
	}

	/*
	 * Syntax is:
	 *   0    1
	 *   fwup incbootcnt
	 */
	if (strcmp(cmd, "incbootcnt") == 0) {

		if (0 > fwupdate_getBootCount(&bootcnt)) {
			return 1;
		} else if (0 > fwupdate_setBootCount(bootcnt+1)) {
			return 1;
		}
		return 0;
	}

	/*
	 * Syntax is:
	 *   0    1
	 *   fwup bootcnt
	 */
	if (strcmp(cmd, "bootcnt") == 0) {

		if (0 > fwupdate_getBootCount(&bootcnt)) {
			return 1;
		} else {
			bootlimit = getenv("bootlimit");
			if (NULL == bootlimit) {
				bootmax = FWUP_MAX_BOOT_CNT;
			} else {
				bootmax = simple_strtoul(bootlimit, NULL, 10);
			}
		}
		return ((bootcnt < bootmax) ? 0 : 1);
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


