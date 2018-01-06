/* hisilicon satav200 reg */
#define HI_SATA_PORT_FIFOTH	0x44
#define HI_SATA_PORT_PHYCTL1	0x48
#define HI_SATA_PORT_PHYCTL2	0x4C
#define HI_SATA_PORT_PHYCTL	0x74

#define HI_SATA_PHY0_CTLL       0xA0
#define HI_SATA_PHY0_CTLH       0xA4
#define HI_SATA_PHY1_CTLL       0xAC
#define HI_SATA_PHY1_CTLH       0xB0

#define HI_SATA_FIFOTH_VALUE	0x76d9f24
#define HI_SATA_PHY_VALUE       0x4900003d
#define HI_SATA_PHYCTL2_VALUE   0x60555
#define HI_SATA_LANE0_RESET     (1 << 18)
#define HI_SATA_PHY_REV_CLK     (1 << 9)
#define HI_SATA_BIGENDINE       (1 << 3)
#define HI_SATA_PHY_RESET       (1 << 0)

#define HI_SATA_PORT_BIGENDINE  0x82e5cb8

#define HI_SATA_PHY_MODE_1_5G	0
#define HI_SATA_PHY_MODE_3G	1
#define HI_SATA_PHY_MODE_6G	2

#define HI_SATA_PX_TX_AMPLITUDE CONFIG_HI_SATA_AMPLITUDE
#define HI_SATA_PX_TX_PREEMPH   CONFIG_HI_SATA_PREEMPH

#define HI_SATA_PHY_1_5G	0x0e180000
#define HI_SATA_PHY_3G		0x0e390000
#define HI_SATA_PHY_6G		0x0e5a0000

#define HI_SATA_FORCE_1_5G      0x2f180000
#define HI_SATA_FORCE_3G        0x2f390000
#define HI_SATA_FORCE_6G        0x2f5a0000

void hisata_phy_init(void __iomem *mmio, int phy_mode, int n_port);
