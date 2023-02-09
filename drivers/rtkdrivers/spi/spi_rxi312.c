/*
 * Ameba SPI controller driver
 *
 * Copyright 2015  Jethro Hsu (jethro@realtek.com)
 *
 */

#include <common.h>
#include <log.h>
#include <malloc.h>
#include <spi.h>
#include <asm/io.h>

#include "spi_rxi312.h"

#ifdef CONFIG_DM_SPI
#include <dm.h>
DECLARE_GLOBAL_DATA_PTR;
#endif

/* FIFO length */
#define RXI312_FIFO_LENGTH	32

/* Get unaligned buffer size */
#define UNALIGNED32(buf)	((4 - ((u32)(buf) & 0x3)) & 0x3)

void udelay(unsigned long usec);

/*
 * This function is used to wait the spi_flash is not at busy state.
 */
static void spic_waitbusy(struct ameba_spi *dev, u32 WaitType)
{
	u32 BusyCheck = 0;
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;

	do {

		if (WaitType == WAIT_SPIC_BUSY) {
			BusyCheck = (spi_flash_map->sr & BIT_BUSY);

		} else if (WaitType == WAIT_TRANS_COMPLETE) {
			/* When transfer completes, SSIENR.SPIC_EN are cleared by HW automatically. */
			BusyCheck = (spi_flash_map->ssienr & BIT_SPIC_EN);
		}

		if (!BusyCheck) {
			break;
		}
	} while (1);
}

static void spic_usermode_en(struct ameba_spi *dev, u8 enable)
{
	struct spi_flash_portmap *spi_flash = (struct spi_flash_portmap *)dev->regs;

	/* Wait spic busy done before switch mode */
	spic_waitbusy(dev, WAIT_SPIC_BUSY);

	if (enable) {
		spi_flash->ctrlr0 |= BIT_USER_MODE;
	} else {
		spi_flash->ctrlr0 &= ~BIT_USER_MODE;
	}
}

#ifndef CONFIG_SPIC_RTK_AMEBA_NAND

static int select_op(spic_mode *mode, uint8_t cmd)
{
	int ret = 0;

	switch (cmd) {
	case FLASH_CMD_PP:
	case FLASH_CMD_WREN:
	case FLASH_CMD_WRDI:
	case FLASH_CMD_WRSR:
	case FLASH_CMD_EXTNADDR_WREAR:
	case FLASH_CMD_CE:
	case FLASH_CMD_SE:
	case FLASH_CMD_BE:
	case FLASH_CMD_EN4B:
	case FLASH_CMD_EX4B:
		mode->addr_ch = 0;
		mode->data_ch = 0;
		mode->tmod = 0;
		break;

	case FLASH_CMD_RDSFDP:
	case FLASH_CMD_RDSR:
	case FLASH_CMD_RDSR2:
	case FLASH_CMD_EXTNADDR_RDEAR:
	case FLASH_CMD_RDID:
	case FLASH_CMD_READ:
	case FLASH_CMD_FREAD:
		mode->addr_ch = 0;
		mode->data_ch = 0;
		mode->tmod = 3;
		break;

	case FLASH_CMD_DREAD:
		mode->addr_ch = 0;
		mode->data_ch = 1;
		mode->tmod = 3;
		break;
	case FLASH_CMD_2READ:
		mode->addr_ch = 1;
		mode->data_ch = 1;
		mode->tmod = 3;
		break;
	case FLASH_CMD_4READ:
		mode->addr_ch = 2;
		mode->data_ch = 2;
		mode->tmod = 3;
		break;
	case FLASH_CMD_QREAD:
		mode->addr_ch = 0;
		mode->data_ch = 2;
		mode->tmod = 3;
		break;
	case FLASH_CMD_4PP:
		mode->addr_ch = 2;
		mode->data_ch = 2;
		mode->tmod = 0;
		break;

	default:
		printf("WARNING: Unsupported NOR flash cmd: 0x%02X\n", cmd);
		ret = -1;
		break;
	}

	return ret;
}

#endif // #ifndef CONFIG_SPIC_RTK_AMEBA_NAND

