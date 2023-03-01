// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef __FWUPDATE_H__
#define __FWUPDATE_H__

#define FWUP_FLAG_UPDATE_INDEX	0
#define FWUP_FLAG_FAIL_INDEX	1
#define FWUP_FLAG_SWRESET_REQ	2	/* This flag will be set when we request a reset using the `reset` command */

int fwupdate_init(void);

#endif /* __FWUPDATE_H__ */
