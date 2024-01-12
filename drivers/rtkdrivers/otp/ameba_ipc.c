// SPDX-License-Identifier: GPL-2.0+
/*
* Realtek IPC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/
	
#include <iotrace.h>
#include <cpu_func.h>
#include <dm.h>
#include <asm/io.h>
#include <dm/devres.h>
#include <dm/read.h>
#include <dm/device_compat.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/ioport.h>
#include <linux/dma-mapping.h>
#include <linux/completion.h>
#include <linux/compat.h>

#include "ameba_ipc.h"
#include "ameba_ipc_reg.h"

#define MAX_NUM_AIPC_PORT (2)
#define TOT_MEM_SZIE (3 * sizeof(struct ipc_msg_struct) * AIPC_CH_MAX_NUM * MAX_NUM_AIPC_PORT)

/* Define the name of IPC port */
const char NAME_OF_NP_PORT[] = "IPC NP port";
const char NAME_OF_LP_PORT[] = "IPC LP port";

static int ameba_ipc_send_work(struct udevice *dev, struct aipc_ch_node *chn, struct ipc_msg_struct *msg)
{
	struct aipc_ch *ch = chn->ch;
	struct aipc_device *pdev = chn->port->dev;
	u32 reg_tx_data = 0;
	int ret = 0;

	reg_tx_data = readl(pdev->preg_tx_data);
	if (ch->port_id == AIPC_PORT_LP) {
		/* Check the TX data */
		if (AIPC_GET_LP_CH_NR(ch->ch_id, reg_tx_data)) {
			dev_err(dev, "LP TX busy!\n");
			reg_tx_data = AIPC_CLR_LP_CH_NR(ch->ch_id, reg_tx_data);
			ret = -EBUSY;
		} else {
			/* Copy data to the lp shared memory */
			memcpy_toio((u8 *)chn->ch_wmem, (u8 *)msg, sizeof(struct ipc_msg_struct));
			reg_tx_data = AIPC_SET_LP_CH_NR(ch->ch_id, reg_tx_data);
		}
	} else if (ch->port_id == AIPC_PORT_NP) {
		/* Check the TX data */
		if (AIPC_GET_NP_CH_NR(ch->ch_id, reg_tx_data)) {
			dev_err(dev, "NP TX busy\n");
			reg_tx_data = AIPC_CLR_NP_CH_NR(ch->ch_id, reg_tx_data);
			ret = -EBUSY;
		} else {
			/* Copy data to the lp shared memory */
			memcpy_toio((u8 *)chn->ch_wmem, (u8 *)msg, sizeof(struct ipc_msg_struct));
			reg_tx_data = AIPC_SET_NP_CH_NR(ch->ch_id, reg_tx_data);
		}
	} else {
		dev_err(dev, "Invalid port id %d\n", ch->port_id);
		ret = -EINVAL;
	}

	writel(reg_tx_data, pdev->preg_tx_data);

	return ret;
}

static u32 ameba_ipc_find_channel_id(struct udevice *dev, struct aipc_port *pport)
{
	u32 id = AIPC_NOT_ASSIGNED_CH;
	struct aipc_ch_node *chn = NULL;

	if (!pport) {
		dev_err(dev, "Invalid port\n");
		goto func_exit;
	}

	/* Assume id is 0 before polling the channel list */
	id = 0;
	list_for_each_entry(chn, &pport->ch_list, list) {
		if (id == chn->ch->ch_id) {
			id++;    /* Find same id, id++ and compare next channel */
		} else {
			goto func_exit;
		}; /* This id is not used, break. */

		if (id >= AIPC_CH_MAX_NUM) {
			id = AIPC_NOT_ASSIGNED_CH;
			dev_err(dev, "No valid channel\n");
			goto func_exit;
		}
	}

func_exit:
	return id;
}

static void ameba_ipc_check_channel_id(struct udevice *dev, struct aipc_port *pport, u32 *pid)
{
	struct aipc_ch_node *chn = NULL;

	if (!pport) {
		dev_err(dev, "Invalid port\n");
		*pid = AIPC_NOT_ASSIGNED_CH;
		return;
	}

	list_for_each_entry(chn, &pport->ch_list, list) {
		if (*pid == chn->ch->ch_id) {
			*pid = AIPC_CH_IS_ASSIGNED;
			return;
		}
	}
}

static inline struct aipc_ch_node *ameba_ipc_find_ch(struct aipc_device *pipc, \
		struct aipc_ch *ch)
{
	struct aipc_ch_node *chn = NULL;
	struct aipc_port *pport = NULL;

	if (!pipc || !ch) {
		goto func_exit;
	}

