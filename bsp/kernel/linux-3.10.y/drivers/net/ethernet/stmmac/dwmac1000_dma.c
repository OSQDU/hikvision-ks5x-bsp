/*******************************************************************************
  This is the driver for the GMAC on-chip Ethernet controller for ST SoCs.
  DWC Ether MAC 10/100/1000 Universal version 3.41a  has been used for
  developing this code.

  This contains the functions to handle the dma.

  Copyright (C) 2007-2009  STMicroelectronics Ltd

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
*******************************************************************************/

#include "dwmac1000.h"
#include "dwmac_dma.h"

/*  TODO - for debug only, remove later */
#include "tnkhw_regmap.h"

static int dwmac1000_dma_init(void __iomem *ioaddr, int channel, int pbl,
			      u32 dma_tx, u32 dma_rx, int use_atds)
{
	u32 value = readl(ioaddr + (channel * 0x100) + DMA_BUS_MODE);
	u32 burst_len;

	value = /* DMA_BUS_MODE_FB | DMA_BUS_MODE_4PBL | */
		((pbl << DMA_BUS_MODE_PBL_SHIFT) |
		 (pbl << DMA_BUS_MODE_RPBL_SHIFT));

	/* Alternate Descriptor Size
	 * Descriptor size is 32-bytes if enabled, 16-bytes otherwise */
	if (use_atds)
		value |= DMA_BUS_MODE_ATDS;

#ifdef CONFIG_STMMAC_DA
	value |= DMA_BUS_MODE_DA;	/* Rx has priority over tx */
#endif
	writel(value, ioaddr + (channel * 0x100) + DMA_BUS_MODE);

	burst_len = readl(ioaddr + (channel * 0x100) + DMA_AXI_BUS_MODE);
	burst_len = burst_len &
		~(DMA_AXI_WR_OSR_LMT_MASK << DMA_AXI_WR_OSR_LMT_OFFSET);
	burst_len = burst_len |
		((0xf & DMA_AXI_WR_OSR_LMT_MASK) << DMA_AXI_WR_OSR_LMT_OFFSET);
	burst_len = burst_len &
		~(DMA_AXI_RD_OSR_LMT_MASK << DMA_AXI_RD_OSR_LMT_OFFSET);
	burst_len = burst_len |
		((0xf & DMA_AXI_RD_OSR_LMT_MASK) << DMA_AXI_RD_OSR_LMT_OFFSET);
	writel(burst_len, ioaddr + (channel * 0x100) + DMA_AXI_BUS_MODE);

	/* Mask interrupts by writing to CSR7 */
	writel(DMA_INTR_DEFAULT_MASK,
	       ioaddr + (channel * 0x100) + DMA_INTR_ENA);

	/* The base address of the RX/TX descriptor lists must be written into
	 * DMA CSR3 and CSR4, respectively. */
	writel(dma_tx, ioaddr + (channel * 0x100) + DMA_TX_BASE_ADDR);
	writel(dma_rx, ioaddr + (channel * 0x100) + DMA_RCV_BASE_ADDR);

	return 0;
}

