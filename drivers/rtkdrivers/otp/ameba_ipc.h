// SPDX-License-Identifier: GPL-2.0+
/*
* Realtek IPC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef __AMEBA_IPC_H__
#define __AMEBA_IPC_H__
/* -------------------------------- Includes -------------------------------- */
/* external head files */
#include <linux/kernel.h>
#include <linux/compat.h>
#include <linux/completion.h>

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
struct ipc_msg_struct {
	u32 msg_type;
	u32 msg;
	u32 msg_len;
	u32 rsvd;
};

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
enum aipc_ch_config {
	AIPC_CONFIG_NOTHING = (unsigned int)0x00000000,
	AIPC_CONFIG_HANDSAKKE = (unsigned int)0x00000001
};

/*
 * structure to describe device reigisting channel, and it will be associated
 *	to a struct aipc_c.
 */
struct aipc_ch {
	struct udevice *pdev; /* to get the udevice for ipc device */
	u32 port_id; /* port id for NP or LP */
	u32 ch_id; /* channel number */
	enum aipc_ch_config ch_config; /* configuration for channel */
	struct aipc_ch_ops *ops; /* operations for channel */
	void *priv_data; /* generic platform data pointer */
};

struct aipc_device {
	spinlock_t lock; /* list lock */
	struct udevice *dev; /* parent device */
	struct aipc_port *pnp_port; /* pointer to NP port */
	struct aipc_port *plp_port; /* pointer to LP port */
	struct ipc_msg_struct *dev_mem; /* LP shared SRAM for IPC */
	u32* preg_tx_data; /* mapping address of AIPC_REG_TX_DATA */
	u32* preg_rx_data; /* mapping address of AIPC_REG_RX_DATA */
	u32* preg_isr; /* mapping address of AIPC_REG_ISR */
	u32* preg_imr; /* mapping address of AIPC_REG_IMR */
	u32* preg_icr; /* mapping address of AIPC_REG_ICR */
	u32 irq; /* irq number */
	unsigned long irqflags; /* irq flags */
};

struct aipc_port {
	struct list_head ch_list; /* channel list */
	struct aipc_device *dev; /* parent device */
	u32 port_id; /* port id for NP or LP */
	const char *name; /* port name */
	spinlock_t lock; /* port lock */
	u32 free_chnl_num; /* free channel number in port */
};

struct aipc_ch_node {
	struct list_head list; /* list to add the channel list */
	struct aipc_port *port; /* pointer to ipc port */
	struct ipc_msg_struct *ch_rmem; /* channel read shared SRAM for IPC */
	struct ipc_msg_struct *ch_wmem; /* channel written shared SRAM for IPC */
	struct aipc_ch *ch; /* customer regisiting channel */
};

struct aipc_ch *ameba_ipc_alloc_ch(int size_of_priv);
int ameba_ipc_poll(struct aipc_device *pipc_dev);
int ameba_ipc_channel_send(struct udevice *dev, struct aipc_ch *ch, struct ipc_msg_struct *pmsg, struct aipc_device *pipc_dev);
int ameba_ipc_channel_register(struct udevice *dev, struct aipc_ch *ch, struct aipc_device *pipc_dev);
int ameba_ipc_channel_unregister(struct udevice *dev, struct aipc_ch *ch, struct aipc_device *pipc_dev);
int ameba_ipc_probe(struct udevice *dev, struct aipc_device *pipc_dev);

#endif /* __AMEBA_IPC_H__ */
