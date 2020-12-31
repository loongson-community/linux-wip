/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#ifndef _ASM_IRQ_H
#define _ASM_IRQ_H

#include <linux/irqdomain.h>
#include <irq.h>
#include <asm-generic/irq.h>

#define IRQ_STACK_SIZE			THREAD_SIZE
#define IRQ_STACK_START			(IRQ_STACK_SIZE - 16)

DECLARE_PER_CPU(unsigned long, irq_stack);

/*
 * The highest address on the IRQ stack contains a dummy frame put down in
 * genex.S (except_vec_vi_handler) which is structured as follows:
 *
 *   top ------------
 *       | task sp  | <- irq_stack[cpu] + IRQ_STACK_START
 *       ------------
 *       |          | <- First frame of IRQ context
 *       ------------
 *
 * task sp holds a copy of the task stack pointer where the struct pt_regs
 * from exception entry can be found.
 */

static inline bool on_irq_stack(int cpu, unsigned long sp)
{
	unsigned long low = per_cpu(irq_stack, cpu);
	unsigned long high = low + IRQ_STACK_SIZE;

	return (low <= sp && sp <= high);
}

struct irq_data;
struct device_node;

void arch_init_irq(void);
void do_IRQ(unsigned int irq);
void spurious_interrupt(void);
int loongarch_cpu_irq_init(struct device_node *of_node, struct device_node *parent);

#define NR_IRQS_LEGACY 16

void arch_trigger_cpumask_backtrace(const struct cpumask *mask,
					bool exclude_self);
#define arch_trigger_cpumask_backtrace arch_trigger_cpumask_backtrace

#endif /* _ASM_IRQ_H */
