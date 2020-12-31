/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */

#include <asm/asm.h>
#include <asm/asmmacro.h>
#include <asm/export.h>
#include <asm/regdef.h>

#define _ASM_EXTABLE(from, to)			\
	.section __ex_table, "a";		\
	PTR	from, to;			\
	.previous

/*
 * size_t __copy_user(void *to, const void *from, size_t n)
 *
 * a0: to
 * a1: from
 * a2: n
 */
SYM_FUNC_START(__copy_user)
	beqz	a2, 3f

1:	ld.b	t0, a1, 0
2:	st.b	t0, a0, 0
	addi.d	a0, a0, 1
	addi.d	a1, a1, 1
	addi.d	a2, a2, -1
	bgt	a2, zero, 1b

3:	move	v0, a2
	jr	ra

	.section .fixup, "ax"
4:	move	v0, a2
	jr	ra
	.previous

	_ASM_EXTABLE(1b, 4b)
	_ASM_EXTABLE(2b, 4b)
SYM_FUNC_END(__copy_user)

EXPORT_SYMBOL(__copy_user)
