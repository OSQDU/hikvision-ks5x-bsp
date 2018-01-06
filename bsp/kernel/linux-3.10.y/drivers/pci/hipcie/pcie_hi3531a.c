
#include "pcie_hi3531a.h"

static void * __iomem misc_ctrl_virt;
static int __arch_pcie_info_setup(struct pcie_info *info, int *controllers_nr);
static int __arch_pcie_sys_init(struct pcie_info *info);
static void __arch_pcie_info_release(struct pcie_info *info);

struct pcie_iatu pcie0_iatu_table[] = {
		{
			.viewport	= 0,
			.region_ctrl_1  = 0x00000004,
			.region_ctrl_2  = 0x90000000,
			.lbar           = PCIE0_EP_CONF_BASE + (1<<20),
			.ubar           = 0x0,
			.lar            = PCIE0_EP_CONF_BASE + (2<<20) - 1,
			.ltar           = 0x01000000,
			.utar           = 0x00000000,
		},
		{
			.viewport       = 1,
			.region_ctrl_1  = 0x00000005,
			.region_ctrl_2  = 0x90000000,
			.lbar           = PCIE0_EP_CONF_BASE + (2<<20),
			.ubar           = 0x0,
			.lar            = PCIE0_EP_CONF_BASE + (__128MB__ - 1),
			.ltar           = 0x02000000,
			.utar           = 0x00000000,
		},
};

struct pcie_iatu pcie1_iatu_table[] = {
		{
			.viewport	= 0,
			.region_ctrl_1  = 0x00000004,
			.region_ctrl_2  = 0x90000000,
			.lbar           = PCIE1_EP_CONF_BASE,
			.ubar           = 0x0,
			.lar            = PCIE1_EP_CONF_BASE + (1<<20) - 1,
			.ltar           = 0x01000000,
			.utar           = 0x00000000,
		},
		{
			.viewport       = 1,
			.region_ctrl_1  = 0x00000005,
			.region_ctrl_2  = 0x90000000,
			.lbar           = PCIE1_EP_CONF_BASE,
			.ubar           = 0x0,
			.lar            = PCIE1_EP_CONF_BASE + (__128MB__ - 1),
			.ltar           = 0x02000000,
			.utar           = 0x00000000,
		},
};

static void __arch_pcie_preinit(void)
{

}

static struct resource pcie_mem[2];
static struct resource pcie_io[2];
static void __arch_get_pcie_res(int controller,
		struct resource **pmem,
		struct resource **pio)
{
	if (controller >= 2) {
		pcie_error("Pcie controller index(%d) error!", controller);
		return;
	}


	*pmem = &pcie_mem[controller];
	*pio = &pcie_io[controller];

	if (pcie_controller_0 == controller) {
		(*pmem)->start = PCIE0_MEM_BASE;
		(*pmem)->end = PCIE0_MEM_BASE + 0x8000000 - 1;
		(*pmem)->flags = IORESOURCE_MEM;
		(*pmem)->name = "memory";

		(*pio)->start = 0x0;
		(*pio)->end = 0x0;
		(*pio)->flags = IORESOURCE_IO;
		(*pio)->name = "io";
	}

	if (pcie_controller_1 == controller) {

		(*pmem)->start = PCIE1_MEM_BASE;
		(*pmem)->end = PCIE1_MEM_BASE + 0x8000000 - 1;
		(*pmem)->flags = IORESOURCE_MEM;
		(*pmem)->name = "memory";

		(*pio)->start = 0x1;
		(*pio)->end = 0x1;
		(*pio)->flags = IORESOURCE_IO;
		(*pio)->name = "io";
	}
}

static int __arch_get_int_irq(struct pcie_info *info, u8 pin)
{
	if (pcie_controller_0 == info->controller) {
		switch (pin) {
		case PCIE_INTA_PIN:
			return PCIE0_IRQ_INTA;
		case PCIE_INTB_PIN:
			return PCIE0_IRQ_INTB;
		case PCIE_INTC_PIN:
			return PCIE0_IRQ_INTC;
		case PCIE_INTD_PIN:
			return PCIE0_IRQ_INTD;
		default:
			pcie_error("Unkown pin for mapping irq!");
			return -1;
		}
	}

	if (pcie_controller_1 == info->controller) {
		switch (pin) {
		case PCIE_INTA_PIN:
			return PCIE1_IRQ_INTA;
		case PCIE_INTB_PIN:
			return PCIE1_IRQ_INTB;
		case PCIE_INTC_PIN:
			return PCIE1_IRQ_INTC;
		case PCIE_INTD_PIN:
			return PCIE1_IRQ_INTD;
		default:
			pcie_error("Unkown pin for mapping irq!");
			return -1;
		}
	}

	return -1;
}