static void dwmac1000_dma_operation_mode(void __iomem *ioaddr, int channel,
					 int txmode, int rxmode,
					int rfa, int rfd)
{
	u32 csr6 = readl(ioaddr + (channel * 0x100) + DMA_CONTROL);

	if (txmode == SF_DMA_MODE) {
		CHIP_DBG(KERN_DEBUG "GMAC: enable TX store and forward mode\n");
		/* Transmit COE type 2 cannot be done in cut-through mode. */
		csr6 |= DMA_CONTROL_TSF;
		/* Operating on second frame increase the performance
		 * especially when transmit store-and-forward is used.*/
		/* TOE don't support "Operating on second frame",
		 * so if tx_coe enable, we should clear this bit.
		 */
		csr6 &= ~DMA_CONTROL_OSF;
	} else {
		CHIP_DBG(KERN_DEBUG "GMAC: disabling TX store and forward mode"
			      " (threshold = %d)\n", txmode);
		csr6 &= ~DMA_CONTROL_TSF;
		csr6 &= DMA_CONTROL_TC_TX_MASK;
		/* Set the transmit threshold */
		if (txmode <= 32)
			csr6 |= DMA_CONTROL_TTC_32;
		else if (txmode <= 64)
			csr6 |= DMA_CONTROL_TTC_64;
		else if (txmode <= 128)
			csr6 |= DMA_CONTROL_TTC_128;
		else if (txmode <= 192)
			csr6 |= DMA_CONTROL_TTC_192;
		else
			csr6 |= DMA_CONTROL_TTC_256;
	}

	if (rxmode == SF_DMA_MODE) {
		CHIP_DBG(KERN_DEBUG "GMAC: enable RX store and forward mode\n");
		csr6 |= DMA_CONTROL_RSF;
		/* Disable flushing of received frames, required by TNK */
		csr6 |= DMA_CONTROL_DFF;
	} else {
		CHIP_DBG(KERN_DEBUG "GMAC: disabling RX store and forward mode"
			      " (threshold = %d)\n", rxmode);
		csr6 &= ~DMA_CONTROL_RSF;
		csr6 &= DMA_CONTROL_TC_RX_MASK;
		if (rxmode <= 32)
			csr6 |= DMA_CONTROL_RTC_32;
		else if (rxmode <= 64)
			csr6 |= DMA_CONTROL_RTC_64;
		else if (rxmode <= 96)
			csr6 |= DMA_CONTROL_RTC_96;
		else
			csr6 |= DMA_CONTROL_RTC_128;
	}

	csr6 |= DMA_CONTROL_EFC;
	csr6 &= DMA_CONTROL_RFA_MASK;
	switch (rfa) {
	case 1:
		csr6 |= DMA_CONTROL_RFA_FULL_MINUS_1KB;
		break;
	case 2:
		csr6 |= DMA_CONTROL_RFA_FULL_MINUS_2KB;
		break;
	case 3:
		csr6 |= DMA_CONTROL_RFA_FULL_MINUS_3KB;
		break;
	case 4:
		csr6 |= DMA_CONTROL_RFA_FULL_MINUS_4KB;
		break;
	case 5:
		csr6 |= DMA_CONTROL_RFA_FULL_MINUS_5KB;
		break;
	case 6:
		csr6 |= DMA_CONTROL_RFA_FULL_MINUS_6KB;
		break;
	case 7:
		csr6 |= DMA_CONTROL_RFA_FULL_MINUS_7KB;
		break;
	default:
		csr6 |= DMA_CONTROL_RFA_FULL_MINUS_2KB;
		break;
	}

	csr6 &= DMA_CONTROL_RFD_MASK;
	switch (rfd) {
	case 1:
		csr6 |= DMA_CONTROL_RFD_FULL_MINUS_1KB;
		break;
	case 2:
		csr6 |= DMA_CONTROL_RFD_FULL_MINUS_2KB;
		break;
	case 3:
		csr6 |= DMA_CONTROL_RFD_FULL_MINUS_3KB;
		break;
	case 4:
		csr6 |= DMA_CONTROL_RFD_FULL_MINUS_4KB;
		break;
	case 5:
		csr6 |= DMA_CONTROL_RFD_FULL_MINUS_5KB;
		break;
	case 6:
		csr6 |= DMA_CONTROL_RFD_FULL_MINUS_6KB;
		break;
	case 7:
		csr6 |= DMA_CONTROL_RFD_FULL_MINUS_7KB;
		break;
	default:
		csr6 |= DMA_CONTROL_RFD_FULL_MINUS_2KB;
		break;
	}

	writel(csr6, ioaddr + (channel * 0x100) + DMA_CONTROL);
}

/* Not yet implemented --- no RMON module */
static void dwmac1000_dma_diagnostic_fr(void *data,
					struct stmmac_extra_stats *x,
					void __iomem *ioaddr,
					int channel)
{
	return;
}

static void dwmac1000_dump_dma_regs(void __iomem *ioaddr,
				    int channel)
{
	int i;
	pr_info(" DMA registers chan %d\n", channel);
	for (i = 0; i < 22; i++) {
		if ((i < 9) || (i > 17)) {
			int offset = (channel * 0x100) + (i * 4);
			pr_err("\t Reg No. %d (offset 0x%x): 0x%08x\n", i,
			       (DMA_BUS_MODE + offset),
			       readl(ioaddr + DMA_BUS_MODE + offset));
		}
	}
}

const struct stmmac_dma_ops dwmac1000_dma_ops = {
	.init = dwmac1000_dma_init,
	.dump_regs = dwmac1000_dump_dma_regs,
	.dma_mode = dwmac1000_dma_operation_mode,
	.dma_diagnostic_fr = dwmac1000_dma_diagnostic_fr,
	.enable_dma_transmission = dwmac_enable_dma_transmission,
	.enable_dma_receive = dwmac_enable_dma_receive,
	.get_dma_rx_state = dwmac_get_dma_rx_state,
	.enable_dma_irq = dwmac_enable_dma_irq,
	.disable_dma_irq = dwmac_disable_dma_irq,
	.start_tx = dwmac_dma_start_tx,
	.stop_tx = dwmac_dma_stop_tx,
	.start_rx = dwmac_dma_start_rx,
	.stop_rx = dwmac_dma_stop_rx,
	.dma_interrupt = dwmac_dma_interrupt,
};
