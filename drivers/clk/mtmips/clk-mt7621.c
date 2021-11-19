// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc. All Rights Reserved.
 *
 * Author: Weijie Gao <weijie.gao@mediatek.com>
 */

#include <common.h>
#include <clk-uclass.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <regmap.h>
#include <syscon.h>
#include <dt-bindings/clock/mt7621-clk.h>
#include <linux/bitops.h>
#include <linux/io.h>

#define SYSC_MAP_SIZE			0x100
#define MEMC_MAP_SIZE			0x1000

/* SYSC */
#define SYSCFG0_REG			0x10
#define XTAL_MODE_SEL_S			6
#define XTAL_MODE_SEL_M			0x1c0

#define CLKCFG0_REG			0x2c
#define CPU_CLK_SEL_S			30
#define CPU_CLK_SEL_M			0xc0000000
#define PERI_CLK_SEL			0x10

#define CLKCFG1_REG			0x30

#define CUR_CLK_STS_REG			0x44
#define CUR_CPU_FDIV_S			8
#define CUR_CPU_FDIV_M			0x1f00
#define CUR_CPU_FFRAC_S			0
#define CUR_CPU_FFRAC_M			0x1f

/* MEMC */
#define MEMPLL1_REG			0x0604
#define RG_MEPL_DIV2_SEL_S		1
#define RG_MEPL_DIV2_SEL_M		0x06

#define MEMPLL6_REG			0x0618
#define MEMPLL18_REG			0x0648
#define RG_MEPL_PREDIV_S		12
#define RG_MEPL_PREDIV_M		0x3000
#define RG_MEPL_FBDIV_S			4
#define RG_MEPL_FBDIV_M			0x7f0

/* Clock sources */
#define CLK_SRC_CPU			-1
#define CLK_SRC_CPU_D2			-2
#define CLK_SRC_DDR			-3
#define CLK_SRC_SYS			-4
#define CLK_SRC_XTAL			-5
#define CLK_SRC_PERI			-6

/* EPLL clock */
#define EPLL_CLK			50000000

struct mt7621_clk_priv {
	void __iomem *sysc_base;
	void __iomem *memc_base;
	int cpu_clk;
	int ddr_clk;
	int sys_clk;
	int xtal_clk;
};

static const int mt7621_clks[] = {
	[CLK_SYS] = CLK_SRC_SYS,
	[CLK_DDR] = CLK_SRC_DDR,
	[CLK_CPU] = CLK_SRC_CPU,
	[CLK_XTAL] = CLK_SRC_XTAL,
	[CLK_MIPS_CNT] = CLK_SRC_CPU_D2,
	[CLK_UART3] = CLK_SRC_PERI,
	[CLK_UART2] = CLK_SRC_PERI,
	[CLK_UART1] = CLK_SRC_PERI,
	[CLK_SPI] = CLK_SRC_SYS,
	[CLK_I2C] = CLK_SRC_PERI,
	[CLK_TIMER] = CLK_SRC_PERI,
};

static ulong mt7621_clk_get_rate(struct clk *clk)
{
	struct mt7621_clk_priv *priv = dev_get_priv(clk->dev);
	u32 val;

	if (clk->id >= ARRAY_SIZE(mt7621_clks))
		return 0;

	switch (mt7621_clks[clk->id]) {
	case CLK_SRC_CPU:
		return priv->cpu_clk;
	case CLK_SRC_CPU_D2:
		return priv->cpu_clk / 2;
	case CLK_SRC_DDR:
		return priv->ddr_clk;
	case CLK_SRC_SYS:
		return priv->sys_clk;
	case CLK_SRC_XTAL:
		return priv->xtal_clk;
	case CLK_SRC_PERI:
		val = readl(priv->sysc_base + CLKCFG0_REG);
		if (val & PERI_CLK_SEL)
			return priv->xtal_clk;
		else
			return EPLL_CLK;
	default:
		return 0;
	}
}

static int mt7621_clk_enable(struct clk *clk)
{
	struct mt7621_clk_priv *priv = dev_get_priv(clk->dev);

	if (clk->id > 31)
		return -1;

	setbits_32(priv->sysc_base + CLKCFG1_REG, BIT(clk->id));

	return 0;
}

static int mt7621_clk_disable(struct clk *clk)
{
	struct mt7621_clk_priv *priv = dev_get_priv(clk->dev);

	if (clk->id > 31)
		return -1;

	clrbits_32(priv->sysc_base + CLKCFG1_REG, BIT(clk->id));

	return 0;
}

const struct clk_ops mt7621_clk_ops = {
	.enable = mt7621_clk_enable,
	.disable = mt7621_clk_disable,
	.get_rate = mt7621_clk_get_rate,
};

