/* linux/drivers/spi/spi-hi3521a.c
 *
 * HI3516A SPI Controller driver
 *
 * Copyright (c) 2014 Hisilicon Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * History:
 *      3-February-2015 create this file
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi-hisilicon.h>

#include <mach/platform.h>

/* ********** sys reg define *************** */
#define CRG33_SSP_CKEN_SHIFT	(13)
#define CRG33_SSP_RST_SHIFT	(5)
#define CRG33_SSP_CKEN		(0x1 << 13) /* 0:close clk, 1:open clk */
#define CRG33_SSP_RST		(0x1 << 5) /* 0:cancel reset, 1:reset */


#define MISC_REG_BASE		0x12120000

#define REG_MISC_CTRL_MAP	IO_ADDRESS(MISC_REG_BASE + 0x04)
#define MISC_CTRL_SSP_CS_SHIFT	0
#define MISC_CTRL_SSP_CS	1 /* 0:cs0, 1:cs1 */

/* ********************** spi cs ******************************* */
#define HI3521A_SPI_NUM_CS	2

static int hi3521a_spi_cfg_cs(s16 bus_num, u8 csn)
{
	unsigned int value;
	hi_msg("\n");
	switch (bus_num) {
	case 0:
		if (csn < HI3521A_SPI_NUM_CS) {
			value = readl((void *)REG_MISC_CTRL_MAP);
			value &= ~MISC_CTRL_SSP_CS;
			value |= csn;
			writel(value, (void *)REG_MISC_CTRL_MAP);
		} else {
			dev_err(NULL, "%s, %s, %d line: error\n",
					__FILE__, __func__, __LINE__);
			return -1;
		}
		break;

	default:
		dev_err(NULL, "%s, %s, %d line: error\n",
				__FILE__, __func__, __LINE__);
		return -1;

	}
	return 0;
}

static int hi3521a_spi_hw_init_cfg(s16 bus_num)
{
	unsigned int value;

	hi_msg("\n");
	switch (bus_num) {
	case 0:
		value = readl((void *)REG_PERI_CRG33);
		value &= ~CRG33_SSP_RST;
		value |= CRG33_SSP_CKEN;
		writel(value, (void *)REG_PERI_CRG33); /* open spi0 clk */
		break;

	default:
		dev_err(NULL, "%s, %s, %d line: error\n",
				__FILE__, __func__, __LINE__);
		return -1;
	}
	return 0;
}

static int hi3521a_spi_hw_exit_cfg(s16 bus_num)
{
	unsigned int value;

	hi_msg("\n");
	switch (bus_num) {
	case 0:
		value = readl((void *)REG_PERI_CRG33);
		value |= CRG33_SSP_RST;
		value &= ~CRG33_SSP_CKEN;
		writel(value, (void *)REG_PERI_CRG33); /* close spi0 clk */
		break;

	default:
		dev_err(NULL, "%s, %s, %d line: error\n",
				__FILE__, __func__, __LINE__);
		return -1;
	}
	return 0;
}

int hi_spi_set_platdata(struct hi_spi_platform_data *spd,
		struct platform_device *pdev)
{
	hi_msg("\n");

	if ((spd == NULL) || (pdev == NULL)) {
		dev_err(NULL, "%s (spd || pdev) == NULL\n", __func__);
		return -1;
	}

	switch (pdev->id) {
	case 0:
		spd->num_cs = HI3521A_SPI_NUM_CS;
		break;

	default:
		dev_err(NULL, "%s bus num error\n", __func__);
		return -1;
	}

	spd->clk_rate = get_bus_clk() / 4;
	spd->cfg_cs = hi3521a_spi_cfg_cs;
	spd->hw_init_cfg = hi3521a_spi_hw_init_cfg;
	spd->hw_exit_cfg = hi3521a_spi_hw_exit_cfg;

	pdev->dev.platform_data = spd;

	return 0;
}
EXPORT_SYMBOL(hi_spi_set_platdata);
