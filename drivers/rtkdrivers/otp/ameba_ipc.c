/* SPDX-License-Identifier:  GPL-2.0-or-later */
/*
 * Derived from many drivers using ameba IPC device.
 *
 * Copyright (C) 2020-2021 Realsil <andrew_su@realsil.com.cn>
 *
 * RTK IPC(Inter Process Communication) driver for Ameba IPC.
 *
 */

#define __AMEBA_IPC_C__

/* -------------------------------- Includes -------------------------------- */
/* external head files */
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
#define AIPC_DGB_INFO "ameba ipc"
#define TOT_MEM_SZIE (3 * sizeof(ipc_msg_struct_t) * AIPC_CH_MAX_NUM * MAX_NUM_AIPC_PORT)

struct aipc_ch_node;
struct aipc_device {
	spinlock_t lock; /* list lock */
	struct udevice *dev; /* parent device */
	struct aipc_port *pnp_port; /* pointer to NP port */
	struct aipc_port *plp_port; /* pointer to LP port */
	ipc_msg_struct_t *dev_mem; /* LP shared SRAM for IPC */
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
	ipc_msg_struct_t *ch_rmem; /* channel read shared SRAM for IPC */
	ipc_msg_struct_t *ch_wmem; /* channel written shared SRAM for IPC */
	struct aipc_ch *ch; /* customer regisiting channel */
};

/* define the name of ipc port */
const char NAME_OF_NP_PORT[] = "ipc np port";
const char NAME_OF_LP_PORT[] = "ipc lp port";

static int ameba_ipc_send_work(struct aipc_ch_node *chn, ipc_msg_struct_t *msg)
{
	struct aipc_ch *ch = chn->ch;
	struct aipc_device *pdev = chn->port->dev;
	u32 reg_tx_data = 0;
	int ret = 0;

	reg_tx_data = readl(pdev->preg_tx_data);
	if (ch->port_id == AIPC_PORT_LP) {
		/* check the TX data */
		if (AIPC_GET_LP_CH_NR(ch->ch_id, reg_tx_data)) {
			printk(KERN_ERR "%s: tx is busy!\n", AIPC_DGB_INFO);
			reg_tx_data = AIPC_CLR_LP_CH_NR(ch->ch_id, reg_tx_data);
			ret = -EBUSY;
		} else {
			/* copy data to the lp shared memory */
			memcpy_toio((u8*)chn->ch_wmem, (u8*)msg, sizeof(ipc_msg_struct_t));
			reg_tx_data = AIPC_SET_LP_CH_NR(ch->ch_id, reg_tx_data);
		}
	} else if (ch->port_id == AIPC_PORT_NP) {
		/* check the TX data */
		if (AIPC_GET_NP_CH_NR(ch->ch_id, reg_tx_data)) {
			printk(KERN_ERR "%s: tx is busy!\n", AIPC_DGB_INFO);
			reg_tx_data = AIPC_CLR_NP_CH_NR(ch->ch_id, reg_tx_data);
			ret = -EBUSY;
		} else {
			/* copy data to the lp shared memory */
			memcpy_toio((u8*)chn->ch_wmem, (u8*)msg, sizeof(ipc_msg_struct_t));
			reg_tx_data = AIPC_SET_NP_CH_NR(ch->ch_id, reg_tx_data);
		}
	} else {
		printk(KERN_ERR "%s: inavalib port id!\n", AIPC_DGB_INFO);
		ret = -EINVAL;
	}

	writel(reg_tx_data, pdev->preg_tx_data);

	return ret;
}