static void mt7621_get_clocks(struct mt7621_clk_priv *priv)
{
	u32 bs, xtal_sel, clkcfg0, cur_clk, mempll, dividx, fb;
	u32 xtal_clk, xtal_div, ffiv, ffrac, cpu_clk, ddr_clk;
	static const u32 xtal_div_tbl[] = {0, 1, 2, 2};

	bs = readl(priv->sysc_base + SYSCFG0_REG);
	clkcfg0 = readl(priv->sysc_base + CLKCFG0_REG);
	cur_clk = readl(priv->sysc_base + CUR_CLK_STS_REG);

	xtal_sel = (bs & XTAL_MODE_SEL_M) >> XTAL_MODE_SEL_S;

	if (xtal_sel <= 2)
		xtal_clk = 20 * 1000 * 1000;
	else if (xtal_sel <= 5)
		xtal_clk = 40 * 1000 * 1000;
	else
		xtal_clk = 25 * 1000 * 1000;

	switch ((clkcfg0 & CPU_CLK_SEL_M) >> CPU_CLK_SEL_S) {
	case 0:
		cpu_clk = 500 * 1000 * 1000;
		break;
	case 1:
		mempll = readl(priv->memc_base + MEMPLL18_REG);
		dividx = (mempll & RG_MEPL_PREDIV_M) >> RG_MEPL_PREDIV_S;
		fb = (mempll & RG_MEPL_FBDIV_M) >> RG_MEPL_FBDIV_S;
		xtal_div = 1 << xtal_div_tbl[dividx];
		cpu_clk = (fb + 1) * xtal_clk / xtal_div;
		break;
	default:
		cpu_clk = xtal_clk;
	}

	ffiv = (cur_clk & CUR_CPU_FDIV_M) >> CUR_CPU_FDIV_S;
	ffrac = (cur_clk & CUR_CPU_FFRAC_M) >> CUR_CPU_FFRAC_S;
	cpu_clk = cpu_clk / ffiv * ffrac;

	mempll = readl(priv->memc_base + MEMPLL6_REG);
	dividx = (mempll & RG_MEPL_PREDIV_M) >> RG_MEPL_PREDIV_S;
	fb = (mempll & RG_MEPL_FBDIV_M) >> RG_MEPL_FBDIV_S;
	xtal_div = 1 << xtal_div_tbl[dividx];
	ddr_clk = fb * xtal_clk / xtal_div;

	bs = readl(priv->memc_base + MEMPLL1_REG);
	if (((bs & RG_MEPL_DIV2_SEL_M) >> RG_MEPL_DIV2_SEL_S) == 0)
		ddr_clk *= 2;

	priv->cpu_clk = cpu_clk;
	priv->sys_clk = cpu_clk / 4;
	priv->ddr_clk = ddr_clk;
	priv->xtal_clk = xtal_clk;
}

static int mt7621_clk_probe(struct udevice *dev)
{
	struct mt7621_clk_priv *priv = dev_get_priv(dev);
	struct ofnode_phandle_args args;
	struct regmap *regmap;
	void __iomem *base;
	int ret;

	/* get corresponding sysc phandle */
	ret = dev_read_phandle_with_args(dev, "mediatek,sysc", NULL, 0, 0,
					 &args);
	if (ret)
		return ret;

	regmap = syscon_node_to_regmap(args.node);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	base = regmap_get_range(regmap, 0);
	if (!base) {
		dev_err(dev, "Unable to find sysc\n");
		return -ENODEV;
	}

	priv->sysc_base = ioremap_nocache((phys_addr_t)base, SYSC_MAP_SIZE);

	/* get corresponding memc phandle */
	ret = dev_read_phandle_with_args(dev, "mediatek,memc", NULL, 0, 0,
					 &args);
	if (ret)
		return ret;

	regmap = syscon_node_to_regmap(args.node);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	base = regmap_get_range(regmap, 0);
	if (!base) {
		dev_err(dev, "Unable to find memc\n");
		return -ENODEV;
	}

	priv->memc_base = ioremap_nocache((phys_addr_t)base, MEMC_MAP_SIZE);

	mt7621_get_clocks(priv);

	return 0;
}

static const struct udevice_id mt7621_clk_ids[] = {
	{ .compatible = "mediatek,mt7621-clk" },
	{ }
};

U_BOOT_DRIVER(mt7621_clk) = {
	.name = "mt7621-clk",
	.id = UCLASS_CLK,
	.of_match = mt7621_clk_ids,
	.probe = mt7621_clk_probe,
	.priv_auto = sizeof(struct mt7621_clk_priv),
	.ops = &mt7621_clk_ops,
};
