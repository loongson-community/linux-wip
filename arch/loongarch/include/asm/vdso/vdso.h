/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Author: Huacai Chen <chenhuacai@loongson.cn>
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */

#ifndef __ASSEMBLY__

#include <asm/asm.h>
#include <asm/page.h>
#include <asm/vdso.h>

static inline unsigned long get_vdso_base(void)
{
	unsigned long addr;

	/*
	 * Get the base load address of the VDSO. We have to avoid generating
	 * relocations and references to the GOT because ld.so does not peform
	 * relocations on the VDSO. We use the current offset from the VDSO base
	 * and perform a PC-relative branch which gives the absolute address in
	 * ra, and take the difference. The assembler chokes on
	 * "li.w %0, _start - .", so embed the offset as a word and branch over
	 * it.
	 *
	 */

	__asm__(
	" la.pcrel %0, _start\n"
	: "=r" (addr)
	:
	:);

	return addr;
}

static inline const struct vdso_data *get_vdso_data(void)
{
	return (const struct vdso_data *)(get_vdso_base() - PAGE_SIZE);
}

#endif /* __ASSEMBLY__ */
