/******************************************************************************
 *    Copyright (c) 2009-2012 by Hisi.
 *    All rights reserved.
 * ***
 *
 ******************************************************************************/

#include <asm/arch/platform.h>
#include <config.h>
#ifdef CONFIG_DDR_TRAINING_V2
#include <ddr_interface.h>
#endif

#define HI_SYS_CTL_REG	0x12020000

#define DDR0_BASE_REG	0x12068000
#define DDR0_PLL_REG    0x1206c000

static inline void delay(unsigned int num)
{
	volatile unsigned int i;

	for (i = 0; i < (100 * num); i++)
		__asm__ __volatile__("nop");
}

extern void reset_cpu(unsigned long addr);

static inline void DWB(void) /* drain write buffer */
{
}

static inline unsigned int readl(unsigned addr)
{
	unsigned int val;

	val = (*(volatile unsigned int *)(addr));
	return val;
}

static inline void writel(unsigned val, unsigned addr)
{
	DWB();
	(*(volatile unsigned *) (addr)) = (val);
	DWB();
}

void hi3519_ddr_init(void)
{
	unsigned int temp1, temp2;

	temp2 = readl(HI_SYS_CTL_REG + 0xa0);
	if (temp2) {
		temp1 = readl(DDR0_BASE_REG + 0x108);
		writel((temp1 & 0xffff0000), DDR0_BASE_REG + 0x108);
		writel(temp2, DDR0_PLL_REG + 0x4);
		writel(0x2, DDR0_BASE_REG);

		while (1) {
			temp2 = readl(DDR0_PLL_REG + 0x4) & 0x1;
			if (!temp2)
				break;
		}

		writel(0x8000, DDR0_PLL_REG + 0x4);
		writel(0x0, DDR0_PLL_REG + 0x4);
		writel(temp1, DDR0_BASE_REG + 0x108);
	}

	/* for low cost*/
	/*writel(0x23201, DDR0_BASE_REG + 0x28);
	writel(0xaf501, DDR0_BASE_REG + 0x1c);
	writel(0xfe014830, DDR0_PLL_REG + 0x1e4);*/
	writel(0x2, DDR0_BASE_REG + 0x0);

	do {
		temp1 = readl(DDR0_BASE_REG + 0x294);
	} while (temp1 & 0x1);
}

void start_ddr_training(unsigned int base)
{
#ifdef CONFIG_SVB_ENABLE
	//start_svb();
#endif

	hi3519_ddr_init();

#ifdef CONFIG_DDR_TRAINING_V2
	int ret = 0;

	/* ddr training function */
	ret = ddr_sw_training_if(0);
	if (ret)
		reset_cpu(0);
#endif
	/*the value should config after trainning, or
	  it will cause chip compatibility problems*/
	writel(0x401, DDR0_BASE_REG + 0x28);
}
