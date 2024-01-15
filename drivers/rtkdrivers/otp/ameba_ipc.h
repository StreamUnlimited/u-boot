/* SPDX-License-Identifier:  GPL-2.0-or-later */
/*
 * Derived from many drivers using ameba IPC device.
 *
 * Copyright (C) 2020 Realsil <andrew_su@realsil.com.cn>
 *
 * RTK IPC(Inter Process Communication) driver for Ameba IPC.
 *
 */
#ifndef __AMEBA_IPC_H__
#define __AMEBA_IPC_H__
/* -------------------------------- Includes -------------------------------- */
/* external head files */
#include <linux/kernel.h>

/* internal head files */

/* -------------------------------- Defines --------------------------------- */
/* port number */
#define AIPC_PORT_NP (0) /* port for NP(KM4) */
#define AIPC_PORT_LP (1) /* port for LP(KM0) */

#define AIPC_CH_MAX_NUM (8)
#define AIPC_NOT_ASSIGNED_CH (AIPC_CH_MAX_NUM + 1)
#define AIPC_CH_IS_ASSIGNED (AIPC_NOT_ASSIGNED_CH + 1)
/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */
struct aipc_ch;
/*
 * structure to describe the ipc message.
 * it's original defination in freertos is below:
 * typedef struct _IPC_MSG_STRUCT_ {
 * 	u32 msg_type;
 * 	u32 msg;
 * 	u32 msg_len;
 * 	u32 rsvd;
 *} IPC_MSG_STRUCT, *PIPC_MSG_STRUCT;
 * don't use typedef in linux advice.
 */
typedef struct ipc_msg_struct {
	u32 msg_type;
	u32 msg;
	u32 msg_len;
	u32 rsvd;
} ipc_msg_struct_t;

/*
 * This structure describes all the operations that can be done on the IPC
 * channel.
 * @channel_recv: The interrupt function for relactive registed channel. The
 *	pmsg is the ipc message from peer.
 * @channel_empty_indicate: The IPC empty interrupt function for relactive
 *	registed channel, If the channel is configured to AIPC_CONFIG_HANDSAKKE.
 *	It means that the interrupt ISR has been clean in other peer.
 */
struct aipc_ch_ops {
	u32 (*channel_recv)(struct aipc_ch *ch, struct ipc_msg_struct *pmsg);
	void (*channel_empty_indicate)(struct aipc_ch *ch);
};

/*
 * This enumeration describes the configuration for IPC channel.
 * @AIPC_CONFIG_NOTHING no configure for this channel, use default
 * 	configutation.
 * @AIPC_CONFIG_HANDSAKKE configure the handshake for send message. If to
 * 	configure this option, need to use ameba_ipc_channel_send_wait. otherwise
 * 	to use ameba_ipc_channel_send.
 */
typedef enum aipc_ch_config {
	AIPC_CONFIG_NOTHING = (unsigned int)0x00000000,
	AIPC_CONFIG_HANDSAKKE = (unsigned int)0x00000001
} aipc_ch_config_t;

/*
 * structure to describe device reigisting channel, and it will be associated
 *	to a struct aipc_c.
 */
typedef struct aipc_ch {
	struct udevice *pdev; /* to get the udevice for ipc device */
	u32 port_id; /* port id for NP or LP */
	u32 ch_id; /* channel number */
	enum aipc_ch_config ch_config; /* configuration for channel */
	struct aipc_ch_ops *ops; /* operations for channel */
	void *priv_data; /* generic platform data pointer */
} aipc_ch_t;

#endif /* __AMEBA_IPC_H__ */