static int dw_spi_setup(struct ameba_spi *dev, unsigned int cs)
{
	/* spic is intialized by km4, no need to re-intialize it.
	   Initialize it for other chips if need. */

	return 0;
}

#ifdef CONFIG_SPIC_RTK_AMEBA_NAND

static int select_nand_op(spic_mode *mode, uint8_t cmd)
{
	int ret = 0;

	switch (cmd) {
	case NAND_CMD_BE:
	case NAND_CMD_WREN:
	case NAND_CMD_WRDI:
	case NAND_CMD_WRSR:
	case NAND_CMD_RESET:
	case NAND_CMD_PAGERD:
	case NAND_CMD_PP:
	case NAND_CMD_PP_RANDOM:
	case NAND_CMD_PROMEXEC:
		mode->addr_ch = 0;
		mode->data_ch = 0;
		mode->tmod = 0;
		break;

	case NAND_CMD_READ:
	case NAND_CMD_FREAD:
	case NAND_CMD_RDID:
	case NAND_CMD_RDSR:
		mode->addr_ch = 0;
		mode->data_ch = 0;
		mode->tmod = 3;
		break;

	case NAND_CMD_DREAD:
		mode->addr_ch = 0;
		mode->data_ch = 1;
		mode->tmod = 3;
		break;
	case NAND_CMD_2READ:
		mode->addr_ch = 1;
		mode->data_ch = 1;
		mode->tmod = 3;
		break;
	case NAND_CMD_QREAD:
		mode->addr_ch = 0;
		mode->data_ch = 2;
		mode->tmod = 3;
		break;
	case NAND_CMD_4READ:
		mode->addr_ch = 2;
		mode->data_ch = 2;
		mode->tmod = 3;
		break;
	case NAND_CMD_QPP:
	case NAND_CMD_QPP_RANDOM:
		mode->addr_ch = 0;
		mode->data_ch = 2;
		mode->tmod = 0;
		break;

	default:
		printf("WARNING: Unsupported NAND flash cmd: 0x%02X\n", cmd);
		ret = -1;
		break;
	}

	return ret;
}

#endif // #ifdef CONFIG_SPIC_RTK_AMEBA_NAND

