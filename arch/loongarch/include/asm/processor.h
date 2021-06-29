/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#ifndef _ASM_PROCESSOR_H
#define _ASM_PROCESSOR_H

#include <linux/atomic.h>
#include <linux/cpumask.h>
#include <linux/sizes.h>
#include <linux/threads.h>

#include <asm/cpu.h>
#include <asm/cpu-info.h>
#include <asm/loongarchregs.h>
#include <asm/vdso/processor.h>
#include <uapi/asm/ptrace.h>

/*
 * LoongArch does have an arch_pick_mmap_layout()
 */
#define HAVE_ARCH_PICK_MMAP_LAYOUT 1

#ifdef CONFIG_32BIT
#define TASK_SIZE	0x80000000UL
#define STACK_TOP_MAX	TASK_SIZE

#define TASK_IS_32BIT_ADDR 1

#endif

#ifdef CONFIG_64BIT
#define TASK_SIZE32	0x80000000UL

#ifdef CONFIG_VA_BITS_40
#define TASK_SIZE64     (0x1UL << ((cpu_vabits > 40)?40:cpu_vabits))
#endif
#ifdef CONFIG_VA_BITS_48
#define TASK_SIZE64     (0x1UL << ((cpu_vabits > 48)?48:cpu_vabits))
#endif

#define TASK_SIZE (test_thread_flag(TIF_32BIT_ADDR) ? TASK_SIZE32 : TASK_SIZE64)
#define STACK_TOP_MAX	TASK_SIZE64

#define TASK_SIZE_OF(tsk)						\
	(test_tsk_thread_flag(tsk, TIF_32BIT_ADDR) ? TASK_SIZE32 : TASK_SIZE64)

#define TASK_IS_32BIT_ADDR test_thread_flag(TIF_32BIT_ADDR)

#endif

#define VDSO_RANDOMIZE_SIZE	(TASK_IS_32BIT_ADDR ? SZ_1M : SZ_64M)

unsigned long stack_top(void);
#define STACK_TOP stack_top()

/*
 * This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE PAGE_ALIGN(TASK_SIZE / 3)

#define NUM_FPU_REGS	32
#define FPU_REG_WIDTH	256

union fpureg {
	__u32	val32[FPU_REG_WIDTH / 32];
	__u64	val64[FPU_REG_WIDTH / 64];
};

#define FPR_IDX(width, idx)	(idx)

#define BUILD_FPR_ACCESS(width) \
static inline u##width get_fpr##width(union fpureg *fpr, unsigned idx)	\
{									\
	return fpr->val##width[FPR_IDX(width, idx)];			\
}									\
									\
static inline void set_fpr##width(union fpureg *fpr, unsigned int idx,	\
				  u##width val)				\
{									\
	fpr->val##width[FPR_IDX(width, idx)] = val;			\
}

BUILD_FPR_ACCESS(32)
BUILD_FPR_ACCESS(64)

struct loongarch_fpu {
	unsigned int	fcsr;
	unsigned int	vcsr;
	uint64_t	fcc;	/* 8x8 */
	union fpureg	fpr[NUM_FPU_REGS];
};

#define INIT_CPUMASK { \
	{0,} \
}

#define ARCH_MIN_TASKALIGN	32
#define FPU_ALIGN		__aligned(32)

struct loongarch_abi;

/*
 * If you change thread_struct remember to change the #defines below too!
 */
struct thread_struct {
	/* Saved main processor registers. */
	unsigned long reg01, reg03, reg22; /* ra sp fp */
	unsigned long reg23, reg24, reg25, reg26; /* s0-s3 */
	unsigned long reg27, reg28, reg29, reg30, reg31; /* s4-s8 */

	/* Saved csr registers */
	unsigned long csr_prmd;
	unsigned long csr_crmd;
	unsigned long csr_euen;
	unsigned long csr_ecfg;

	/* Saved scratch registers */
	unsigned long scr0;
	unsigned long scr1;
	unsigned long scr2;
	unsigned long scr3;

	/* Saved eflag register */
	unsigned long eflag;

	/* Other stuff associated with the thread. */
	unsigned long csr_badvaddr;	/* Last user fault */
	unsigned long csr_baduaddr;	/* Last kernel fault accessing user space */
	unsigned long error_code;
	unsigned long trap_nr;
	struct loongarch_abi *abi;

	/*
	 * Saved fpu register stuff, must be at last because
	 * it is conditionally copied at fork.
	 */
	struct loongarch_fpu fpu FPU_ALIGN;
};

#define INIT_THREAD  {						\
	/*							\
	 * Saved main processor registers			\
	 */							\
	.reg01			= 0,				\
	.reg03			= 0,				\
	.reg22			= 0,				\
	.reg23			= 0,				\
	.reg24			= 0,				\
	.reg25			= 0,				\
	.reg26			= 0,				\
	.reg27			= 0,				\
	.reg28			= 0,				\
	.reg29			= 0,				\
	.reg30			= 0,				\
	.reg31			= 0,				\
	.csr_crmd		= 0,				\
	.csr_prmd		= 0,				\
	.csr_euen		= 0,				\
	.csr_ecfg		= 0,				\
	/*							\
	 * Other stuff associated with the process		\
	 */							\
	.csr_badvaddr		= 0,				\
	.csr_baduaddr		= 0,				\
	.error_code		= 0,				\
	.trap_nr		= 0,				\
	/*							\
	 * Saved fpu register stuff				\
	 */							\
	.fpu			= {				\
		.fcsr		= 0,				\
		.vcsr		= 0,				\
		.fcc		= 0,				\
		.fpr		= {{{0,},},},			\
	},							\
}

struct task_struct;

/* Free all resources held by a thread. */
#define release_thread(thread) do { } while (0)

enum idle_boot_override {IDLE_NO_OVERRIDE = 0, IDLE_HALT, IDLE_NOMWAIT, IDLE_POLL};

extern unsigned long		boot_option_idle_override;
/*
 * Do necessary setup to start up a newly executed thread.
 */
extern void start_thread(struct pt_regs *regs, unsigned long pc, unsigned long sp);

static inline void flush_thread(void)
{
}

unsigned long get_wchan(struct task_struct *p);

#define __KSTK_TOS(tsk) ((unsigned long)task_stack_page(tsk) + \
			 THREAD_SIZE - 32 - sizeof(struct pt_regs))
#define task_pt_regs(tsk) ((struct pt_regs *)__KSTK_TOS(tsk))
#define KSTK_EIP(tsk) (task_pt_regs(tsk)->csr_epc)
#define KSTK_ESP(tsk) (task_pt_regs(tsk)->regs[3])
#define KSTK_EUEN(tsk) (task_pt_regs(tsk)->csr_euen)
#define KSTK_ECFG(tsk) (task_pt_regs(tsk)->csr_ecfg)

#define return_address() ({__asm__ __volatile__("":::"$1"); __builtin_return_address(0);})

#ifdef CONFIG_CPU_HAS_PREFETCH

#define ARCH_HAS_PREFETCH
#define prefetch(x) __builtin_prefetch((x), 0, 1)

#define ARCH_HAS_PREFETCHW
#define prefetchw(x) __builtin_prefetch((x), 1, 1)

#endif

#endif /* _ASM_PROCESSOR_H */
