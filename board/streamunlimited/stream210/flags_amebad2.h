// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef __FLAGS_AMEBAD2_H__
#define __FLAGS_AMEBAD2_H__

#include <common.h>

int flag_write(u8 index, bool val);
int flag_read(u8 index, bool *val);
int flags_clear(void);

int bootcnt_write(u8 data);
int bootcnt_read(u8 *data);

#endif /* __FLAGS_AMEBAD2_H__ */