static void __arch_config_iatu_tbl(struct pcie_info *info,
		struct pci_sys_data *sys)
{
	int i;
	void __iomem *config_base;
	struct pcie_iatu *ptable;
	int table_size;
	unsigned int ctl1_lbar_offset;

	config_base = (void __iomem *)info->conf_base_addr;

	if (pcie_controller_0 == info->controller) {
		ptable = pcie0_iatu_table;
		table_size = ARRAY_SIZE(pcie0_iatu_table);
	}

	if (pcie_controller_1 == info->controller) {
		ptable = pcie1_iatu_table;
		table_size = ARRAY_SIZE(pcie1_iatu_table);

		ctl1_lbar_offset = (sys->busnr + 1) << 20;
		ptable->lbar |= ctl1_lbar_offset;
		ptable->lar |= ctl1_lbar_offset;

		ctl1_lbar_offset = (sys->busnr + 2) << 20;
		(ptable + 1)->lbar |= ctl1_lbar_offset;
	}

	for (i = 0; i < table_size; i++) {
		writel((ptable + i)->viewport, config_base + 0x900);
		writel((ptable + i)->lbar, config_base + 0x90c);
		writel((ptable + i)->ubar, config_base + 0x910);
		writel((ptable + i)->lar,  config_base + 0x914);
		writel((ptable + i)->ltar, config_base + 0x918);
		writel((ptable + i)->utar, config_base + 0x91c);
		writel((ptable + i)->region_ctrl_1, config_base + 0x904);
		writel((ptable + i)->region_ctrl_2, config_base + 0x908);
	}

}

static inline int __arch_check_pcie_link(struct pcie_info *info)
{
	int val;

	udelay(2000);

	if (pcie_controller_0 == info->controller) {
		val = readl(misc_ctrl_virt + PCIE0_SYS_STATE0);
		return ((val & (1 << PCIE_XMLH_LINK_UP))
				&& (val & (1 << PCIE_RDLH_LINK_UP))) ? 1 : 0;
	}

	if (pcie_controller_1 == info->controller) {
		val = readl(misc_ctrl_virt + PCIE1_SYS_STATE0);
		return ((val & (1 << PCIE_XMLH_LINK_UP))
				&& (val & (1 << PCIE_RDLH_LINK_UP))) ? 1 : 0;
	}

	return 0;
}

/*
 * ret:
 */
static int __arch_pcie_info_set(struct pcie_info *info, int controller)
{
	unsigned int pcie_mem_size;
	unsigned int pcie_cfg_size;
	unsigned int pcie_dbi_base;
	unsigned int pcie_ep_conf_base;

#ifdef CONFIG_PCIE0_SEL
	if (pcie_controller_0 == controller) {
		pcie_mem_size = CONFIG_PCIE0_DEVICES_MEM_SIZE;
		pcie_cfg_size = CONFIG_PCIE0_DEVICES_CONFIG_SIZE;
		pcie_dbi_base = PCIE0_DBI_BASE;
		pcie_ep_conf_base = PCIE0_EP_CONF_BASE;
	}
#endif
#ifdef CONFIG_PCIE1_SEL
	if (pcie_controller_1 == controller) {
		pcie_mem_size = CONFIG_PCIE1_DEVICES_MEM_SIZE;
		pcie_cfg_size = CONFIG_PCIE1_DEVICES_CONFIG_SIZE;
		pcie_dbi_base = PCIE1_DBI_BASE;
		pcie_ep_conf_base = PCIE1_EP_CONF_BASE;
	}
#endif

	if ((pcie_mem_size > __128MB__) || (pcie_cfg_size > __128MB__)) {
		pcie_error(
		"Invalid parameter: pcie mem size[0x%x], pcie cfg size[0x%x]!",
		pcie_mem_size, pcie_cfg_size);
		return -EINVAL;
	}

	info->controller = controller;

	/* RC configuration space */
	info->conf_base_addr = (unsigned int)ioremap_nocache(pcie_dbi_base,
			__4KB__);
	if (!info->conf_base_addr) {
		pcie_error("Address mapping for RC dbi failed!");
		return -EIO;
	}

	/* Configuration space for all EPs */
	info->base_addr = (unsigned int)ioremap_nocache(pcie_ep_conf_base,
			pcie_cfg_size);
	if (!info->base_addr) {
		iounmap((void *)info->conf_base_addr);
		pcie_error("Address mapping for EPs cfg failed!");
		return -EIO;
	}

	return 0;
}

static void __arch_pcie_info_clr(struct pcie_info *info)
{
	if (info->base_addr)
		iounmap((void *)info->base_addr);

	if (info->conf_base_addr)
		iounmap((void *)info->conf_base_addr);
}