static u32 ameba_ipc_find_channel_id(struct aipc_port *pport)
{
	u32 id = AIPC_NOT_ASSIGNED_CH;
	struct aipc_ch_node *chn = NULL;

	if (!pport) {
		printk(KERN_ERR "%s: port is NULL.\n", AIPC_DGB_INFO);
		goto func_exit;
	}

	/* assume is is 0 before polling the channel list */
	id = 0;
	list_for_each_entry(chn, &pport->ch_list, list) {
		if (id == chn->ch->ch_id)
			id++; /* find same id, id++ and compare next channel */
		else
			goto func_exit;; /* this id is not used, break. */

		if (id >= AIPC_CH_MAX_NUM) {
			id = AIPC_NOT_ASSIGNED_CH;
			printk(KERN_ERR "%s: no valid channel.\n", AIPC_DGB_INFO);
			goto func_exit;
		}
	}

func_exit:
	return id;
}

static void ameba_ipc_check_channel_id(struct aipc_port *pport, u32 *pid)
{
	struct aipc_ch_node *chn = NULL;

	if (!pport) {
		printk(KERN_ERR "%s: port is NULL.\n", AIPC_DGB_INFO);
		*pid = AIPC_NOT_ASSIGNED_CH;
		goto func_exit;
	}

	list_for_each_entry(chn, &pport->ch_list, list) {
		if (*pid == chn->ch->ch_id) {
			*pid = AIPC_CH_IS_ASSIGNED;
			goto func_exit; /* this id is used, break. */
		}
	}

func_exit:
	return;
}

int ameba_ipc_poll(struct aipc_device *pipc_dev) {
	u32 reg_isr;
	int retry = 0;

	mdelay(500);

	while (1)
	{
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
			return 1;
		}
		mdelay(1);
		retry++;
		if (retry > 0xffffff) {
			printf("Error: Cannot get ipc res.\n");
			return 0;
		}
	}

}

static inline struct aipc_ch_node *ameba_ipc_find_ch(struct aipc_device *pipc,\
						     struct aipc_ch *ch)
{
	struct aipc_ch_node *chn = NULL;
	struct aipc_port *pport = NULL;

	if (!pipc || !ch)
		goto func_exit;

	if (ch->port_id == AIPC_PORT_NP) {
		pport = pipc->pnp_port;
	} else if (ch->port_id == AIPC_PORT_LP) {
		pport = pipc->plp_port;
	}

	if (!pport) {
		printk(KERN_ERR "%s: channel's pport is null .\n", AIPC_DGB_INFO);
		goto func_exit;
	}

	list_for_each_entry(chn, &pport->ch_list, list) {
		if ((chn->ch) && (chn->ch == ch))
			break;
	}

func_exit:
	return chn;
}

