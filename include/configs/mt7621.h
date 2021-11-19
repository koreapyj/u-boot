/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc. All Rights Reserved.
 *
 * Author: Weijie Gao <weijie.gao@mediatek.com>
 */

#ifndef __CONFIG_MT7621_H
#define __CONFIG_MT7621_H

#define CONFIG_SYS_HZ			1000
#define CONFIG_SYS_MIPS_TIMER_FREQ	440000000

#define CONFIG_SYS_MONITOR_BASE		CONFIG_SYS_TEXT_BASE

#define CONFIG_SYS_BOOTPARAMS_LEN	0x20000

#define CONFIG_SYS_SDRAM_BASE		0x80000000

#define CONFIG_VERY_BIG_RAM
#define CONFIG_MAX_MEM_MAPPED		0x1c000000

#define CONFIG_SYS_INIT_SP_OFFSET	0x80000

#define CONFIG_SYS_BOOTM_LEN		0x2000000

#define CONFIG_SYS_MAXARGS		16
#define CONFIG_SYS_CBSIZE		1024

/* MMC */
#define MMC_SUPPORTS_TUNING

/* NAND */
#define CONFIG_SYS_MAX_NAND_DEVICE	1

#endif /* __CONFIG_MT7621_H */
