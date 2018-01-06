/*****************************************************************************
 *    Copyright (c) 2009-2011 by HiC
 *    All rights reserved.
 *****************************************************************************/

/*****************************************************************************/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <asm/setup.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#include "../../mtdcore.h"
#include "../spi_ids.h"
#include "hisfc350.h"

static inline int mtd_has_partitions(void) { return 1; }
#define THREE_BYTE_ADDR_BOOT 0

#ifdef CONFIG_ARCH_HI3536
#include "hisfc350_hi3536.c"
#endif

#ifndef GET_SFC_ADDR_MODE
#define GET_SFC_ADDR_MODE  (0)
#endif

/* Don't change the follow config */
#define HISFC350_SUPPORT_READ (SPI_IF_READ_STD \
	| SPI_IF_READ_FAST \
	| SPI_IF_READ_DUAL \
	| SPI_IF_READ_DUAL_ADDR \
	| SPI_IF_READ_QUAD \
	| SPI_IF_READ_QUAD_ADDR)

#define HISFC350_SUPPORT_WRITE (SPI_IF_WRITE_STD \
	| SPI_IF_WRITE_DUAL \
	| SPI_IF_WRITE_DUAL_ADDR \
	| SPI_IF_WRITE_QUAD \
	| SPI_IF_WRITE_QUAD_ADDR)

#define HISFC350_SUPPORT_MAX_DUMMY        (7)

#ifdef CONFIG_HISFC350_ENABLE_INTR_DMA
static loff_t sfc_ft;
static size_t sfc_length;
static int sfc_num;
static unsigned char *sfc_offset;
static unsigned int sfc_rw;
#endif

static int  start_up_mode;

static char *ultohstr(unsigned long long size)
{
	int ix;
	static char buffer[20];
	char *fmt[] = {"%u", "%uK", "%uM", "%uG", "%uT", "%uT"};

	for (ix = 0; (ix < 5) && !(size & 0x3FF) && size; ix++)
		size = (size >> 10);

	sprintf(buffer, fmt[ix], size);
	return buffer;
}

#ifdef CONFIG_HISFC350_SHOW_CYCLE_TIMING
static char *hisfc350_get_ifcycle_str(int ifcycle)
{
	static char *ifcycle_str[] = {
		"single",
		"dual",
		"dual-addr",
		"dual-cmd",
		"reserve",
		"quad",
		"quad-addr",
		"quad-cmd",
	};

	return ifcycle_str[(ifcycle & 0x07)];
}
#endif

static void hisfc350_set_host_addr_mode(struct hisfc_host *host, int enable)
{
	unsigned int regval;
	regval = hisfc_read(host, HISFC350_GLOBAL_CONFIG);
	if (enable)
		regval |= HISFC350_GLOBAL_CONFIG_ADDR_MODE_4B;
	else
		regval &= ~HISFC350_GLOBAL_CONFIG_ADDR_MODE_4B;

	hisfc_write(host, HISFC350_GLOBAL_CONFIG, regval);
}

static void hisfc350_driver_shutdown(struct platform_device *pltdev)
{
	if (start_up_mode == THREE_BYTE_ADDR_BOOT) {
		int ix;

		struct hisfc_host *host = platform_get_drvdata(pltdev);
		struct hisfc_spi *spi = host->spi;

		for (ix = 0; ix < host->num_chip; ix++, spi++) {
			if (spi->addrcycle == SPI_4BYTE_ADDR_LEN) {
				spi->driver->wait_ready(spi);
				spi->driver->entry_4addr(spi, 0);
			}
		}
	}
}
static void hisfc350_map_iftype_and_clock(struct hisfc_spi *spi)
{
	int ix;
	const int iftype_read[] = {
		SPI_IF_READ_STD,       HISFC350_IFCYCLE_STD,
		SPI_IF_READ_FAST,      HISFC350_IFCYCLE_STD,
		SPI_IF_READ_DUAL,      HISFC350_IFCYCLE_DUAL,
		SPI_IF_READ_DUAL_ADDR, HISFC350_IFCYCLE_DUAL_ADDR,
		SPI_IF_READ_QUAD,      HISFC350_IFCYCLE_QUAD,
		SPI_IF_READ_QUAD_ADDR, HISFC350_IFCYCLE_QUAD_ADDR,
		0, 0,
	};
	const int iftype_write[] = {
		SPI_IF_WRITE_STD,       HISFC350_IFCYCLE_STD,
		SPI_IF_WRITE_DUAL,      HISFC350_IFCYCLE_DUAL,
		SPI_IF_WRITE_DUAL_ADDR, HISFC350_IFCYCLE_DUAL_ADDR,
		SPI_IF_WRITE_QUAD,      HISFC350_IFCYCLE_QUAD,
		SPI_IF_WRITE_QUAD_ADDR, HISFC350_IFCYCLE_QUAD_ADDR,
		0, 0,
	};

	for (ix = 0; iftype_write[ix]; ix += 2) {
		if (spi->write->iftype == iftype_write[ix]) {
			spi->write->iftype = iftype_write[ix + 1];
			break;
		}
	}
	hisfc350_get_best_clock(&spi->write->clock);

	for (ix = 0; iftype_read[ix]; ix += 2) {
		if (spi->read->iftype == iftype_read[ix]) {
			spi->read->iftype = iftype_read[ix + 1];
			break;
		}
	}
	hisfc350_get_best_clock(&spi->read->clock);

	hisfc350_get_best_clock(&spi->erase->clock);
	spi->erase->iftype = HISFC350_IFCYCLE_STD;
}

static void hisfc350_dma_transfer(struct hisfc_host *host,
	unsigned int spi_start_addr, unsigned char *dma_buffer,
	unsigned char is_read, unsigned int size, unsigned char chipselect)
{
	hisfc_write(host, HISFC350_BUS_DMA_MEM_SADDR, dma_buffer);

	hisfc_write(host, HISFC350_BUS_DMA_FLASH_SADDR,
		spi_start_addr);

	hisfc_write(host, HISFC350_BUS_DMA_LEN,
		HISFC350_BUS_DMA_LEN_DATA_CNT(size));

	hisfc_write(host, HISFC350_BUS_DMA_AHB_CTRL,
		HISFC350_BUS_DMA_AHB_CTRL_INCR4_EN
		| HISFC350_BUS_DMA_AHB_CTRL_INCR8_EN
		| HISFC350_BUS_DMA_AHB_CTRL_INCR16_EN);

	hisfc_write(host, HISFC350_BUS_DMA_CTRL,
		HISFC350_BUS_DMA_CTRL_RW(is_read)
		| HISFC350_BUS_DMA_CTRL_CS(chipselect)
		| HISFC350_BUS_DMA_CTRL_START);

#ifndef CONFIG_HISFC350_ENABLE_INTR_DMA
	HISFC350_DMA_WAIT_CPU_FINISH(host);
#endif
}

