/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#ifndef _ASM_DMI_H
#define _ASM_DMI_H

#include <linux/io.h>
#include <linux/memblock.h>

#define dmi_early_remap		early_ioremap
#define dmi_early_unmap		early_iounmap
#define dmi_remap		dmi_ioremap
#define dmi_unmap		dmi_iounmap
#define dmi_alloc(l)		memblock_alloc_low(l, PAGE_SIZE)

void __init __iomem *dmi_ioremap(u64 phys_addr, unsigned long size)
{
	return ((void *)TO_CAC(phys_addr));
}

void dmi_iounmap(volatile void __iomem *addr)
{
}

#endif /* _ASM_DMI_H */
