// SPDX-License-Identifier: GPL-2.0+
/*
* Realtek USB PHY support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef _RTK_USB_PHY_H_
#define _RTK_USB_PHY_H_

#include <common.h>

int rtk_phy_calibrate(struct phy *phy, uintptr_t dwc);

#endif