static int do_spi_send(struct udevice *udev, const void *tx_data, unsigned int len, unsigned long flags)
{
	int ret = 0;
	struct ameba_spi *dev = dev_get_priv(udev);
	struct spi_flash_portmap *spi_flash_map = dev->regs;
	u32 ctrl0, value;
	spic_mode mode = {0};
	u8 *tx_buf = (u8*) tx_data;
	u8 cmd = tx_buf[0];
	u32 i = 0;
	u32 j;
	u32 *aligned32_buf;
	u32 unaligned32_bytes;
	u32 fifo_level;

	if (flags & SPI_XFER_BEGIN) {
		/* Enter user mode */
		spic_usermode_en(dev, 1);

#ifdef CONFIG_SPIC_RTK_AMEBA_NAND
		ret = select_nand_op(&mode, cmd);
#else
		ret = select_op(&mode, cmd);
#endif
		if (ret) {
			return ret;
		}

		/* set CTRLR0: TX mode and one bit mode */
		ctrl0 = spi_flash_map->ctrlr0;
		ctrl0 &= ~(TMOD(3) | CMD_CH(3) | ADDR_CH(3) | DATA_CH(3));
		ctrl0 |= TMOD(mode.tmod) | ADDR_CH(mode.addr_ch) | DATA_CH(mode.data_ch);
		spi_flash_map->ctrlr0 = ctrl0;
		
		value = spi_flash_map->user_length & ~MASK_USER_ADDR_LENGTH;
		value |= USER_ADDR_LENGTH(len - 1);
		
#ifdef CONFIG_SPIC_RTK_AMEBA_NAND
		/* Set DUM length */
		if (mode.tmod == TMODE_TX) {
			value &= ~MASK_USER_RD_DUMMY_LENGTH;
		} else {
			/* Get Feature CMD do not have dummy cycle after address phase */
			if (cmd == NAND_CMD_RDSR) {
				value &= ~MASK_USER_RD_DUMMY_LENGTH;
			}
		}
#else
		value &= ~MASK_USER_RD_DUMMY_LENGTH;
#endif
		
		spi_flash_map->user_length = value;

		/* set flash_cmd: write cmd & address to fifo & addr is MSB */
		for ( i = 0; i < len; i++)
			spi_flash_map->dr[0].byte = tx_buf[i];

		if (flags & SPI_XFER_END) {
			spi_flash_map->tx_ndf = TX_NDF(0);
			spi_flash_map->rx_ndf = RX_NDF(0);

			/* Command without data: WREN, BE_4K, SE, and CE  */
			/* Enable SSIENR to start the transfer */
			spi_flash_map->ssienr = BIT_SPIC_EN;

			/* Wait transfer complete. When complete, SSIENR.SPIC_EN are cleared by HW automatically. */
			spic_waitbusy(dev, WAIT_TRANS_COMPLETE);

			/* Exit user mode */
			spic_usermode_en(dev, 0);
		}

	} else if (flags & SPI_XFER_END) {
		/* Set TX_NDF: frame number of Tx data. */
		spi_flash_map->tx_ndf = TX_NDF(len);
		spi_flash_map->rx_ndf = RX_NDF(0);
		
		/* Enable SSIENR to start the transfer */
		spi_flash_map->ssienr = BIT_SPIC_EN;

		unaligned32_bytes = UNALIGNED32(tx_buf);
		while ((i < unaligned32_bytes) && (i < len)) {
			if (spi_flash_map->txflr <= RXI312_FIFO_LENGTH - 1) {
				spi_flash_map->dr[0].byte = tx_buf[i++];
			}
		}

		aligned32_buf = (u32 *)&tx_buf[i];

		while (i + 4 <= len) {
			fifo_level = (RXI312_FIFO_LENGTH - spi_flash_map->txflr) >> 2;
			for (j = 0; (j < fifo_level) && (i + 4 <= len); j++) {
				spi_flash_map->dr[0].word = aligned32_buf[j];
				i += 4;
			}
			aligned32_buf += fifo_level;
		}

		while (i < len) {
			if (spi_flash_map->txflr <= RXI312_FIFO_LENGTH - 1) {
				spi_flash_map->dr[0].byte = tx_buf[i++];
			}
		}

		/* Wait transfer complete. When complete, SSIENR.SPIC_EN are cleared by HW automatically. */
		spic_waitbusy(dev, WAIT_TRANS_COMPLETE);

		/* Exit user mode */
		spic_usermode_en(dev, 0);
	}

	return ret;
}

static int do_spi_recv(struct udevice *udev, const unsigned char *rx_data, unsigned int len, unsigned long flags)
{
	struct ameba_spi *dev = dev_get_priv(udev);
	struct spi_flash_portmap *spi_flash_map = dev->regs;
	u8 *rx_buf = (u8 *)rx_data;
	u32 i = 0;
	u32 j;
	u32 *aligned32_buf;
	u32 unaligned32_bytes;
	u32 fifo_level;
	
	/* Set RX_NDF: frame number of receiving data. TX_NDF should be set in both transmit mode and receive mode.
		TX_NDF should be set to zero in receive mode to skip the TX_DATA phase. */
	spi_flash_map->rx_ndf = RX_NDF(len);
	spi_flash_map->tx_ndf = TX_NDF(0);

	/* Enable SSIENR to start the transfer */
	spi_flash_map->ssienr = BIT_SPIC_EN;

	unaligned32_bytes = UNALIGNED32(rx_buf);
	while ((i < unaligned32_bytes) && (i < len)) {
		if (spi_flash_map->rxflr >= 1) {
			rx_buf[i++] = spi_flash_map->dr[0].byte;
		}
	}

	aligned32_buf = (u32 *)&rx_buf[i];

	while (i + 4 <= len) {
		fifo_level = spi_flash_map->rxflr >> 2;
		// A safe way to avoid HW error: (j < fifo_level) && (i + 4 <= nbytes)
		for (j = 0; j < fifo_level; j++) {
			aligned32_buf[j] = spi_flash_map->dr[0].word;
		}
		i += fifo_level << 2;
		aligned32_buf += fifo_level;
	}

	while (i < len) {
		if (spi_flash_map->rxflr >= 1) {
			rx_buf[i++] = spi_flash_map->dr[0].byte;
		}
	}

	/* Wait transfer complete. When complete, SSIENR.SPIC_EN are cleared by HW automatically. */
	spic_waitbusy(dev, WAIT_TRANS_COMPLETE);

	/* Exit user mode */
	spic_usermode_en(dev, 0);

	return 0;
}

