/*
 * Ameba Serial Port
 * (C) 2020 by Jingjun WU, Realsil Microelectronics
 *
 * Added serial driver for Realsil Ameba soc
 */

#ifndef __serial_ameba_h
#define __serial_ameba_h

/*
 *Reister define for Ameba
 */
struct ameba_regs {
	u32 DLL;				/*!< LOGUART Divisor Latch register(not used in Amebaz),	Address offset: 0x00*/
	u32 DLH_INTCR;			/*!< LOGUART interrupt enable register,					Address offset: 0x04*/
	u32 INTID;				/*!< LOGUART interrupt identification register,			Address offset: 0x08*/
	u32 LCR;				/*!< LOGUART line control register,						Address offset: 0x0C*/
	u32 MCR;				/*!< LOGUART modem control register,						Address offset: 0x10*/
	u32 LSR;					/*!< LOGUART line status register,						Address offset: 0x14*/
	u32 MDSR;				/*!< LOGUART modem status register,						Address offset: 0x18*/
	u32 SPR;				/*!< LOGUART scratch pad register,							Address offset: 0x1C*/
	u32 STSR;				/*!< LOGUART STS register,									Address offset: 0x20*/
	u32 RBR;			/*!< LOGUART receive buffer register,	Address offset: 0x24*/
	u32 MISCR;				/*!< LOGUART MISC control register,						Address offset: 0x28*/
	u32 TXPLSR;				/*!< LOGUART IrDA SIR TX Pulse Width Control register,	Address offset: 0x2C*/

	u32 RXPLSR;			/*!< LOGUART IrDA SIR RX Pulse Width Control register,	Address offset: 0x30*/
	u32 BAUDMONR;			/*!< LOGUART baud monitor register,						Address offset: 0x34*/
	u32 RSVD2;				/*!< LOGUART reserved field,								Address offset: 0x38*/
	u32 DBG_UART;			/*!< LOGUART debug register,								Address offset: 0x3C*/

	/* AmebaZ add for power save */
	u32 RX_PATH;			/*!< LOGUART rx path control register,						Address offset: 0x40*/
	u32 MON_BAUD_CTRL;	/*!< UART monitor baud rate control register,				Address offset: 0x44*/
	u32 MON_BAUD_STS;		/*!< LOGUART monitor baud rate status register,			Address offset: 0x48*/
	u32 MON_CYC_NUM;		/*!< LOGUART monitor cycle number register,				Address offset: 0x4c*/
	u32 RX_BYTE_CNT;		/*!< LOGUART rx byte counter register,						Address offset: 0x50*/

	/* AmebaZ change */
	u32 FCR;				/*!< LOGUART FIFO Control register,						Address offset: 0x54*/

	/* AmebaD2 change */
	u32 AGGC;				/*!< LOGUART AGG Control register,					Address offset: 0x58*/
	u32 THR1;			/*!< LOGUART transmitter holding register1,			Address offset: 0x5C*/
	u32 THR2;			/*!< LOGUART transmitter holding register2,			Address offset: 0x60*/
	u32 THR3;			/*!< LOGUART transmitter holding register3,			Address offset: 0x64*/
	u32 THR4;			/*!< LOGUART transmitter holding register4,			Address offset: 0x68*/
	u32 RP_LCR;			/*!< LOGUART relay rx path line control register,	Address offset: 0x6C*/
	u32 RP_CTRL;			/*!< LOGUART relay rx path control register,		Address offset: 0x70*/
	u32 ICR;			/*!< LOGUART interrupt clear register,					Address offset: 0x74*/
};

/*
 *Information about a serial port
 *
 * @base: Register base address
 * @clock: Clock of Uart
 * @skip_init: As the serial has been initialized by Rom code, 1 means skip
 */
struct ameba_serial_platdata {
	unsigned long base;
	unsigned int clock;
	bool skip_init;
};

/* Registers Definitions --------------------------------------------------------*/
#define LOGUART_LINE_STATUS_REG_DR			((u32)0x00000001)	     /*BIT[0], Data ready indicator*/
#define LOGUART_LINE_STATUS_REG_P4_THRE			((u32)0x00000001<<19)	     /*BIT[19], Path4 FIFO empty indicator*/
#define LOGUART_LINE_STATUS_REG_P4_FIFONF			((u32)0x00000001<<23)	     /*BIT[23], Path4 FIFO not full indicator*/


#define RUART_LINE_CTL_REG_DLAB_ENABLE	((u32)0x00000001<<7)	      /*BIT[7], 0x80*/
#define RUART_SP_REG_RXBREAK_INT_STATUS	((u32)0x00000001<<7)		/*BIT[7], 0x80, Write 1 clear*/
#define RUART_STS_REG_XFACTOR			((u32)0x000FFFFF<<4)       /*BIT[23:4]ovsr parameter field*/
#define RUART_SP_REG_XFACTOR_ADJ			((u32)0x000007FF<<16)	/*BIT[26:16], ovsr_adj parameter field*/
#define RUART_LP_RX_XTAL_CYCNUM_PERBIT		((u32)0x000FFFFF)     		/*BIT[19:0], Cycle number perbit for xtal clock */
#define RUART_LP_RX_OSC_CYCNUM_PERBIT		((u32)0x000FFFFF << 9)    	/*BIT[28:9], Cycle number perbit for osc clock */
#define RUART_REG_RX_XFACTOR_ADJ					((u32)0x000007FF << 3)	/*BIT[13:3], One factor of Baud rate calculation for rx path, similar with xfactor_adj */

#endif