#ifdef HISFCV350_SUPPORT_REG_READ
static char *hisfc350_reg_read_buf(struct hisfc_host *host,
	struct hisfc_spi *spi, unsigned int spi_start_addr,
	unsigned int size, unsigned char *buffer)
{
	int index = 0;

	if (size > HISFC350_REG_BUF_SIZE)
		DBG_BUG("reg read out of reg range.\n");

	hisfc_write(host, HISFC350_CMD_INS, spi->read->cmd);
	hisfc_write(host, HISFC350_CMD_ADDR,
		(spi_start_addr & HISFC350_CMD_ADDR_MASK));
	hisfc_write(host, HISFC350_CMD_CONFIG,
		HISFC350_CMD_CONFIG_MEM_IF_TYPE(spi->read->iftype)
		| HISFC350_CMD_CONFIG_DATA_CNT(size)
		| HISFC350_CMD_CONFIG_RW_READ
		| HISFC350_CMD_CONFIG_DATA_EN
		| HISFC350_CMD_CONFIG_DUMMY_CNT(spi->read->dummy)
		| HISFC350_CMD_CONFIG_ADDR_EN
		| HISFC350_CMD_CONFIG_SEL_CS(spi->chipselect)
		| HISFC350_CMD_CONFIG_START);

	HISFC350_CMD_WAIT_CPU_FINISH(host);

	while (index < size) {
		*(unsigned int *)(host->reg_buffer + index) = hisfc_read(host,
			HISFC350_CMD_DATABUF0 + index);
		index    += 4;
	}

	memcpy(buffer, host->reg_buffer, size);

	return buffer;
}

static int hisfc350_reg_read(struct mtd_info *mtd, loff_t from, size_t len,
	size_t *retlen, u_char *buf)
{
	int num;
	int result = -EIO;
	unsigned char *ptr = buf;
	struct hisfc_host *host = MTD_TO_HOST(mtd);
	struct hisfc_spi *spi = host->spi;

	if ((from + len) > mtd->size) {
		DBG_MSG("read area out of range.\n");
		return -EINVAL;
	}

	*retlen = 0;
	if (!len) {
		DBG_MSG("read length is 0.\n");
		return 0;
	}
	mutex_lock(&host->lock);
#ifdef CONFIG_SPI_TYPE_SWITCH
	mutex_lock(&spi_type_mutex);
	mtd_switch_spi_type(mtd);
#endif

	if (spi->driver->wait_ready(spi))
		goto fail;
	host->set_system_clock(host, spi->read, TRUE);

	while (len > 0) {
		while (from >= spi->chipsize) {
			from -= spi->chipsize;
			spi++;
			if (!spi->name)
				DBG_BUG("read memory out of range.\n");
			if (spi->driver->wait_ready(spi))
				goto fail;
			host->set_system_clock(host, spi->read, TRUE);
		}

		num = ((from + len) >= spi->chipsize)
			? (spi->chipsize - from) : len;

		while (num >= HISFC350_REG_BUF_SIZE) {
			hisfc350_reg_read_buf(host, spi,
				from, HISFC350_REG_BUF_SIZE, ptr);
			ptr  += HISFC350_REG_BUF_SIZE;
			from += HISFC350_REG_BUF_SIZE;
			len  -= HISFC350_REG_BUF_SIZE;
			num  -= HISFC350_REG_BUF_SIZE;
		}

		if (num) {
			hisfc350_reg_read_buf(host, spi,
				from, num, ptr);
			from += num;
			ptr  += num;
			len  -= num;
		}
	}
	result = 0;
	*retlen = (size_t)(ptr - buf);
fail:
#ifdef CONFIG_SPI_TYPE_SWITCH
	mutex_unlock(&spi_type_mutex);
#endif
	mutex_unlock(&host->lock);
	return result;
}
#endif /* HISFCV350_SUPPORT_REG_READ */