int ameba_ipc_probe(struct udevice *dev, struct aipc_device *pipc_dev)
{
	int ret = 0;
	struct aipc_port *pport = NULL;
	struct resource *res_mem = NULL;
	struct resource *res_reg = NULL;

	if (!pipc_dev) {
		ret = -ENOMEM;
		printk(KERN_ERR "%s: alloc ipc device filed (%d).\n", AIPC_DGB_INFO, ret);
		goto func_exit;
	}
	/* initialize the port list */
	spin_lock_init(&pipc_dev->lock);

	pipc_dev->dev = dev;

	res_mem = dev_remap_addr_index(dev, 0);
	if (!res_mem) {
		printk(KERN_ERR "%s: no LP shared SRAM for IPC.\n", AIPC_DGB_INFO);
		ret = -EINVAL;
		goto free_pipc;
	}

	if ((res_mem->end - res_mem->start + 1) < TOT_MEM_SZIE) {
		printk(KERN_ERR "%s: LP shared SRAM for IPC is not enough.\n", AIPC_DGB_INFO);
		ret = -EINVAL;
		goto free_pipc;
	}
	/* mapping the LP shared SRAM for IPC */
	pipc_dev->dev_mem = (ipc_msg_struct_t *)res_mem;

	/* mapping the address of registers */
	res_reg = dev_remap_addr_index(dev, 1);
	if (!res_reg) {
		printk(KERN_ERR "%s: no TX REGISTER in the resources.\n", AIPC_DGB_INFO);
		ret = -EINVAL;
		goto free_pipc;
	}
	pipc_dev->preg_tx_data = (u32 *)res_reg;

	res_reg = dev_remap_addr_index(dev, 2);
	if (!res_reg) {
		printk(KERN_ERR "%s: no RX REGISTER in the resources.\n", AIPC_DGB_INFO);
		ret = -EINVAL;
		goto free_pipc;
	}
	pipc_dev->preg_rx_data = (u32 *)res_reg;

	res_reg = dev_remap_addr_index(dev, 3);
	if (!res_reg) {
		printk(KERN_ERR "%s: no INTERRUPT STATUS REGISTER in the resources.\n", AIPC_DGB_INFO);
		ret = -EINVAL;
		goto free_pipc;
	}
	pipc_dev->preg_isr = (u32 *)res_reg;

	res_reg = dev_remap_addr_index(dev, 4);
	if (!res_reg) {
		printk(KERN_ERR "%s: no INTERRUPT MASK REGISTER in the resources.\n", AIPC_DGB_INFO);
		ret = -EINVAL;
		goto free_pipc;
	}
	pipc_dev->preg_imr = (u32 *)res_reg;

	res_reg = dev_remap_addr_index(dev, 5);
	if (!res_reg) {
		printk(KERN_ERR "%s: no CLEAR TX REGISTER in the resources.\n", AIPC_DGB_INFO);
		ret = -EINVAL;
		goto free_pipc;
	}
	pipc_dev->preg_icr = (u32 *)res_reg;

	/* initialize NP port start */
	pport = kzalloc(sizeof(struct aipc_port), GFP_KERNEL);
	if (!pport) {
		ret = -ENOMEM;
		printk(KERN_ERR "%s: alloc ipc port filed (%d).\n",
		       AIPC_DGB_INFO, ret);
		goto free_pipc;
	}
	pipc_dev->pnp_port = pport;

	/* initialize the channel list */
	spin_lock_init(&pport->lock);
	INIT_LIST_HEAD(&pport->ch_list);
	pport->free_chnl_num = AIPC_CH_MAX_NUM;

	/* associate the ipc device */
	pport->dev = pipc_dev;

	pport->port_id = AIPC_PORT_NP;
	pport->name = NAME_OF_NP_PORT;
	/* initialize NP port end */

	/* initialize LP port start */
	pport = NULL;
	pport = kzalloc(sizeof(struct aipc_port), GFP_KERNEL);
	if (!pport) {
		ret = -ENOMEM;
		printk(KERN_ERR "%s: alloc ipc port filed (%d).\n",
		       AIPC_DGB_INFO, ret);
		goto free_np_port;
	}
	pipc_dev->plp_port = pport;

	/* initialize the channel list */
	spin_lock_init(&pport->lock);
	INIT_LIST_HEAD(&pport->ch_list);
	pport->free_chnl_num = AIPC_CH_MAX_NUM;

	/* associate the ipc device */
	pport->dev = pipc_dev;

	pport->port_id = AIPC_PORT_LP;
	pport->name = NAME_OF_LP_PORT;

	goto func_exit;

free_np_port:
	kfree(pipc_dev->pnp_port);

free_pipc:
	/* do not free pipc in ipc area. */

func_exit:
	return ret;
}

