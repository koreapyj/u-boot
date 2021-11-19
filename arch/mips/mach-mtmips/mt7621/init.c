// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc. All Rights Reserved.
 *
 * Author:  Weijie Gao <weijie.gao@mediatek.com>
 */

#include <clk.h>
#include <dm.h>
#include <dm/uclass.h>
#include <dt-bindings/clock/mt7621-clk.h>
#include <asm/global_data.h>
#include <linux/io.h>
#include "mt7621.h"

DECLARE_GLOBAL_DATA_PTR;

static const char *const boot_mode[(CHIP_MODE_M >> CHIP_MODE_S) + 1] = {
	[1] = "NAND 2K+64",
	[2] = "SPI-NOR 3-Byte Addr",
	[3] = "SPI-NOR 4-Byte Addr",
	[10] = "NAND 2K+128",
	[11] = "NAND 4K+128",
	[12] = "NAND 4K+256",
};

int print_cpuinfo(void)
{
	void __iomem *sysc = ioremap_nocache(SYSCTL_BASE, SYSCTL_SIZE);
	u32 cpu_clk, ddr_clk, bus_clk, xtal_clk, timer_freq;
	u32 val, ver, eco, pkg, core, dram, chipmode;
	struct udevice *clkdev;
	const char *bootdev;
	struct clk clk;
	int ret;

	val = readl(sysc + SYSCTL_CHIP_REV_ID_REG);
	ver = (val & VER_ID_M) >> VER_ID_S;
	eco = (val & ECO_ID_M) >> ECO_ID_S;
	pkg = !!(val & PKG_ID);
	core = !!(val & CPU_ID);

	val = readl(sysc + SYSCTL_SYSCFG0_REG);
	dram = val & DRAM_TYPE;
	chipmode = (val & CHIP_MODE_M) >> CHIP_MODE_S;

	bootdev = boot_mode[chipmode];
	if (!bootdev)
		bootdev = "Unsupported boot mode";

	printf("CPU:   MediaTek MT7621%c ver %u, eco %u\n",
	       core ? (pkg ? 'A' : 'N') : 'S', ver, eco);

	printf("Boot:  DDR%u, %s\n", dram ? 2 : 3, bootdev);

	ret = uclass_get_device_by_driver(UCLASS_CLK, DM_DRIVER_GET(mt7621_clk),
					  &clkdev);
	if (ret)
		return ret;

	clk.dev = clkdev;

	clk.id = CLK_CPU;
	cpu_clk = clk_get_rate(&clk);

	clk.id = CLK_SYS;
	bus_clk = clk_get_rate(&clk);

	clk.id = CLK_DDR;
	ddr_clk = clk_get_rate(&clk);

	clk.id = CLK_XTAL;
	xtal_clk = clk_get_rate(&clk);

	clk.id = CLK_MIPS_CNT;
	timer_freq = clk_get_rate(&clk);

	/* Set final timer frequency */
	if (timer_freq)
		gd->arch.timer_freq = timer_freq;

	printf("Clock: CPU: %uMHz, DDR: %uMT/s, Bus: %uMHz, XTAL: %uMHz\n",
	       cpu_clk / 1000000, ddr_clk / 500000, bus_clk / 1000000,
	       xtal_clk / 1000000);

	return 0;
}

void lowlevel_init(void)
{
	void __iomem *usbh = ioremap_nocache(SSUSB_BASE, SSUSB_SIZE);

	/* Setup USB xHCI */

	writel((0x20 << SSUSB_MAC3_SYS_CK_GATE_MASK_TIME_S) |
	       (0x20 << SSUSB_MAC2_SYS_CK_GATE_MASK_TIME_S) |
	       (2 << SSUSB_MAC3_SYS_CK_GATE_MODE_S) |
	       (2 << SSUSB_MAC2_SYS_CK_GATE_MODE_S) | 0x10,
	       usbh + SSUSB_MAC_CK_CTRL_REG);

	writel((2 << SSUSB_PLL_PREDIV_PE1D_S) | (1 << SSUSB_PLL_PREDIV_U3_S) |
	       (4 << SSUSB_PLL_FBKDI_S), usbh + DA_SSUSB_U3PHYA_10_REG);

	writel((0x18 << SSUSB_PLL_FBKDIV_PE2H_S) |
	       (0x18 << SSUSB_PLL_FBKDIV_PE1D_S) |
	       (0x18 << SSUSB_PLL_FBKDIV_PE1H_S) |
	       (0x1e << SSUSB_PLL_FBKDIV_U3_S),
	       usbh + DA_SSUSB_PLL_FBKDIV_REG);

	writel((0x1e400000 << SSUSB_PLL_PCW_NCPO_U3_S),
	       usbh + DA_SSUSB_PLL_PCW_NCPO_REG);

	writel((0x25 << SSUSB_PLL_SSC_DELTA1_PE1H_S) |
	       (0x73 << SSUSB_PLL_SSC_DELTA1_U3_S),
	       usbh + DA_SSUSB_PLL_SSC_DELTA1_REG);

	writel((0x71 << SSUSB_PLL_SSC_DELTA_U3_S) |
	       (0x4a << SSUSB_PLL_SSC_DELTA1_PE2D_S),
	       usbh + DA_SSUSB_U3PHYA_21_REG);

	writel((0x140 << SSUSB_PLL_SSC_PRD_S), usbh + SSUSB_U3PHYA_9_REG);

	writel((0x11c00000 << SSUSB_SYSPLL_PCW_NCPO_S),
	       usbh + SSUSB_U3PHYA_3_REG);

	writel((4 << SSUSB_PCIE_CLKDRV_AMP_S) | (1 << SSUSB_SYSPLL_FBSEL_S) |
	       (1 << SSUSB_SYSPLL_PREDIV_S), usbh + SSUSB_U3PHYA_1_REG);

	writel((0x12 << SSUSB_SYSPLL_FBDIV_S) | SSUSB_SYSPLL_VCO_DIV_SEL |
	       SSUSB_SYSPLL_FPEN | SSUSB_SYSPLL_MONCK_EN | SSUSB_SYSPLL_VOD_EN,
	       usbh + SSUSB_U3PHYA_2_REG);

	writel(SSUSB_EQ_CURSEL | (8 << SSUSB_RX_DAC_MUX_S) |
	       (1 << SSUSB_PCIE_SIGDET_VTH_S) | (1 << SSUSB_PCIE_SIGDET_LPF_S),
	       usbh + SSUSB_U3PHYA_11_REG);

	writel((0x1ff << SSUSB_RING_OSC_CNTEND_S) |
	       (0x7f << SSUSB_XTAL_OSC_CNTEND_S) | SSUSB_RING_BYPASS_DET,
	       usbh + SSUSB_B2_ROSC_0_REG);

	writel((3 << SSUSB_RING_OSC_FRC_RECAL_S) | SSUSB_RING_OSC_FRC_SEL,
	       usbh + SSUSB_B2_ROSC_1_REG);
}

ulong notrace get_tbclk(void)
{
	return gd->arch.timer_freq;
}

void _machine_restart(void)
{
	void __iomem *sysc = ioremap_nocache(SYSCTL_BASE, SYSCTL_SIZE);

	while (1)
		writel(SYS_RST, sysc + SYSCTL_RSTCTL_REG);
}