static int  __arch_get_port_nr(void)
{
	unsigned int val, mode;
	int nr;

	val = readl((void *)PCIE_SYS_STAT);
	mode = (val >> 12) & 0xf;
	switch (mode) {
	case 0x1:
	case 0x2:
	case 0x9:
	case 0xa:
		nr = 1;
		break;

	case 0x3:
	case 0x4:
	case 0xb:
	case 0xc:
		nr = 2;
		break;

	default:
		nr = 0;
		break;
	}

	return nr;
}

static int __arch_pcie_info_setup(struct pcie_info *info, int *controllers_nr)
{
	int nr;

	misc_ctrl_virt = (void *)IO_ADDRESS(MISC_CTRL_BASE);

	nr = __arch_get_port_nr();
	if (!nr) {
		pr_err("Pcie port number: 0\n");
		return -EINVAL;
	}
	*controllers_nr = 0;

#ifdef CONFIG_PCIE0_SEL
	if (__arch_pcie_info_set(&info[*controllers_nr], pcie_controller_0))
		return -EIO;
	(*controllers_nr)++;
#endif
#ifdef CONFIG_PCIE1_SEL
	if (nr < 2) {
		pr_err("1-PCIE port mode, skip PCIE1 even config pcie1-sel!\n");
		return 0;
	}
	if (__arch_pcie_info_set(&info[*controllers_nr], pcie_controller_1)) {
#ifdef CONFIG_PCIE0_SEL
		__arch_pcie_info_clr(&info[pcie_controller_0]);
#endif
		return -EIO;
	}
	(*controllers_nr)++;
#endif

	return 0;
}

static void __arch_pcie_info_release(struct pcie_info *info)
{
	int nr;
	for (nr = 0; nr < pcie_controllers_nr; nr++)
		__arch_pcie_info_clr(&info[nr]);
}

static void __arch_pcie_sys_config(struct pcie_info *info)
{
	unsigned int val;
	void *dbi_base = (void *)info->conf_base_addr;
	void *crg_base = (void *)IO_ADDRESS(PERI_CRG_BASE);

	if (pcie_controller_0 == info->controller) {
		/*
		 * Disable PCIE
		 */
		val = readl(misc_ctrl_virt + PCIE0_SYS_CTRL7);
		val &= (~(1 << PCIE_APP_LTSSM_ENBALE));
		writel(val, misc_ctrl_virt + PCIE0_SYS_CTRL7);

		/*
		 * Reset
		 */
		val = readl(crg_base + PERI_CRG73);
		val |= (1 << PCIE0_X2_SRST_REQ);
		writel(val, crg_base + PERI_CRG73);

		/*
		 * Retreat from the reset state
		 */
		udelay(500);
		val = readl(crg_base + PERI_CRG73);
		val &= ~(1 << PCIE0_X2_SRST_REQ);
		writel(val, crg_base + PERI_CRG73);
		mdelay(10);

		/*
		 * PCIE RC work mode
		 */
		val = readl(misc_ctrl_virt + PCIE0_SYS_CTRL0);
		val &= (~(0xf << PCIE_DEVICE_TYPE));
		val |= (PCIE_WM_RC << PCIE_DEVICE_TYPE);
		writel(val, misc_ctrl_virt + PCIE0_SYS_CTRL0);

		/*
		 * Enable clk
		 */
		val = readl(crg_base + PERI_CRG73);
		val |= ((1 << PCIE0_X2_BUS_CKEN)
				| (1 << PCIE0_X2_SYS_CKEN)
				| (1 << PCIE0_X2_PIPE_CKEN)
				| (1 << PCIE0_X2_AUX_CKEN));
		writel(val, crg_base + PERI_CRG73);
		mdelay(10);

		val = readl(dbi_base + LINK_CTRL2_STATUS2);
		val &= ~((0xF << 12) | (0x7 << 7) | (0x1 << 6));
		val |= ((0x1 << 12) | (0x1 << 7) | (0x1 << 6));
		writel(val, dbi_base + LINK_CTRL2_STATUS2);

		/*
		 * Enable controller
		 */
		val = readl(misc_ctrl_virt + PCIE0_SYS_CTRL7);
		val |= (1 << PCIE_APP_LTSSM_ENBALE);
		writel(val, misc_ctrl_virt + PCIE0_SYS_CTRL7);
		udelay(1000);

	}

	if (pcie_controller_1 == info->controller) {
		/*
		 * Disable PCIE
		 */
		val = readl(misc_ctrl_virt + PCIE1_SYS_CTRL7);
		val &= (~(1 << PCIE_APP_LTSSM_ENBALE));
		writel(val, misc_ctrl_virt + PCIE1_SYS_CTRL7);

		/*
		 * Reset
		 */
		val = readl(crg_base + PERI_CRG73);
		val |= (1 << PCIE1_X2_SRST_REQ);
		writel(val, crg_base + PERI_CRG73);

		/*
		 * Retreat from the reset state
		 */
		udelay(500);
		val = readl(crg_base + PERI_CRG73);
		val &= ~(1 << PCIE1_X2_SRST_REQ);
		writel(val, crg_base + PERI_CRG73);
		mdelay(10);

		/*
		 * PCIE RC work mode
		 */
		val = readl(misc_ctrl_virt + PCIE1_SYS_CTRL0);
		val &= (~(0xf << PCIE_DEVICE_TYPE));
		val |= (PCIE_WM_RC << PCIE_DEVICE_TYPE);
		writel(val, misc_ctrl_virt + PCIE1_SYS_CTRL0);

		/*
		 * Enable clk
		 */
		val = readl(crg_base + PERI_CRG73);
		val |= ((1 << PCIE1_X2_BUS_CKEN)
				| (1 << PCIE1_X2_SYS_CKEN)
				| (1 << PCIE1_X2_PIPE_CKEN)
				| (1 << PCIE1_X2_AUX_CKEN));
		writel(val, crg_base + PERI_CRG73);

		val = readl(dbi_base + LINK_CTRL2_STATUS2);
		val &= ~((0xF << 12) | (0x7 << 7) | (0x1 << 6));
		val |= ((0x1 << 12) | (0x1 << 7) | (0x1 << 6));
		writel(val, dbi_base + LINK_CTRL2_STATUS2);

		/*
		 * Enable controller
		 */
		val = readl(misc_ctrl_virt + PCIE1_SYS_CTRL7);
		val |= (1 << PCIE_APP_LTSSM_ENBALE);
		writel(val, misc_ctrl_virt + PCIE1_SYS_CTRL7);
		udelay(1000);
	}

	/*
	 * Set PCIE controller class code to be PCI-PCI bridge device
	 */
	val = readl(dbi_base + PCI_CLASS_REVISION);
	val &= ~(0xffffff00);
	val |= (0x60400 << 8);
	writel(val, dbi_base + PCI_CLASS_REVISION);
	udelay(1000);

	val = readl(dbi_base + PCI_COMMAND);
	val |= 7;
	writel(val, dbi_base + PCI_COMMAND);
}