#ifdef CONFIG_DM_SPI

int ameba_dm_spi_xfer(struct udevice *uflash, unsigned int bitlen, const void *dout, void *din, unsigned long flags)
{
	struct udevice *udev = dev_get_parent(uflash);
	const unsigned char *tx_data = dout;
	unsigned char *rx_data = din;
	unsigned int len = bitlen / 8;
	int ret = 0;

	if (flags & SPI_XFER_BEGIN) {
		spi_cs_activate(uflash);
	}
	
	if (tx_data) {
		do_spi_send(udev, tx_data, len, flags);
	}

	if (rx_data) {
		do_spi_recv(udev, rx_data, len, flags);
	}
		
	if (flags & SPI_XFER_END) {
		spi_cs_deactivate(uflash);
	}

	return ret;
}

int ameba_dm_spi_ofdata_to_platdata(struct udevice *udev)
{
	struct ameba_spi_platdata *plat = udev->platdata;
	const void *blob = gd->fdt_blob;
	int node = dev_of_offset(udev);

	/* Use 500KHz as a suitable default */
	plat->deactivate_delay_us = fdtdec_get_int(blob, node, "spi-deactivate-delay", 0);

	return 0;
}

int ameba_dm_spi_probe(struct udevice *udev)
{
	fdt_addr_t addr;
	struct ameba_spi_platdata *plat = dev_get_platdata(udev);
	struct ameba_spi *priv = dev_get_priv(udev);

	if (!plat || !priv)
		return -ENODEV;

	addr = dev_read_addr(udev);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	/* Convert 0x1fb0xxxx to 0xbfb0xxxx */
#if defined(CONFIG_SOC_CPU_RLX) || defined(CONFIG_SOC_CPU_MIPS)
	priv->regs = (struct spi_flash_portmap *)map_physmem(addr, 0, MAP_NOCACHE);
#else
	priv->regs = (struct spi_flash_portmap *)addr;
#endif

	priv->last_transaction_us = timer_get_us();

	/* Init flash */
	dw_spi_setup(priv, 0);

	return 0;
}

int ameba_dm_spi_set_speed(struct udevice *udev, uint speed)
{
	/* Do nothing */
	return 0;
}

int ameba_dm_spi_set_mode(struct udevice *udev, uint mode)
{
	struct ameba_spi *priv = dev_get_priv(udev);
	struct spi_flash_portmap *spi_flash_map = priv->regs;
	u32 ctrl0;
	u32 valid_cmd;

	if (!mode) {
		ctrl0 = spi_flash_map->ctrlr0;
		valid_cmd = spi_flash_map->valid_cmd;

		if (ctrl0 & BIT_SCPOL) {
			mode |= SPI_CPOL;
		}

		if (ctrl0 & BIT_SCPH) {
			mode |= SPI_CPHA;
		}

		if (((valid_cmd & BIT_RD_QUAD_IO) != 0) || ((valid_cmd & BIT_RD_QUAD_O) != 0)) {
			mode |= SPI_TX_QUAD | SPI_RX_QUAD;
		} else if (((valid_cmd & BIT_RD_DUAL_IO) != 0) || ((valid_cmd & BIT_RD_DUAL_I) != 0)) {
			mode |= SPI_TX_DUAL | SPI_RX_DUAL;
		}
	}
	priv->mode = mode;

	printf("SPIC mode 0x%X\n", priv->mode);

	return 0;
}

int ameba_flush_fifo(struct udevice *uflash)
{
	if (!uflash)
		return -ENODEV;
	struct ameba_spi *priv = dev_get_priv(uflash->parent);
	struct spi_flash_portmap *spi_flash_map = priv->regs;

	spi_flash_map->ssienr = 0;
	spi_flash_map->flush_fifo = 0;
	return 0;
}