#ifdef CONFIG_HISFC350_ENABLE_INTR_DMA
static int hisfc350_dma_intr_read(struct mtd_info *mtd, loff_t from, size_t len,
	size_t *retlen, u_char *buf)
{
	int result = -EIO;
	struct hisfc_host *host = MTD_TO_HOST(mtd);
	struct hisfc_spi *spi = host->spi;

	if ((from + len) > mtd->size) {
		DBG_MSG("read area out of range.\n");
		return -EINVAL;
	}

	*retlen = 0;
	if (!len) {
		DBG_MSG("read length is 0.\n");
		return 0;
	}

	mutex_lock(&host->lock);
#ifdef CONFIG_SPI_TYPE_SWITCH
	mutex_lock(&spi_type_mutex);
	mtd_switch_spi_type(mtd);
#endif

	sfc_offset = (unsigned char *)buf;
	sfc_ft = from;
	sfc_length = len;
	sfc_rw = READ;
	if (spi->driver->wait_ready(spi))
		goto fail;
	spi->driver->bus_prepare(spi, READ);

	if (sfc_ft & HISFC350_DMA_ALIGN_MASK) {
		sfc_num = HISFC350_DMA_ALIGN_SIZE -
			(sfc_ft & HISFC350_DMA_ALIGN_MASK);
		if (sfc_num > sfc_length)
			sfc_num = sfc_length;
		while (sfc_ft >= spi->chipsize) {
			sfc_ft -= spi->chipsize;
			spi++;
			if (!spi->name)
				DBG_BUG("write memory out of range.\n");
			if (spi->driver->wait_ready(spi))
				goto fail;
			spi->driver->bus_prepare(spi, sfc_rw);
		}
	} else{
		while (sfc_ft >= spi->chipsize) {
			sfc_ft -= spi->chipsize;
			spi++;
			if (!spi->name)
				DBG_BUG("read memory out of range.\n");
			if (spi->driver->wait_ready(spi))
				goto fail;
			spi->driver->bus_prepare(spi, sfc_rw);
		}
		if ((sfc_ft + sfc_length) >= spi->chipsize) {
			sfc_num = spi->chipsize - sfc_ft;
			if (sfc_num >= HISFC350_DMA_MAX_SIZE)
				sfc_num = HISFC350_DMA_MAX_SIZE;
		} else{
			sfc_num = sfc_length;
			if (sfc_num >= HISFC350_DMA_MAX_SIZE)
				sfc_num = HISFC350_DMA_MAX_SIZE;
		}
	}

	hisfc350_dma_transfer(host, sfc_ft,
		(unsigned char *)host->dma_buffer, sfc_rw,
		sfc_num, spi->chipselect);
	wait_event(host->intr_wait, host->wait_fg == SFC_WAIT_FLAG_R);
	host->wait_fg = 0;
	result = 0;
	*retlen = (size_t)(sfc_offset - buf);
fail:
#ifdef CONFIG_SPI_TYPE_SWITCH
	mutex_unlock(&spi_type_mutex);
#endif
	mutex_unlock(&host->lock);
	return result;
}
#else
static int hisfc350_dma_read(struct mtd_info *mtd, loff_t from, size_t len,
	size_t *retlen, u_char *buf)
{
	int num;
	int result = -EIO;
	unsigned char *ptr = buf;
	struct hisfc_host *host = MTD_TO_HOST(mtd);
	struct hisfc_spi *spi = host->spi;

	if ((from + len) > mtd->size) {
		DBG_MSG("read area out of range.\n");
		return -EINVAL;
	}

	*retlen = 0;
	if (!len) {
		DBG_MSG("read length is 0.\n");
		return 0;
	}

	mutex_lock(&host->lock);
#ifdef CONFIG_SPI_TYPE_SWITCH
	mutex_lock(&spi_type_mutex);
	mtd_switch_spi_type(mtd);
#endif

	if (spi->driver->wait_ready(spi))
		goto fail;
	spi->driver->bus_prepare(spi, READ);

	if (from & HISFC350_DMA_ALIGN_MASK) {
		num = HISFC350_DMA_ALIGN_SIZE -
			(from & HISFC350_DMA_ALIGN_MASK);
		if (num > len)
			num = len;
		while (from >= spi->chipsize) {
			from -= spi->chipsize;
			spi++;
			if (!spi->name)
				DBG_BUG("write memory out of range.\n");
			if (spi->driver->wait_ready(spi))
				goto fail;
			spi->driver->bus_prepare(spi, READ);
		}
		hisfc350_dma_transfer(host, from,
			(unsigned char *)host->dma_buffer, READ,
			num, spi->chipselect);
		memcpy(ptr, host->buffer, num);
		from  += num;
		ptr += num;
		len -= num;
	}

	while (len > 0) {
		while (from >= spi->chipsize) {
			from -= spi->chipsize;
			spi++;
			if (!spi->name)
				DBG_BUG("read memory out of range.\n");
			if (spi->driver->wait_ready(spi))
				goto fail;
			spi->driver->bus_prepare(spi, READ);
		}

		num = ((from + len) >= spi->chipsize)
			? (spi->chipsize - from) : len;
		while (num >= HISFC350_DMA_MAX_SIZE) {
			hisfc350_dma_transfer(host, from,
				(unsigned char *)host->dma_buffer, READ,
				HISFC350_DMA_MAX_SIZE, spi->chipselect);
			memcpy(ptr, host->buffer, HISFC350_DMA_MAX_SIZE);
			ptr  += HISFC350_DMA_MAX_SIZE;
			from += HISFC350_DMA_MAX_SIZE;
			len  -= HISFC350_DMA_MAX_SIZE;
			num  -= HISFC350_DMA_MAX_SIZE;
		}

		if (num) {
			hisfc350_dma_transfer(host, from,
				(unsigned char *)host->dma_buffer, READ,
				num, spi->chipselect);
			memcpy(ptr, host->buffer, num);
			from += num;
			ptr  += num;
			len  -= num;
		}
	}
	result = 0;
	*retlen = (size_t)(ptr - buf);
fail:
#ifdef CONFIG_SPI_TYPE_SWITCH
	mutex_unlock(&spi_type_mutex);
#endif
	mutex_unlock(&host->lock);
	return result;
}
#endif

static unsigned char *hisfc350_read_ids(struct hisfc_host *host,
	int chipselect, unsigned char *buffer)
{
	int regindex = 0;
	int numread = 8;
	unsigned int *ptr = (unsigned int *)buffer;

	if (numread > HISFC350_REG_BUF_SIZE)
		numread = HISFC350_REG_BUF_SIZE;

	hisfc_write(host, HISFC350_CMD_INS, SPI_CMD_RDID);
	hisfc_write(host, HISFC350_CMD_CONFIG,
		HISFC350_CMD_CONFIG_SEL_CS(chipselect)
		| HISFC350_CMD_CONFIG_RW_READ
		| HISFC350_CMD_CONFIG_DATA_EN
		| HISFC350_CMD_CONFIG_DATA_CNT(numread)
		| HISFC350_CMD_CONFIG_START);

	HISFC350_CMD_WAIT_CPU_FINISH(host);

	while (numread) {
		*ptr = hisfc_read(host,
			HISFC350_CMD_DATABUF0 + regindex);
		ptr      += 1;
		regindex += 4;
		numread  -= 4;
	}

	return buffer;
}

static int hisfc350_reg_erase_one_block(struct hisfc_host *host,
	struct hisfc_spi *spi, unsigned int offset)
{
	if (spi->driver->wait_ready(spi))
		return 1;

	spi->driver->write_enable(spi);
	host->set_system_clock(host, spi->erase, TRUE);

	hisfc_write(host, HISFC350_CMD_INS, spi->erase->cmd);

	hisfc_write(host, HISFC350_CMD_ADDR,
		(offset & HISFC350_CMD_ADDR_MASK));

	hisfc_write(host, HISFC350_CMD_CONFIG,
		HISFC350_CMD_CONFIG_SEL_CS(spi->chipselect)
		| HISFC350_CMD_CONFIG_MEM_IF_TYPE(spi->erase->iftype)
		| HISFC350_CMD_CONFIG_DUMMY_CNT(spi->erase->dummy)
		| HISFC350_CMD_CONFIG_ADDR_EN
		| HISFC350_CMD_CONFIG_START);

	HISFC350_CMD_WAIT_CPU_FINISH(host);

	return 0;
}