int ameba_ipc_channel_register(struct aipc_ch *ch, struct aipc_device *pipc_dev)
{
	struct aipc_port *pport = NULL;
	struct aipc_ch_node *chn = NULL;
	u32 reg_imr = 0;
	int ret = 0;

	if (!ch) {
		printk(KERN_ERR "%s: input parameter error!\n", AIPC_DGB_INFO);
		ret = -EINVAL;
		goto func_exit;
	}

	if (!pipc_dev) {
		printk(KERN_ERR "%s: ipc device is not valid!\n", AIPC_DGB_INFO);
		ret = -ENODEV;
		goto func_exit;
	}

	/* find the port by port id */
	if (ch->port_id == AIPC_PORT_NP) {
		pport = pipc_dev->pnp_port;
	} else if (ch->port_id == AIPC_PORT_LP) {
		pport = pipc_dev->plp_port;
	} else {
		printk(KERN_ERR "%s: no avalib port id!\n", AIPC_DGB_INFO);
		ret = -EINVAL;
		goto func_exit;
	}

	if (!pport) {
		printk(KERN_ERR "%s: this port id is not initialized!\n", AIPC_DGB_INFO);
		ret = -EINVAL;
		goto func_exit;
	}

	/*
	 * If channel list is empty, the interrupt is not enabled. So enable
	 * the interrupt.
	 */
	if (list_empty(&pipc_dev->plp_port->ch_list) \
	    && list_empty(&pipc_dev->pnp_port->ch_list))
		enable_irq(pipc_dev->irq);

	if (pport->free_chnl_num == 0) {
		printk(KERN_ERR "%s: no channel to register!\n", AIPC_DGB_INFO);
		ret = -EBUSY;
		goto func_exit;
	}

	chn = kzalloc(sizeof(struct aipc_ch_node), GFP_KERNEL);
	if (!chn) {
		printk(KERN_ERR "%s: alloc ipc channel enity filed.\n", AIPC_DGB_INFO);
		ret = -EBUSY;
		goto func_exit;
	}

	/* find valid channel and register the channel to the port */
	if (ch->ch_id >= AIPC_CH_MAX_NUM)
		ch->ch_id = ameba_ipc_find_channel_id(pport);
	else
		ameba_ipc_check_channel_id(pport, &(ch->ch_id));

	if (ch->ch_id >= AIPC_CH_MAX_NUM) {
		printk(KERN_ERR "%s: to get channel id is wrong, %s.\n",
		       AIPC_DGB_INFO, (ch->ch_id == AIPC_CH_IS_ASSIGNED) ?
		       "channel is assgined, to choose another one" :
		       "no valid channel.");
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
	/* set the hard ware interrupt */
	if (ch->port_id == AIPC_PORT_NP) {
		chn->ch_rmem = pipc_dev->dev_mem + BUF_NP2AP_IDX(ch->ch_id);
		chn->ch_wmem = pipc_dev->dev_mem + BUF_AP2NP_IDX(ch->ch_id);
		reg_imr = AIPC_SET_NP_CH_IR_FULL(ch->ch_id, reg_imr);
		/* configure the empty interrupt */
		if (ch->ch_config & AIPC_CONFIG_HANDSAKKE)
			reg_imr = AIPC_SET_NP_CH_IR_EMPT(ch->ch_id, reg_imr);
	} else if (ch->port_id == AIPC_PORT_LP) {
		chn->ch_rmem = pipc_dev->dev_mem + BUF_LP2AP_IDX(ch->ch_id);
		chn->ch_wmem = pipc_dev->dev_mem + BUF_AP2LP_IDX(ch->ch_id);
		reg_imr = AIPC_SET_LP_CH_IR_FULL(ch->ch_id, reg_imr);
		/* configure the empty interrupt */
		if (ch->ch_config & AIPC_CONFIG_HANDSAKKE)
			reg_imr = AIPC_SET_LP_CH_IR_EMPT(ch->ch_id, reg_imr);
	}
	writel(reg_imr, pipc_dev->preg_imr);
	goto func_exit;
free_chn:
	kfree(chn);

func_exit:
	return ret;
}

int ameba_ipc_channel_unregister(struct aipc_ch *ch, struct aipc_device *pipc_dev)
{
	struct aipc_port *pport = NULL;
	struct aipc_ch_node *chn = NULL;
	u32 reg_imr = 0;
	int ret = 0;

	if (!ch) {
		printk(KERN_ERR "%s: input parameter error!\n", AIPC_DGB_INFO);
		ret = -EINVAL;
		goto func_exit;
	}

	if (!pipc_dev) {
		printk(KERN_ERR "%s: ipc device is not valid!\n", AIPC_DGB_INFO);
		ret = -ENODEV;
		goto func_exit;
	}

	chn = ameba_ipc_find_ch(pipc_dev, ch);
	if (!chn) {
		printk(KERN_ERR "%s: no regisiting this channel!\n", AIPC_DGB_INFO);
		ret = -EINVAL;
		goto func_exit;
	}
	pport = chn->port;

	reg_imr = readl(pipc_dev->preg_imr);
	/* set the hard ware interrupt */
	if (ch->port_id == AIPC_PORT_NP) {
		chn->ch_rmem = pipc_dev->dev_mem + BUF_NP2AP_IDX(ch->ch_id);
		chn->ch_wmem = pipc_dev->dev_mem + BUF_AP2NP_IDX(ch->ch_id);
		reg_imr = AIPC_CLR_NP_CH_IR_FULL(ch->ch_id, reg_imr);
		/* configure the empty interrupt */
		if (ch->ch_config & AIPC_CONFIG_HANDSAKKE)
			reg_imr = AIPC_CLR_NP_CH_IR_EMPT(ch->ch_id, reg_imr);
	} else if (ch->port_id == AIPC_PORT_LP) {
		chn->ch_rmem = pipc_dev->dev_mem + BUF_LP2AP_IDX(ch->ch_id);
		chn->ch_wmem = pipc_dev->dev_mem + BUF_AP2LP_IDX(ch->ch_id);
		reg_imr = AIPC_CLR_LP_CH_IR_FULL(ch->ch_id, reg_imr);
		/* configure the empty interrupt */
		if (ch->ch_config & AIPC_CONFIG_HANDSAKKE)
			reg_imr = AIPC_CLR_LP_CH_IR_EMPT(ch->ch_id, reg_imr);
	}
	writel(reg_imr, pipc_dev->preg_imr);

	/* unregister the channel from the port and disable the interrupt */
	spin_lock(&pport->lock);
	chn->ch = NULL;
	list_del(&chn->list);
	spin_unlock(&pport->lock);
	pport->free_chnl_num++;
	kfree(chn);

	/*
	 * no channel in the channel list. disable the interrupt.
	 */
	if (list_empty(&pipc_dev->plp_port->ch_list) \
	    && list_empty(&pipc_dev->pnp_port->ch_list))
		disable_irq(pipc_dev->irq);

func_exit:
	return ret;
}

int ameba_ipc_channel_send(struct aipc_ch *ch, ipc_msg_struct_t *pmsg, struct aipc_device *pipc_dev)
{
	int ret = 0;
	struct aipc_ch_node *chn = NULL;

	if (!ch) {
		printk(KERN_ERR "n%s: input parameter error!\n", AIPC_DGB_INFO);
		ret = -EINVAL;
		goto func_exit;
	}

	if (!pipc_dev) {
		printk(KERN_ERR "%s: ipc device is not valid!\n", AIPC_DGB_INFO);
		ret = -ENODEV;
		goto func_exit;
	}

	chn = ameba_ipc_find_ch(pipc_dev, ch);
	if (!chn) {
		printk(KERN_ERR "%s: no regisiting this channel!\n", AIPC_DGB_INFO);
		ret = -EINVAL;
		goto func_exit;
	}

	ret = ameba_ipc_send_work(chn, pmsg);

func_exit:
	return ret;
}

struct aipc_ch *ameba_ipc_alloc_ch(int size_of_priv)
{
	struct aipc_ch *ch = NULL;
	int alloc_size = sizeof(struct aipc_ch);

	alloc_size += size_of_priv - sizeof(void *);

	ch = kzalloc(alloc_size, GFP_KERNEL);
	if (!ch) {
		printk(KERN_ERR "%s: alloc ipc reg channel filed.\n", AIPC_DGB_INFO);
		goto func_exit;
	}
	ch->ch_id = AIPC_NOT_ASSIGNED_CH;

func_exit:
	return ch;
}