	if (ch->port_id == AIPC_PORT_NP) {
		pport = pipc->pnp_port;
	} else if (ch->port_id == AIPC_PORT_LP) {
		pport = pipc->plp_port;
	}

	if (!pport) {
		goto func_exit;
	}

	list_for_each_entry(chn, &pport->ch_list, list) {
		if ((chn->ch) && (chn->ch == ch)) {
			break;
		}
	}

func_exit:
	return chn;
}

int ameba_ipc_poll(struct aipc_device *pipc_dev)
{
	u32 reg_isr;
	int retry = 0;
	int ret = 0;

	mdelay(500);

	while (1) {
		reg_isr = readl(pipc_dev->preg_isr);
		/* There is a bug in test chip. The isr cannot be clean sometimes. So to clear
		* it three times.
		*/
		writel(reg_isr, pipc_dev->preg_isr);
		udelay(1);
		writel(reg_isr, pipc_dev->preg_isr);
		udelay(1);
		writel(reg_isr, pipc_dev->preg_isr);
		if (reg_isr & ISR_FROM_NP_MASK) {
			break;
		}
		mdelay(1);
		retry++;
		if (retry > 0xffffff) {
			ret = -1;
			break;
		}
	}

	return ret;
}

int ameba_ipc_probe(struct udevice *dev, struct aipc_device *pipc_dev)
{
	int ret = 0;
	struct aipc_port *pport = NULL;
	struct resource *res_mem = NULL;
	struct resource *res_reg = NULL;

	if (!pipc_dev) {
		ret = -ENOMEM;
		dev_err(dev, "Failed to alloc IPC device\n");
		goto func_exit;
	}

	/* Initialize the port list */
	spin_lock_init(&pipc_dev->lock);

	pipc_dev->dev = dev;

	res_mem = (struct resource *)dev_remap_addr_index(dev, 0);
	if (!res_mem) {
		dev_err(dev, "No LP shared SRAM for IPC\n");
		ret = -EINVAL;
		goto free_pipc;
	}

	if ((res_mem->end - res_mem->start + 1) < TOT_MEM_SZIE) {
		dev_err(dev, "LP shared SRAM for IPC is not enough\n");
		ret = -EINVAL;
		goto free_pipc;
	}

	/* Mapping LP shared SRAM for IPC */
	pipc_dev->dev_mem = (struct ipc_msg_struct *)res_mem;

	/* Mapping the address of registers */
	res_reg = (struct resource *)dev_remap_addr_index(dev, 1);
	if (!res_reg) {
		dev_err(dev, "No TX REGISTER in the resources\n");
		ret = -EINVAL;
		goto free_pipc;
	}
	pipc_dev->preg_tx_data = (u32 *)res_reg;

	res_reg = (struct resource *)dev_remap_addr_index(dev, 2);
	if (!res_reg) {
		dev_err(dev, "No RX REGISTER in the resources\n");
		ret = -EINVAL;
		goto free_pipc;
	}
	pipc_dev->preg_rx_data = (u32 *)res_reg;

	res_reg = (struct resource *)dev_remap_addr_index(dev, 3);
	if (!res_reg) {
		dev_err(dev, "No INTERRUPT STATUS REGISTER in the resources\n");
		ret = -EINVAL;
		goto free_pipc;
	}
	pipc_dev->preg_isr = (u32 *)res_reg;

	res_reg = (struct resource *)dev_remap_addr_index(dev, 4);
	if (!res_reg) {
		dev_err(dev, "No INTERRUPT MASK REGISTER in the resources\n");
		ret = -EINVAL;
		goto free_pipc;
	}
	pipc_dev->preg_imr = (u32 *)res_reg;

	res_reg = (struct resource *)dev_remap_addr_index(dev, 5);
	if (!res_reg) {
		dev_err(dev, "No CLEAR TX REGISTER in the resources\n");
		ret = -EINVAL;
		goto free_pipc;
	}
	pipc_dev->preg_icr = (u32 *)res_reg;

	/* Initialize NP port */
	pport = kzalloc(sizeof(struct aipc_port), GFP_KERNEL);
	if (!pport) {
		ret = -ENOMEM;
		dev_err(dev, "Fail to alloc IPC NP port\n");
		goto free_pipc;
	}
	pipc_dev->pnp_port = pport;

	/* Initialize the channel list */
	spin_lock_init(&pport->lock);
	INIT_LIST_HEAD(&pport->ch_list);
	pport->free_chnl_num = AIPC_CH_MAX_NUM;

	/* Associate the ipc device */
	pport->dev = pipc_dev;

	pport->port_id = AIPC_PORT_NP;
	pport->name = NAME_OF_NP_PORT;

	/* Initialize NP port done*/

	/* Initialize LP port */
	pport = NULL;
	pport = (struct aipc_port *)kzalloc(sizeof(struct aipc_port), GFP_KERNEL);
	if (!pport) {
		ret = -ENOMEM;
		dev_err(dev, "Fail to alloc IPC LP port\n");
		goto free_np_port;
	}
	pipc_dev->plp_port = pport;

	/* Initialize the channel list */
	spin_lock_init(&pport->lock);
	INIT_LIST_HEAD(&pport->ch_list);
	pport->free_chnl_num = AIPC_CH_MAX_NUM;

	/* Associate the IPC device */
	pport->dev = pipc_dev;

	pport->port_id = AIPC_PORT_LP;
	pport->name = NAME_OF_LP_PORT;

	/* Initialize LP port done*/

	goto func_exit;

free_np_port:
	kfree(pipc_dev->pnp_port);

free_pipc:
	/* Do not free pipc in IPC area. */

func_exit:
	return ret;
}