static int __arch_pcie_sys_init(struct pcie_info *info)
{
	int nr;
	for (nr = 0; nr < pcie_controllers_nr; nr++)
		__arch_pcie_sys_config(&info[nr]);

	return 0;
}

static void __arch_pcie_sys_exit(void)
{
	void *crg_base = (void *)IO_ADDRESS(PERI_CRG_BASE);
	unsigned int val;

#ifdef CONFIG_PCIE0_SEL
	/*
	 * Disable PCIE
	 */
	val = readl(misc_ctrl_virt + PCIE0_SYS_CTRL7);
	val &= (~(1 << PCIE_APP_LTSSM_ENBALE));
	writel(val, misc_ctrl_virt + PCIE0_SYS_CTRL7);

	/*
	 * Reset
	 */
	val = readl(crg_base + PERI_CRG73);
	val |= (1 << PCIE0_X2_SRST_REQ);
	writel(val, crg_base + PERI_CRG73);

	udelay(1000);

	/*
	 * Disable clk
	 */
	val = readl(crg_base + PERI_CRG73);
	val &= (~(1 << PCIE0_X2_AUX_CKEN));
	val &= (~(1 << PCIE0_X2_PIPE_CKEN));
	val &= (~(1 << PCIE0_X2_SYS_CKEN));
	val &= (~(1 << PCIE0_X2_BUS_CKEN));
	writel(val, crg_base + PERI_CRG73);

	udelay(1000);
#endif

#ifdef CONFIG_PCIE1_SEL
	/*
	 * Disable PCIE
	 */
	val = readl(misc_ctrl_virt + PCIE1_SYS_CTRL7);
	val &= (~(1 << PCIE_APP_LTSSM_ENBALE));
	writel(val, misc_ctrl_virt + PCIE1_SYS_CTRL7);

	/*
	 * Reset
	 */
	val = readl(crg_base + PERI_CRG73);
	val |= (1 << PCIE1_X2_SRST_REQ);
	writel(val, crg_base + PERI_CRG73);

	udelay(1000);

	/*
	 * Disable clk
	 */
	val = readl(crg_base + PERI_CRG73);
	val &= (~(1 << PCIE1_X2_AUX_CKEN));
	val &= (~(1 << PCIE1_X2_PIPE_CKEN));
	val &= (~(1 << PCIE1_X2_SYS_CKEN));
	val &= (~(1 << PCIE1_X2_BUS_CKEN));
	writel(val, crg_base + PERI_CRG73);

	udelay(1000);
#endif
}