#ifdef CONFIG_HISFC350_ENABLE_INTR_DMA
static int hisfc350_dma_intr_write(struct mtd_info *mtd, loff_t to, size_t len,
	size_t *retlen, const u_char *buf)
{
	int result = -EIO;
	struct hisfc_host *host = MTD_TO_HOST(mtd);
	struct hisfc_spi *spi = host->spi;

	if ((to + len) > mtd->size) {
		DBG_MSG("write data out of range.\n");
		return -EINVAL;
	}

	*retlen = 0;
	if (!len) {
		DBG_MSG("write length is 0.\n");
		return 0;
	}

	mutex_lock(&host->lock);
#ifdef CONFIG_SPI_TYPE_SWITCH
	mutex_lock(&spi_type_mutex);
	mtd_switch_spi_type(mtd);
#endif

	sfc_offset = (unsigned char *)buf;
	sfc_ft = to;
	sfc_length = len;
	sfc_rw = WRITE;
	if (spi->driver->wait_ready(spi))
		goto fail;

	spi->driver->write_enable(spi);
	spi->driver->bus_prepare(spi, sfc_rw);

	if (sfc_ft & HISFC350_DMA_ALIGN_MASK) {
		sfc_num = HISFC350_DMA_ALIGN_SIZE
			- (sfc_ft & HISFC350_DMA_ALIGN_MASK);
		if (sfc_num > sfc_length)
			sfc_num = sfc_length;
		while (sfc_ft >= spi->chipsize) {
			sfc_ft -= spi->chipsize;
			spi++;
			if (!spi->name)
				DBG_BUG("write memory out of range.\n");
			if (spi->driver->wait_ready(spi))
				goto fail;
			spi->driver->write_enable(spi);
			spi->driver->bus_prepare(spi, sfc_rw);
		}
	} else{
		while (sfc_ft >= spi->chipsize) {
			sfc_ft -= spi->chipsize;
			spi++;
			if (!spi->name)
				DBG_BUG("read memory out of range.\n");
			if (spi->driver->wait_ready(spi))
				goto fail;
			spi->driver->write_enable(spi);
			spi->driver->bus_prepare(spi, sfc_rw);
		}
		if ((sfc_ft + sfc_length) >= spi->chipsize) {
			sfc_num = spi->chipsize - sfc_ft;
			if (sfc_num >= HISFC350_DMA_MAX_SIZE)
				sfc_num = HISFC350_DMA_MAX_SIZE;
		} else{
			sfc_num = sfc_length;
			if (sfc_num >= HISFC350_DMA_MAX_SIZE)
				sfc_num = HISFC350_DMA_MAX_SIZE;
		}
	}

	memcpy(host->buffer, sfc_offset, sfc_num);
	hisfc350_dma_transfer(host, sfc_ft,
		(unsigned char *)host->dma_buffer, sfc_rw,
		sfc_num, spi->chipselect);
	wait_event(host->intr_wait, host->wait_fg == SFC_WAIT_FLAG_W);
	host->wait_fg = 0;
	*retlen = (size_t)(sfc_offset - buf);
	result = 0;
fail:
#ifdef CONFIG_SPI_TYPE_SWITCH
	mutex_unlock(&spi_type_mutex);
#endif
	mutex_unlock(&host->lock);
	return result;
}
#else
static int hisfc350_dma_write(struct mtd_info *mtd, loff_t to, size_t len,
	size_t *retlen, const u_char *buf)
{
	int num;
	int result = -EIO;

	unsigned char *ptr = (unsigned char *)buf;
	struct hisfc_host *host = MTD_TO_HOST(mtd);
	struct hisfc_spi *spi = host->spi;

	if ((to + len) > mtd->size) {
		DBG_MSG("write data out of range.\n");
		return -EINVAL;
	}

	*retlen = 0;
	if (!len) {
		DBG_MSG("write length is 0.\n");
		return 0;
	}

	mutex_lock(&host->lock);
#ifdef CONFIG_SPI_TYPE_SWITCH
	mutex_lock(&spi_type_mutex);
	mtd_switch_spi_type(mtd);
#endif

	if (spi->driver->wait_ready(spi))
		goto fail;

	spi->driver->write_enable(spi);
	spi->driver->bus_prepare(spi, WRITE);

	if (to & HISFC350_DMA_ALIGN_MASK) {
		num = HISFC350_DMA_ALIGN_SIZE - (to & HISFC350_DMA_ALIGN_MASK);
		if (num > len)
			num = len;
		while (to >= spi->chipsize) {
			to -= spi->chipsize;
			spi++;
			if (!spi->name)
				DBG_BUG("write memory out of range.\n");
			if (spi->driver->wait_ready(spi))
				goto fail;
			spi->driver->write_enable(spi);
			spi->driver->bus_prepare(spi, WRITE);
		}
		memcpy(host->buffer, ptr, num);
		hisfc350_dma_transfer(host, to,
			(unsigned char *)host->dma_buffer, WRITE,
			num, spi->chipselect);

		to  += num;
		ptr += num;
		len -= num;
	}

	while (len > 0) {
		num = ((len >= HISFC350_DMA_MAX_SIZE)
			? HISFC350_DMA_MAX_SIZE : len);
		while (to >= spi->chipsize) {
			to -= spi->chipsize;
			spi++;
			if (!spi->name)
				DBG_BUG("write memory out of range.\n");
			if (spi->driver->wait_ready(spi))
				goto fail;
			spi->driver->write_enable(spi);
			spi->driver->bus_prepare(spi, WRITE);
		}

		memcpy(host->buffer, ptr, num);
		hisfc350_dma_transfer(host, to,
			(unsigned char *)host->dma_buffer, WRITE,
			num, spi->chipselect);

		to  += num;
		ptr += num;
		len -= num;
	}
	*retlen = (size_t)(ptr - buf);
	result = 0;
fail:
#ifdef CONFIG_SPI_TYPE_SWITCH
	mutex_unlock(&spi_type_mutex);
#endif
	mutex_unlock(&host->lock);
	return result;
}
#endif

