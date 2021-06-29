/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Author: Hanlu Li <lihanlu@loongson.cn>
 *         Huacai Chen <chenhuacai@loongson.cn>
 *
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */

#ifndef __ASM_LOONGARCH_SYSCALL_H
#define __ASM_LOONGARCH_SYSCALL_H

#include <linux/compiler.h>
#include <uapi/linux/audit.h>
#include <linux/elf-em.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <asm/ptrace.h>
#include <asm/unistd.h>

extern void *sys_call_table[];

static inline long syscall_get_nr(struct task_struct *task,
				  struct pt_regs *regs)
{
	return regs->regs[11];
}

static inline void syscall_set_nr(struct task_struct *task,
				  struct pt_regs *regs,
				  int syscall)
{
	regs->regs[11] = syscall;
}

static inline long syscall_get_error(struct task_struct *task,
				     struct pt_regs *regs)
{
	return regs->regs[7] ? -regs->regs[4] : 0;
}

static inline long syscall_get_return_value(struct task_struct *task,
					    struct pt_regs *regs)
{
	return regs->regs[4];
}

static inline void syscall_rollback(struct task_struct *task,
				    struct pt_regs *regs)
{
	/* Do nothing */
}

static inline void syscall_set_return_value(struct task_struct *task,
					    struct pt_regs *regs,
					    int error, long val)
{
	regs->regs[4] = (long) error ? error : val;
}

static inline void syscall_get_arguments(struct task_struct *task,
					 struct pt_regs *regs,
					 unsigned long *args)
{
	args[0] = regs->orig_a0;
	memcpy(&args[1], &regs->regs[5], 5 * sizeof(long));
}

static inline int syscall_get_arch(struct task_struct *task)
{
	return AUDIT_ARCH_LOONGARCH64;
}

#endif	/* __ASM_LOONGARCH_SYSCALL_H */