int ameba_ipc_channel_register(struct udevice *dev, struct aipc_ch *ch, struct aipc_device *pipc_dev)
{
	struct aipc_port *pport = NULL;
	struct aipc_ch_node *chn = NULL;
	u32 reg_imr = 0;
	int ret = 0;

	if (!ch) {
		dev_err(dev, "Invalid parameter\n");
		ret = -EINVAL;
		goto func_exit;
	}

	if (!pipc_dev) {
		dev_err(dev, "Invalid IPC device\n");
		ret = -ENODEV;
		goto func_exit;
	}

	/* find the port by port id */
	if (ch->port_id == AIPC_PORT_NP) {
		pport = pipc_dev->pnp_port;
	} else if (ch->port_id == AIPC_PORT_LP) {
		pport = pipc_dev->plp_port;
	} else {
		dev_err(dev, "Port not available\n");
		ret = -EINVAL;
		goto func_exit;
	}

	if (!pport) {
		dev_err(dev, "Port not initialized!\n");
		ret = -EINVAL;
		goto func_exit;
	}

	/*
	 * If channel list is empty, the interrupt is not enabled. So enable
	 * the interrupt.
	 */
	if (list_empty(&pipc_dev->plp_port->ch_list) \
		&& list_empty(&pipc_dev->pnp_port->ch_list)) {
		enable_irq(pipc_dev->irq);
	}

	if (pport->free_chnl_num == 0) {
		dev_err(dev, "No channel to register\n");
		ret = -EBUSY;
		goto func_exit;
	}

	chn = kzalloc(sizeof(struct aipc_ch_node), GFP_KERNEL);
	if (!chn) {
		dev_err(dev, "Fail to alloc IPC channel node\n");
		ret = -EBUSY;
		goto func_exit;
	}

	/* Find valid channel and register the channel to the port */
	if (ch->ch_id >= AIPC_CH_MAX_NUM) {
		ch->ch_id = ameba_ipc_find_channel_id(dev, pport);
	} else {
		ameba_ipc_check_channel_id(dev, pport, &(ch->ch_id));
	}

	if (ch->ch_id >= AIPC_CH_MAX_NUM) {
		dev_err(dev, "Wrong channel id %d, %s\n",
			   ch->ch_id, (ch->ch_id == AIPC_CH_IS_ASSIGNED) ?
			   "channel is assgined, choose another one" :
			   "no valid channel");
		ret = -EBUSY;
		goto free_chn;
	}

	chn->port = pport;
	chn->ch = ch;
	ch->pdev = pipc_dev->dev;

	spin_lock(&pport->lock);
	list_add(&chn->list, &pport->ch_list);
	spin_unlock(&pport->lock);
	pport->free_chnl_num--;

	reg_imr = readl(pipc_dev->preg_imr);
	/* Set the HW interrupt */
	if (ch->port_id == AIPC_PORT_NP) {
		chn->ch_rmem = pipc_dev->dev_mem + BUF_NP2AP_IDX(ch->ch_id);
		chn->ch_wmem = pipc_dev->dev_mem + BUF_AP2NP_IDX(ch->ch_id);
		reg_imr = AIPC_SET_NP_CH_IR_FULL(ch->ch_id, reg_imr);
		/* Configure the empty interrupt */
		if (ch->ch_config & AIPC_CONFIG_HANDSAKKE) {
			reg_imr = AIPC_SET_NP_CH_IR_EMPT(ch->ch_id, reg_imr);
		}
	} else if (ch->port_id == AIPC_PORT_LP) {
		chn->ch_rmem = pipc_dev->dev_mem + BUF_LP2AP_IDX(ch->ch_id);
		chn->ch_wmem = pipc_dev->dev_mem + BUF_AP2LP_IDX(ch->ch_id);
		reg_imr = AIPC_SET_LP_CH_IR_FULL(ch->ch_id, reg_imr);
		/* Configure the empty interrupt */
		if (ch->ch_config & AIPC_CONFIG_HANDSAKKE) {
			reg_imr = AIPC_SET_LP_CH_IR_EMPT(ch->ch_id, reg_imr);
		}
	}

	writel(reg_imr, pipc_dev->preg_imr);
	goto func_exit;

free_chn:
	kfree(chn);

func_exit:
	return ret;
}

