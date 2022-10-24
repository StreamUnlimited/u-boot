/*
 * Ameba loguart support
 * modified to use CONFIG_SYS_ISA_MEM and new defines
 */

#include <clock_legacy.h>
#include <common.h>
#include <clk.h>
#include <dm.h>
#include <errno.h>
#include <log.h>
#include <serial_ameba.h>
#include <reset.h>
#include <serial.h>
#include <watchdog.h>
#include <linux/err.h>
#include <linux/types.h>
#include <asm/io.h>

DECLARE_GLOBAL_DATA_PTR;

struct ameba_priv {
	struct ameba_regs *regs;
};

static int ameba_serial_getc(struct udevice *dev)
{
	struct ameba_priv *priv = dev_get_priv(dev);
	struct ameba_regs *regs = priv->regs;
	u32 data;

	if (!(readl(&regs->LSR) & LOGUART_LINE_STATUS_REG_DR))
		return -EAGAIN;

	/*only 8 bit used*/
	data = readl(&regs->RBR);

	return (int)data;
}

static int ameba_serial_putc(struct udevice *dev, const char data)
{
	struct ameba_priv *priv = dev_get_priv(dev);
	struct ameba_regs *regs = priv->regs;

	if (!(readl(&regs->LSR) & LOGUART_LINE_STATUS_REG_P4_FIFONF))
		return -EAGAIN;

	/* Send the character */
	writel(data, &regs->THR4);

	if (data == 0x0a) {
		writel(0x0d, &regs->THR4);
	}

	return 0;
}

static int ameba_serial_pending(struct udevice *dev, bool input)
{
	struct ameba_priv *priv = dev_get_priv(dev);
	struct ameba_regs *regs = priv->regs;
	unsigned int lsr;

	lsr = readl(&regs->LSR);

	if (input) {
		return (lsr & LOGUART_LINE_STATUS_REG_DR) ? 1 : 0;
	} else {
		return (lsr & LOGUART_LINE_STATUS_REG_P4_THRE) ? 0 : 1;
	}
}

/**
  * @brief    get ovsr & ovsr_adj parameters according to the given baudrate and UART IP clock.
  * @param  UARTx: where x can be 0/1/3.
  * @param  IPclk: the given UART IP clock, Unit: [ Hz ]
  * @param  baudrate: the desired baudrate, Unit: bps[ bit per second ]
  * @param  ovsr: parameter related to the desired baud rate( corresponding to STSR[23:4] )
  * @param  ovsr_adj: parameter related to the desired baud rate( corresponding to SCR[26:16] )
  * @retval  None
  */
void uart_baud_para_get(u32 IPclk, u32 baudrate, u32 *ovsr, u32 *ovsr_adj)
{
	u32 i;
	u32 Remainder;
	u32 TempAdj = 0;
	u32 TempMultly;

	/*obtain the ovsr parameter*/
	*ovsr = IPclk / baudrate;

	/*get the remainder related to the ovsr_adj parameter*/
	Remainder = IPclk % baudrate;

	/*calculate the ovsr_adj parameter*/
	for(i = 0; i < 11; i++){
		TempAdj = TempAdj << 1;
		TempMultly = (Remainder * (12-i));
		TempAdj |= ((TempMultly / baudrate - (TempMultly - Remainder) / baudrate) ? 1 : 0);
	}

	/*obtain the ovsr_adj parameter*/
	*ovsr_adj = TempAdj;
}

