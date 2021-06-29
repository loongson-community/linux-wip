// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */

#include <asm/io.h>

void __init __iomem *early_ioremap(u64 phys_addr, unsigned long size)
{
	return ((void *)TO_CAC(phys_addr));
}

void __init early_iounmap(void __iomem *addr, unsigned long size)
{

}

void *early_memremap_ro(resource_size_t phys_addr, unsigned long size)
{
	return early_memremap(phys_addr, size);
}

void *early_memremap_prot(resource_size_t phys_addr, unsigned long size,
		    unsigned long prot_val)
{
	return early_memremap(phys_addr, size);
}