int ameba_dm_spi_claim_bus(struct udevice *uflash)
{
	ameba_flush_fifo(uflash);
	return 0;
}

int ameba_dm_spi_release_bus(struct udevice *uflash)
{
	return 0;
}

int ameba_dm_spi_cs_info(struct udevice *udev, uint cs, struct spi_cs_info *info)
{
	/* Only allow device activity on CS 0 */
	if (!cs)
		return -ENODEV;
	return 0;
}

void spi_cs_activate(struct udevice *uflash)
{
	struct udevice *udev = uflash->parent;
	struct ameba_spi_platdata *plat = dev_get_platdata(udev);
	struct ameba_spi *priv = dev_get_priv(udev);

	/* If it's too soon to do another transaction, wait */
	if (plat->deactivate_delay_us && priv->last_transaction_us) {
		ulong delay_us;         /* The delay completed so far */

		delay_us = timer_get_us() - priv->last_transaction_us;
		if (delay_us < plat->deactivate_delay_us)
			udelay(plat->deactivate_delay_us - delay_us);
	}
}

void spi_cs_deactivate(struct udevice *uflash)
{
	struct udevice *udev = uflash->parent;
	struct ameba_spi_platdata *plat = dev_get_platdata(udev);
	struct ameba_spi *priv = dev_get_priv(udev);
	struct spi_flash_portmap *spi_flash_map = priv->regs;

	/* cs deactivate */
	spi_flash_map->ssienr = 0;

	/* Remember time of this transaction so we can honour the bus delay */
	if (plat->deactivate_delay_us)
		priv->last_transaction_us = timer_get_us();
}

static int ameba_dm_spi_child_pre_probe(struct udevice *uflash)
{
	struct dm_spi_slave_platdata *plat = dev_get_parent_platdata(uflash);
	struct spi_slave *slave = dev_get_parent_priv(uflash);

	/*
	 * This is needed because we pass struct spi_slave around the place
	 * instead slave->dev (a struct udevice). So we have to have some
	 * way to access the slave udevice given struct spi_slave. Once we
	 * change the SPI API to use udevice instead of spi_slave, we can
	 * drop this.
	 */
	slave->dev = uflash;

	slave->max_hz = plat->max_hz;
	slave->mode = plat->mode;
	slave->wordlen = SPI_DEFAULT_WORDLEN;

	return 0;
}

static int ameba_dm_spi_child_post_bind(struct udevice *uflash)
{
	struct dm_spi_slave_platdata *plat = dev_get_parent_platdata(uflash);

	if (dev_has_of_node(uflash) == false)
		return 0;

	return spi_slave_ofdata_to_platdata(uflash, plat);
}

static const struct dm_spi_ops ameba_spi_ops = {
	.claim_bus      = ameba_dm_spi_claim_bus,
	.release_bus    = ameba_dm_spi_release_bus,
	.set_speed      = ameba_dm_spi_set_speed,
	.set_mode       = ameba_dm_spi_set_mode,
	.xfer           = ameba_dm_spi_xfer,
	.cs_info        = ameba_dm_spi_cs_info,
};

static const struct udevice_id ameba_spi_ids[] = {
	{ .compatible = "realtek,rxi312-spi" },
	{ }
};

U_BOOT_DRIVER(ameba_spi) = {
	.name   = "ameba_spi",
	.id     = UCLASS_SPI,
	.of_match = ameba_spi_ids,
	.ops    = &ameba_spi_ops,
	.child_pre_probe = ameba_dm_spi_child_pre_probe,
	.child_post_bind = ameba_dm_spi_child_post_bind,
	.ofdata_to_platdata = ameba_dm_spi_ofdata_to_platdata,
	.per_child_platdata_auto_alloc_size = sizeof(struct dm_spi_slave_platdata),
	.platdata_auto_alloc_size = sizeof(struct ameba_spi_platdata),
	.priv_auto_alloc_size = sizeof(struct ameba_spi),
	.probe  = ameba_dm_spi_probe,
};

#endif /* defined CONFIG_DM_SPI */