#ifdef HISFCV350_SUPPORT_REG_WRITE
static int hisfc350_reg_write_buf(struct hisfc_host *host,
	struct hisfc_spi *spi, unsigned int spi_start_addr,
	unsigned int size, unsigned char *buffer)
{
	int index = 0;

	if (size > HISFC350_REG_BUF_SIZE)
		DBG_BUG("reg read out of reg range.\n");

	if (spi->driver->wait_ready(spi))
		return 1;

	memcpy(host->reg_buffer, buffer, size);

	while (index < size) {
		hisfc_write(host, HISFC350_CMD_DATABUF0 + index,
			*(unsigned int *)(host->reg_buffer + index));
		index    += 4;
	}

	spi->driver->write_enable(spi);

	hisfc_write(host, HISFC350_CMD_INS, spi->write->cmd);
	hisfc_write(host, HISFC350_CMD_ADDR,
		(spi_start_addr & HISFC350_CMD_ADDR_MASK));
	hisfc_write(host, HISFC350_CMD_CONFIG,
		HISFC350_CMD_CONFIG_MEM_IF_TYPE(spi->write->iftype)
		| HISFC350_CMD_CONFIG_DATA_CNT(size)
		| HISFC350_CMD_CONFIG_DATA_EN
		| HISFC350_CMD_CONFIG_DUMMY_CNT(spi->write->dummy)
		| HISFC350_CMD_CONFIG_ADDR_EN
		| HISFC350_CMD_CONFIG_SEL_CS(spi->chipselect)
		| HISFC350_CMD_CONFIG_START);

	HISFC350_CMD_WAIT_CPU_FINISH(host);

	return 0;
}

static int hisfc350_reg_write(struct mtd_info *mtd, loff_t to, size_t len,
	size_t *retlen, const u_char *buf)
{
	int num;
	int result = -EIO;
	unsigned char *ptr = (unsigned char *)buf;
	struct hisfc_host *host = MTD_TO_HOST(mtd);
	struct hisfc_spi *spi = host->spi;

	if ((to + len) > mtd->size) {
		DBG_MSG("write data out of range.\n");
		return -EINVAL;
	}

	*retlen = 0;
	if (!len) {
		DBG_MSG("write length is 0.\n");
		return 0;
	}

	mutex_lock(&host->lock);
#ifdef CONFIG_SPI_TYPE_SWITCH
	mutex_lock(&spi_type_mutex);
	mtd_switch_spi_type(mtd);
#endif

	if (spi->driver->wait_ready(spi))
		goto fail;

	host->set_system_clock(host, spi->write, TRUE);

	if (to & HISFC350_REG_BUF_MASK) {
		num = HISFC350_REG_BUF_SIZE - (to & HISFC350_REG_BUF_MASK);
		if (num > (int)len)
			num = (int)len;

		while (to >= spi->chipsize) {
			to -= spi->chipsize;
			spi++;
			if (!spi->name)
				DBG_BUG("write memory out of range.\n");

			if (spi->driver->wait_ready(spi))
				goto fail;

			host->set_system_clock(host, spi->write, TRUE);
		}
		if (hisfc350_reg_write_buf(host, spi, to, num, ptr))
			goto fail;
		to  += num;
		ptr += num;
		len -= num;
	}

	while (len > 0)	{
		num = ((len >= HISFC350_REG_BUF_SIZE) ?
				HISFC350_REG_BUF_SIZE : len);
		while (to >= spi->chipsize) {
			to -= spi->chipsize;
			spi++;
			if (!spi->name)
				DBG_BUG("write memory out of range.\n");

			if (spi->driver->wait_ready(spi))
				goto fail;

			host->set_system_clock(host, spi->write, TRUE);
		}
		if (hisfc350_reg_write_buf(host, spi, to, num, ptr))
			goto fail;
		to  += num;
		ptr += num;
		len -= num;
	}
	*retlen = (size_t)(ptr - buf);
	result = 0;
fail:
#ifdef CONFIG_SPI_TYPE_SWITCH
	mutex_unlock(&spi_type_mutex);
#endif
	mutex_unlock(&host->lock);
	return result;
}
#endif /* HISFCV350_SUPPORT_REG_WRITE */

static int hisfc350_reg_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct hisfc_host *host = MTD_TO_HOST(mtd);
	struct hisfc_spi *spi = host->spi;

	unsigned long long offset = instr->addr;
	unsigned long long length = instr->len;

	if (offset + length > mtd->size) {
		DBG_MSG("erase area out of range of mtd.\n");
		return -EINVAL;
	}

	if ((unsigned int)offset & (mtd->erasesize-1)) {
		DBG_MSG("erase start address is not alignment.\n");
		return -EINVAL;
	}

	if ((unsigned int)length & (mtd->erasesize-1)) {
		DBG_MSG("erase length is not alignment.\n");
		return -EINVAL;
	}

	mutex_lock(&host->lock);
#ifdef CONFIG_SPI_TYPE_SWITCH
	mutex_lock(&spi_type_mutex);
	mtd_switch_spi_type(mtd);
#endif

	while (length) {
		if (spi->chipsize <= offset) {
			offset -= spi->chipsize;
			spi++;
			if (!spi->name)
				DBG_BUG("erase memory out of range.\n");
		}

		if (hisfc350_reg_erase_one_block(host, spi, offset)) {
			instr->state = MTD_ERASE_FAILED;
#ifdef CONFIG_SPI_TYPE_SWITCH
			mutex_unlock(&spi_type_mutex);
#endif
			mutex_unlock(&host->lock);
			return -EIO;
		}

		offset += spi->erase->size;
		length -= spi->erase->size;
	}

	instr->state = MTD_ERASE_DONE;
#ifdef CONFIG_SPI_TYPE_SWITCH
	mutex_unlock(&spi_type_mutex);
#endif
	mutex_unlock(&host->lock);
	mtd_erase_callback(instr);

	return 0;
}

#ifdef HISFCV350_SUPPORT_BUS_READ
static int hisfc350_bus_read(struct mtd_info *mtd, loff_t from, size_t len,
		size_t *retlen, u_char *buf)
{
	int num;
	int result = -EIO;
	unsigned char *ptr = buf;
	struct hisfc_host *host = MTD_TO_HOST(mtd);
	struct hisfc_spi *spi = host->spi;

	if ((from + len) > mtd->size) {
		DBG_MSG("read area out of range.\n");
		return -EINVAL;
	}

	*retlen = 0;
	if (!len) {
		DBG_MSG("read length is 0.\n");
		return 0;
	}

	mutex_lock(&host->lock);
#ifdef CONFIG_SPI_TYPE_SWITCH
	mutex_lock(&spi_type_mutex);
	mtd_switch_spi_type(mtd);
#endif

	if (spi->driver->wait_ready(spi))
		goto fail;
	spi->driver->bus_prepare(spi, READ);

	while (len > 0) {
		while (from >= spi->chipsize) {
			from -= spi->chipsize;
			spi++;
			if (!spi->name)
				DBG_BUG("read memory out of range.\n");

			if (spi->driver->wait_ready(spi))
				goto fail;
			spi->driver->bus_prepare(spi, READ);
		}

		num = ((from + len) >= spi->chipsize)
				? (spi->chipsize - from) : len;

		if (num) {
			memcpy(ptr, (char *)spi->iobase + from, num);
			from += num;
			ptr  += num;
			len  -= num;
		}
	}
	*retlen = (size_t)(ptr - buf);
	result = 0;
fail:
#ifdef CONFIG_SPI_TYPE_SWITCH
	mutex_unlock(&spi_type_mutex);
#endif
	mutex_unlock(&host->lock);
	return result;
}
#endif /* HISFCV350_SUPPORT_BUS_READ */

