// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc. All rights reserved.
 *
 * Author:  Weijie Gao <weijie.gao@mediatek.com>
 */

#include <malloc.h>
#include <asm/io.h>
#include <asm/addrspace.h>

/* This is used for the mtk-eth driver */
phys_addr_t noncached_alloc(size_t size, size_t align)
{
	void *ptr = memalign(align, ALIGN(size, align));

	if (!ptr)
		return 0;

	return KSEG1ADDR((ulong)ptr);
}