static int ameba_serial_setbrg(struct udevice *dev, int baudrate)
{
	struct ameba_serial_platdata *plat = dev_get_platdata(dev);
	struct ameba_priv *priv = dev_get_priv(dev);
	struct ameba_regs *regs = priv->regs;
	u32 reg_value;
	u32 ovsr;
	u32 ovsr_adj;

	if (plat->skip_init)
		goto out;

	/* get baud rate parameter based on baudrate */
	uart_baud_para_get(plat->clock, baudrate, &ovsr, &ovsr_adj);

	/* Set DLAB bit to 1 to access DLL/DLM */
	reg_value = readl(&regs->LCR);
	reg_value |= RUART_LINE_CTL_REG_DLAB_ENABLE;
	writel(reg_value, &regs->LCR);

	/*Clean Rx break signal interrupt status at initial stage*/
	reg_value = readl(&regs->SPR);
	reg_value |= RUART_SP_REG_RXBREAK_INT_STATUS;

	/* Set OVSR(xfactor) */
	reg_value = readl(&regs->STSR);
	reg_value &= ~(RUART_STS_REG_XFACTOR);
	reg_value |= ((ovsr << 4) & RUART_STS_REG_XFACTOR);
	writel(reg_value, &regs->STSR);

	/* Set OVSR_ADJ[10:0] (xfactor_adj[26:16]) */
	reg_value = readl(&regs->SPR);
	reg_value &= ~(RUART_SP_REG_XFACTOR_ADJ);
	reg_value |= ((ovsr_adj << 16) & RUART_SP_REG_XFACTOR_ADJ);
	writel(reg_value, &regs->SPR);

	/* clear DLAB bit */
	reg_value = readl(&regs->LCR);
	reg_value &= ~(RUART_LINE_CTL_REG_DLAB_ENABLE);
	writel(reg_value, &regs->LCR);
	
	/*rx baud rate configureation*/
	reg_value = readl(&regs->MON_BAUD_STS);
	reg_value &= (~RUART_LP_RX_XTAL_CYCNUM_PERBIT);
	reg_value |= ovsr;
	writel(reg_value, &regs->MON_BAUD_STS);

	reg_value = readl(&regs->MON_BAUD_CTRL);
	reg_value &= (~RUART_LP_RX_OSC_CYCNUM_PERBIT);
	reg_value |= (ovsr<<9);
	writel(reg_value, &regs->MON_BAUD_CTRL);

	reg_value = readl(&regs->RX_PATH);
	reg_value &= (~RUART_REG_RX_XFACTOR_ADJ);
	reg_value |= (ovsr_adj<<3);
	writel(reg_value, &regs->RX_PATH);

out:
	/* Flush the RX queue - all data in there is bogus */
	while (ameba_serial_getc(dev) != -EAGAIN) ;

	return 0;
}

static const struct dm_serial_ops ameba_serial_ops = {
	.putc = ameba_serial_putc,
	.pending = ameba_serial_pending,
	.getc = ameba_serial_getc,
	.setbrg = ameba_serial_setbrg,
};

#if CONFIG_IS_ENABLED(OF_CONTROL)
static int ameba_serial_probe(struct udevice *dev)
{
	struct ameba_serial_platdata *plat = dev_get_platdata(dev);
	struct ameba_priv *priv = dev_get_priv(dev);
	fdt_addr_t addr;

	addr = devfdt_get_addr(dev);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	plat->base = addr;
	plat->clock = dev_read_u32_default(dev, "clock", 1);

	/*
	 * TODO: Reinitialization doesn't always work for now, just skip
	 *       init always - we know we're already initialized
	 */
	plat->skip_init = true;

	priv->regs = (struct ameba_regs *)plat->base;

	return 0;
}

static const struct udevice_id ameba_serial_id[] = {
	{.compatible = "realtek,ameba_serial"},
	{}
};
#endif

U_BOOT_DRIVER(serial_ameba) = {
	.name = "serial_ameba",
	.id = UCLASS_SERIAL,
	.of_match = of_match_ptr(ameba_serial_id),
	.platdata_auto_alloc_size = sizeof(struct ameba_serial_platdata),
	.probe = ameba_serial_probe,
	.ops = &ameba_serial_ops,
#if !CONFIG_IS_ENABLED(OF_CONTROL) || CONFIG_IS_ENABLED(OF_BOARD)
	.flags = DM_FLAG_PRE_RELOC,
#endif
	.priv_auto_alloc_size = sizeof(struct ameba_priv),
};


