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
 * size_t __clear_user(void *addr, size_t size)
 *
 * a0: addr
 * a1: size
 */
SYM_FUNC_START(__clear_user)
	beqz	a1, 2f

1:	st.b	zero, a0, 0
	addi.d	a0, a0, 1
	addi.d	a1, a1, -1
	bgt	a1, zero, 1b

2:	move	v0, a1
	jr	ra

	.section .fixup, "ax"
3:	move	v0, a1
	jr	ra
	.previous

	_ASM_EXTABLE(1b, 3b)
SYM_FUNC_END(__clear_user)

EXPORT_SYMBOL(__clear_user)