#ifdef HISFCV350_SUPPORT_BUS_WRITE
static int hisfc350_bus_write(struct mtd_info *mtd, loff_t to, size_t len,
		size_t *retlen, u_char *buf)
{
	int num;
	int result = -EIO;
	unsigned char *ptr = buf;
	struct hisfc_host *host = MTD_TO_HOST(mtd);
	struct hisfc_spi *spi = host->spi;

	if ((to + len) > mtd->size) {
		DBG_MSG("write data out of range.\n");
		return -EINVAL;
	}

	*retlen = 0;
	if (!len) {
		DBG_MSG("write length is 0.\n");
		return 0;
	}

	mutex_lock(&host->lock);
#ifdef CONFIG_SPI_TYPE_SWITCH
	mutex_lock(&spi_type_mutex);
	mtd_switch_spi_type(mtd);
#endif

	if (spi->driver->wait_ready(spi))
		goto fail;
	spi->driver->bus_prepare(spi, WRITE);

	while (len > 0) {
		while (to >= spi->chipsize) {
			to -= spi->chipsize;
			spi++;
			if (!spi->name)
				DBG_BUG("write spi space out of range.\n");

			if (spi->driver->wait_ready(spi))
				goto fail;
			spi->driver->bus_prepare(spi, WRITE);
		}

		num = ((to + len) >= spi->chipsize)
				? (spi->chipsize - to) : len;

		if (num) {
			memcpy((char *)spi->iobase + to, ptr, num);
			ptr += num;
			to += num;
			len -= num;
		}
	}

	*retlen = (size_t)(ptr - buf);
	result = 0;
fail:
#ifdef CONFIG_SPI_TYPE_SWITCH
	mutex_unlock(&spi_type_mutex);
#endif
	mutex_unlock(&host->lock);
	return result;
}
#endif

static int hisfc350_map_chipsize(unsigned long long chipsize)
{
	int shift = 0;
	chipsize >>= (19 - 3); /* 19: 512K; 3: Bytes -> bit */

	while (chipsize) {
		chipsize >>= 1;
		shift++;
	}
	return shift;
}

#ifdef CONFIG_HISFC350_ENABLE_INTR_DMA
static irqreturn_t hisfc_irq(int irq, void *dev_id)
{
	struct hisfc_host *host = dev_id;
	struct hisfc_spi *spi = host->spi;
	u32 state = 0;
	unsigned int tmp_reg = 0;

	state = hisfc_read(host, SFC_RINTSTS);
	/* clear interrupt */
	tmp_reg = hisfc_read(host, SFC_INTCLR);
	tmp_reg |= ALL_INT_CLR;
	hisfc_write(host , SFC_INTCLR , tmp_reg);

	if (sfc_rw == READ)
		memcpy(sfc_offset, host->buffer, sfc_num);

	sfc_ft += sfc_num;
	sfc_offset += sfc_num;
	sfc_length -= sfc_num;

	if (state & SFC_DMA_INT_STATUS) {
		if (sfc_length > 0) {
			while (sfc_ft >= spi->chipsize) {
				sfc_ft -= spi->chipsize;
				spi++;
				if (sfc_rw == WRITE)
					spi->driver->write_enable(spi);
				spi->driver->bus_prepare(spi, sfc_rw);
			}
			if ((sfc_ft + sfc_length) >= spi->chipsize) {
				sfc_num = spi->chipsize - sfc_ft;
				if (sfc_num >= HISFC350_DMA_MAX_SIZE)
					sfc_num = HISFC350_DMA_MAX_SIZE;
			} else{
				sfc_num = sfc_length;
				if (sfc_num >= HISFC350_DMA_MAX_SIZE)
					sfc_num = HISFC350_DMA_MAX_SIZE;
			}
			if (sfc_rw == WRITE)
				memcpy(host->buffer, sfc_offset, sfc_num);
			hisfc350_dma_transfer(host, sfc_ft,
				(unsigned char *)host->dma_buffer, sfc_rw,
				sfc_num, spi->chipselect);
		} else{
			if (sfc_rw == READ)
				host->wait_fg = SFC_WAIT_FLAG_R;
			else if (sfc_rw == WRITE)
				host->wait_fg = SFC_WAIT_FLAG_W;
			wake_up(&host->intr_wait);
		}
	}
	return IRQ_HANDLED;
}
#endif

