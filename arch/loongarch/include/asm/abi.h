/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#ifndef _ASM_ABI_H
#define _ASM_ABI_H

#include <linux/signal_types.h>

#include <asm/signal.h>
#include <asm/siginfo.h>
#include <asm/vdso.h>

struct loongarch_abi {
	int (* const setup_frame)(void *sig_return, struct ksignal *ksig,
				  struct pt_regs *regs, sigset_t *set);
	int (* const setup_rt_frame)(void *sig_return, struct ksignal *ksig,
				     struct pt_regs *regs, sigset_t *set);

	const unsigned long	restart;
	const int		audit_arch;

	unsigned int off_sc_fpregs;
	unsigned int off_sc_fcc;
	unsigned int off_sc_fcsr;
	unsigned int off_sc_vcsr;
	unsigned int off_sc_flags;

	struct loongarch_vdso_image *vdso;
	unsigned int off_sc_scr;
};

#endif /* _ASM_ABI_H */
