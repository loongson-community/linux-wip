// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/pm.h>
#include <linux/types.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/console.h>

#include <asm/compiler.h>
#include <asm/idle.h>
#include <asm/loongarchregs.h>
#include <asm/reboot.h>

static void machine_hang(void)
{
	local_irq_disable();
	clear_csr_ecfg(ECFG0_IM);

	pr_notice("\n\n** You can safely turn off the power now **\n\n");
	console_flush_on_panic(CONSOLE_FLUSH_PENDING);

	while (true) {
		__arch_cpu_idle();
		local_irq_disable();
	}
}

void (*pm_restart)(void) = machine_hang;
void (*pm_power_off)(void) = machine_hang;

EXPORT_SYMBOL(pm_power_off);

void machine_halt(void)
{
#ifdef CONFIG_SMP
	preempt_disable();
	smp_send_stop();
#endif
	machine_hang();
}

void machine_power_off(void)
{
#ifdef CONFIG_SMP
	preempt_disable();
	smp_send_stop();
#endif
	pm_power_off();
}

void machine_restart(char *command)
{
#ifdef CONFIG_SMP
	preempt_disable();
	smp_send_stop();
#endif
	do_kernel_restart(command);
	pm_restart();
}