static int hisfc350_spi_probe(struct hisfc_host *host)
{
	unsigned int regval = 0;
	unsigned int total = 0;
	unsigned char ids[8];
	struct spi_info *spiinfo;
	struct hisfc_spi *spi = host->spi;
	int chipselect = (CONFIG_HISFC350_CHIP_NUM - 1);

	host->num_chip = 0;

	for (; chipselect >= 0; chipselect--) {

		hisfc350_read_ids(host, chipselect, ids);
		/* can't find spi flash device. */
		if (!(ids[0] | ids[1] | ids[2])
			|| ((ids[0] & ids[1] & ids[2]) == 0xFF))
			continue;

		pr_info("Spi(cs%d) ID: 0x%02X 0x%02X 0x%02X",
				chipselect, ids[0], ids[1], ids[2]);
		pr_info(" 0x%02X 0x%02X 0x%02X\n",
				ids[3], ids[4], ids[5]);

		spiinfo = spi_serach_ids(ids);

		if (spiinfo) {
			spi->name = spiinfo->name;
			spi->chipselect = chipselect;
			spi->chipsize   = spiinfo->chipsize;
			spi->erasesize  = spiinfo->erasesize;
			spi->addrcycle  = spiinfo->addrcycle;
			spi->driver     = spiinfo->driver;
			spi->host       = host;

			spi_search_rw(spiinfo, spi->read,
				HISFC350_SUPPORT_READ,
				HISFC350_SUPPORT_MAX_DUMMY, READ);
			hisfc350_map_iftype_and_clock(spi);

			spi->driver->qe_enable(spi);

			spi_search_rw(spiinfo, spi->read,
				HISFC350_SUPPORT_READ,
				HISFC350_SUPPORT_MAX_DUMMY, READ);

			spi_search_rw(spiinfo, spi->write,
				HISFC350_SUPPORT_WRITE,
				HISFC350_SUPPORT_MAX_DUMMY, WRITE);

			spi_get_erase(spiinfo, spi->erase);
			hisfc350_map_iftype_and_clock(spi);

			regval = hisfc_read(host, HISFC350_BUS_FLASH_SIZE);
			regval &= ~(HISFC350_BUS_FLASH_SIZE_CS0_MASK
				<< (chipselect << 3));
			regval |= (hisfc350_map_chipsize(spi->chipsize)
				<< (chipselect << 3));
			hisfc_write(host, HISFC350_BUS_FLASH_SIZE, regval);

			hisfc_write(host,
				(HISFC350_BUS_BASE_ADDR_CS0
					+ (chipselect << 2)),
				(CONFIG_HISFC350_BUFFER_BASE_ADDRESS + total));

			spi->iobase = (char *)host->iobase + total;

			/* auto check sfc_addr_mode 3 bytes or 4 bytes */
			start_up_mode = GET_SFC_ADDR_MODE;

			if (start_up_mode == THREE_BYTE_ADDR_BOOT) {
				pr_info("SPI nor flash boot mode is 3 Bytes\n");
				spi->driver->entry_4addr(spi, TRUE);
			} else
				pr_info("SPI nor flash boot mode is 4 Bytes\n");

			pr_info("Spi(cs%d): ", spi->chipselect);
			pr_info("Block:%sB", ultohstr(spi->erasesize));
			pr_info("Chip:%sB ", ultohstr(spi->chipsize));
			pr_info("Name:\"%s\"\n", spi->name);

#ifdef CONFIG_HISFC350_SHOW_CYCLE_TIMING

			pr_info("Spi(cs%d): ", spi->chipselect);
			if (spi->addrcycle == SPI_4BYTE_ADDR_LEN)
				pr_info("4 addrcycle ");
			pr_info("read:%s,%02X,%s ",
				hisfc350_get_ifcycle_str(spi->read->iftype),
				spi->read->cmd,
				hisfc350_get_clock_str(spi->read->clock));
			pr_info("write:%s,%02X,%s ",
				hisfc350_get_ifcycle_str(spi->write->iftype),
				spi->write->cmd,
				hisfc350_get_clock_str(spi->write->clock));
			pr_info("erase:%s,%02X,%s\n",
				hisfc350_get_ifcycle_str(spi->erase[0].iftype),
				spi->erase[0].cmd,
				hisfc350_get_clock_str(spi->erase[0].clock));

#endif /* CONFIG_HISFC350_SHOW_CYCLE_TIMING */
			host->num_chip++;
			total += spi->chipsize;
			spi++;
		} else
			pr_err("Spi(cs%d): find unrecognized spi flash.\n",
				chipselect);
	}

	return host->num_chip;
}

#define MAX_MTD_PARTITIONS        (32)

struct partition_entry {
	char name[16];
	unsigned long long start;
	unsigned long long length;
	unsigned int flags;
};

static void hisfc_probe_spi_size(struct hisfc_host *host,
	struct mtd_info *mtd)
{
	int ix = 1;
	struct hisfc_spi *spi = host->spi;

	int total     = spi->chipsize;
	int erasesize = spi->erasesize;

	for (++spi; ix < host->num_chip; ix++, spi++)
		total += spi->chipsize;

	mtd->size = total;
	mtd->erasesize = erasesize;

	pr_info("spi size: %sB\n", ultohstr(mtd->size));
	pr_info("chip num: %x\n", host->num_chip);
}

static int hisfc350_probe(struct hisfc_host *host)
{
	struct mtd_info *mtd = host->mtd;

	host->set_system_clock(host, NULL, TRUE);
	hisfc_write(host, HISFC350_TIMING,
		HISFC350_TIMING_TCSS(0x6)
		| HISFC350_TIMING_TCSH(0x6)
		| HISFC350_TIMING_TSHSL(0xf));

	if (!hisfc350_spi_probe(host))
		return -1;

	mtd->_erase = hisfc350_reg_erase;
#ifdef HISFCV350_SUPPORT_REG_WRITE
	mtd->_write = hisfc350_reg_write;
#elif defined HISFCV350_SUPPORT_BUS_WRITE
	mtd->_write = hisfc350_bus_write;
#elif defined CONFIG_HISFC350_ENABLE_INTR_DMA
	mtd->_write = hisfc350_dma_intr_write;
#else
	mtd->_write = hisfc350_dma_write;
#endif
#ifdef HISFCV350_SUPPORT_REG_READ
	mtd->_read  = hisfc350_reg_read;
#elif defined HISFCV350_SUPPORT_BUS_READ
	mtd->_read  = hisfc350_bus_read;
#elif defined CONFIG_HISFC350_ENABLE_INTR_DMA
	mtd->_read  = hisfc350_dma_intr_read;
#else
	mtd->_read  = hisfc350_dma_read;
#endif

	return 0;
}