int ameba_ipc_channel_unregister(struct udevice *dev, struct aipc_ch *ch, struct aipc_device *pipc_dev)
{
	struct aipc_port *pport = NULL;
	struct aipc_ch_node *chn = NULL;
	u32 reg_imr = 0;

	if (!ch) {
		dev_err(dev, "Invalid parameter\n");
		return -EINVAL;
	}

	if (!pipc_dev) {
		dev_err(dev, "Invalid IPC device\n");
		return -ENODEV;
	}

	chn = ameba_ipc_find_ch(pipc_dev, ch);
	if (!chn) {
		dev_err(dev, "Channel %d not registered\n", ch->ch_id);
		return -EINVAL;
	}

	pport = chn->port;

	reg_imr = readl(pipc_dev->preg_imr);

	/* Set the HW interrupt */
	if (ch->port_id == AIPC_PORT_NP) {
		chn->ch_rmem = pipc_dev->dev_mem + BUF_NP2AP_IDX(ch->ch_id);
		chn->ch_wmem = pipc_dev->dev_mem + BUF_AP2NP_IDX(ch->ch_id);
		reg_imr = AIPC_CLR_NP_CH_IR_FULL(ch->ch_id, reg_imr);
		/* Configure the empty interrupt */
		if (ch->ch_config & AIPC_CONFIG_HANDSAKKE) {
			reg_imr = AIPC_CLR_NP_CH_IR_EMPT(ch->ch_id, reg_imr);
		}
	} else if (ch->port_id == AIPC_PORT_LP) {
		chn->ch_rmem = pipc_dev->dev_mem + BUF_LP2AP_IDX(ch->ch_id);
		chn->ch_wmem = pipc_dev->dev_mem + BUF_AP2LP_IDX(ch->ch_id);
		reg_imr = AIPC_CLR_LP_CH_IR_FULL(ch->ch_id, reg_imr);
		/* Configure the empty interrupt */
		if (ch->ch_config & AIPC_CONFIG_HANDSAKKE) {
			reg_imr = AIPC_CLR_LP_CH_IR_EMPT(ch->ch_id, reg_imr);
		}
	}

	writel(reg_imr, pipc_dev->preg_imr);

	/* Unregister the channel from the port and disable the interrupt */
	spin_lock(&pport->lock);
	chn->ch = NULL;
	list_del(&chn->list);
	spin_unlock(&pport->lock);

	pport->free_chnl_num++;

	kfree(chn);

	/* No channel in the channel list. disable the interrupt. */
	if (list_empty(&pipc_dev->plp_port->ch_list) \
		&& list_empty(&pipc_dev->pnp_port->ch_list)) {
		disable_irq(pipc_dev->irq);
	}

	return 0;
}

int ameba_ipc_channel_send(struct udevice *dev, struct aipc_ch *ch, struct ipc_msg_struct *pmsg, struct aipc_device *pipc_dev)
{
	int ret = 0;
	struct aipc_ch_node *chn = NULL;

	if (!ch) {
		dev_err(dev, "Invalid channel\n");
		ret = -EINVAL;
		goto func_exit;
	}

	if (!pipc_dev) {
		dev_err(dev, "Invalid IPC device\n");
		ret = -ENODEV;
		goto func_exit;
	}

	chn = ameba_ipc_find_ch(pipc_dev, ch);
	if (!chn) {
		dev_err(dev, "IPC channel unregistered\n");
		ret = -EINVAL;
		goto func_exit;
	}

	ret = ameba_ipc_send_work(dev, chn, pmsg);

func_exit:
	return ret;
}

struct aipc_ch *ameba_ipc_alloc_ch(int size_of_priv)
{
	struct aipc_ch *ch = NULL;
	int alloc_size = sizeof(struct aipc_ch);

	alloc_size += size_of_priv - sizeof(void *);

	ch = kzalloc(alloc_size, GFP_KERNEL);
	if (ch) {
		ch->ch_id = AIPC_NOT_ASSIGNED_CH;
	}

	return ch;
}