static int hisfc350_driver_probe(struct platform_device *plat_dev)
{
	int result = -EIO;
	struct hisfc_host *host;
	struct mtd_info   *mtd;
	int nr_parts = 0;
	struct mtd_partition *parts = NULL;
	int ret;
#ifdef CONFIG_HISFC350_ENABLE_INTR_DMA
	int irq;
	unsigned int tmp_reg = 0;
#endif

	host = kmalloc(sizeof(struct hisfc_host), GFP_KERNEL);
	if (!host)
		return -ENOMEM;
	memset(host, 0, sizeof(struct hisfc_host));

	platform_set_drvdata(plat_dev, host);

	host->sysreg = ioremap_nocache(CONFIG_HISFC350_SYSCTRL_ADDRESS,
		HISFC350_SYSCTRL_LENGTH);
	if (!host->sysreg) {
		pr_err("spi system reg ioremap failed.\n");
		goto fail;
	}

	host->regbase = ioremap_nocache(CONFIG_HISFC350_REG_BASE_ADDRESS,
		HISFC350_REG_BASE_LEN);
	host->set_system_clock = hisfc350_set_system_clock;
	host->set_host_addr_mode = hisfc350_set_host_addr_mode;
	if (!host->regbase) {
		pr_err("spi base reg ioremap failed.\n");
		goto fail;
	}

#ifdef HISFCV350_SUPPORT_BUS_READ
	host->iobase = ioremap_nocache(CONFIG_HISFC350_BUFFER_BASE_ADDRESS,
		HISFC350_BUFFER_BASE_LEN);
	if (!host->iobase) {
		pr_err("spi buffer ioremap failed.\n");
		goto fail;
	}
#endif

	host->buffer = dma_alloc_coherent(host->dev, HISFC350_DMA_MAX_SIZE,
		&host->dma_buffer, GFP_KERNEL);
	if (host->buffer == NULL) {
		pr_err("spi alloc dma buffer failed.\n");
		goto fail;
	}

	mutex_init(&host->lock);
	mtd = host->mtd;
	mtd->name = (char *)plat_dev->name;
	mtd->type = MTD_NORFLASH;
	mtd->writesize = 1;
	mtd->flags = MTD_CAP_NORFLASH;
	mtd->owner = THIS_MODULE;

	if (hisfc350_probe(host)) {
		result = -ENODEV;
		goto fail;
	}

	hisfc_probe_spi_size(host, mtd);

	if (mtd_has_partitions()) {

		static char const *part_probes[] = {
			"cmdlinepart",
			NULL,
		};

		nr_parts = parse_mtd_partitions(host->mtd,
				part_probes, &parts, 0);

		if (nr_parts > 0)
			host->add_partition = 1;
	}
	ret = mtd_device_register(host->mtd, parts, nr_parts);

	kfree(parts);
	parts = NULL;

#ifdef CONFIG_HISFC350_ENABLE_INTR_DMA
	host->wait_fg = 0;
	init_waitqueue_head(&host->intr_wait);

	/* clear SFC intr */
	tmp_reg |= ALL_INT_CLR;
	hisfc_write(host , SFC_INTCLR , tmp_reg);
	/* MASK SFC host intr */
	tmp_reg = hisfc_read(host , SFC_INTMASK);
	tmp_reg |= SFC_DMA_INT_MASK;
	tmp_reg &= ~SFC_CMD_INT_MASK;
	hisfc_write(host , SFC_INTMASK , tmp_reg);

	irq = SFC_IRQ_NUM;
	result = request_irq(irq, hisfc_irq, IRQF_SHARED, "hisfc", host);
	if (result) {
		pr_err("request_irq error!\n");
		goto fail;
	}
#endif
	return (1 == ret) ? -ENODEV : 0;

fail:
	if (host->regbase)
		iounmap(host->regbase);
	if (host->iobase)
		iounmap(host->iobase);
	if (host->buffer)
		dma_free_coherent(host->dev, HISFC350_DMA_MAX_SIZE,
			host->buffer, host->dma_buffer);
	if (host->sysreg)
		iounmap(host->sysreg);
	kfree(host);
	platform_set_drvdata(plat_dev, NULL);
	return result;
}

static int hisfc350_driver_remove(struct platform_device *plat_dev)
{
	struct hisfc_host *host = platform_get_drvdata(plat_dev);
#ifdef CONFIG_HISFC350_ENABLE_INTR_DMA
	int irq = SFC_IRQ_NUM;
	free_irq(irq, host);
#endif

	mtd_device_unregister(host->mtd);

	if (host->regbase)
		iounmap(host->regbase);
	if (host->iobase)
		iounmap(host->iobase);
	if (host->buffer)
		dma_free_coherent(host->dev, HISFC350_DMA_MAX_SIZE,
			host->buffer, host->dma_buffer);
	if (host->sysreg)
		iounmap(host->sysreg);

	kfree(host);
	platform_set_drvdata(plat_dev, NULL);

	return 0;
}


static void hisfc350_pltdev_release(struct device *dev)
{
}

#ifdef CONFIG_PM

static int hisfc350_driver_suspend(struct platform_device *dev,
		pm_message_t state)
{
	return 0;
}
static int hisfc350_driver_resume(struct platform_device *dev)
{
	return 0;
}

#endif

static struct platform_driver hisfc350_driver_pltdrv = {
	.probe		= hisfc350_driver_probe,
	.remove		= hisfc350_driver_remove,
	.shutdown	= hisfc350_driver_shutdown,
#ifdef CONFIG_PM
	.suspend	= hisfc350_driver_suspend,
	.resume		= hisfc350_driver_resume,
#endif
	.driver.name	= "hi_sfc",
	.driver.owner	= THIS_MODULE,
	.driver.bus	= &platform_bus_type,
};

static struct resource hisfc350_device_resources[] = {
	[0] = {
		.start = CONFIG_HISFC350_REG_BASE_ADDRESS,
		.end   = CONFIG_HISFC350_REG_BASE_ADDRESS
			+ HISFC350_REG_BASE_LEN - 1,
		.flags = IORESOURCE_MEM,
	},

	[1] = {
		.start = CONFIG_HISFC350_BUFFER_BASE_ADDRESS,
		.end   = CONFIG_HISFC350_BUFFER_BASE_ADDRESS
			+ HISFC350_BUFFER_BASE_LEN - 1,
		.flags = IORESOURCE_MEM,
	},
};

static u64 hisfc_dmamask = DMA_BIT_MASK(32);

static struct platform_device hisfc350_device_pltdev = {
	.name           = "hi_sfc",
	.id             = -1,

	.dev.release    = hisfc350_pltdev_release,
	.dev.dma_mask = &hisfc_dmamask,
	.dev.coherent_dma_mask = DMA_BIT_MASK(32),
	.num_resources  = ARRAY_SIZE(hisfc350_device_resources),
	.resource       = hisfc350_device_resources,
};

static int __init hisfc350_module_init(void)
{
	int result = 0;

	pr_debug("Hisfc350 Device Driver, Version 1.10\n");

	result = platform_driver_register(&hisfc350_driver_pltdrv);
	if (result < 0)
		return result;

	result = platform_device_register(&hisfc350_device_pltdev);
	if (result < 0) {
		platform_driver_unregister(&hisfc350_driver_pltdrv);
		return result;
	}

	return result;
}

static void __exit hisfc350_module_exit(void)
{
	platform_device_unregister(&hisfc350_device_pltdev);
	platform_driver_unregister(&hisfc350_driver_pltdrv);
}

module_init(hisfc350_module_init);
module_exit(hisfc350_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HiC");
MODULE_DESCRIPTION("Hisfc350 Device Driver, Version 1.00");
