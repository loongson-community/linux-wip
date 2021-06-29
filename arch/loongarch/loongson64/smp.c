// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author: Huacai Chen <chenhuacai@loongson.cn>
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */

#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/sched/hotplug.h>
#include <linux/sched/task_stack.h>
#include <linux/seq_file.h>
#include <linux/smp.h>
#include <linux/syscore_ops.h>
#include <linux/tracepoint.h>
#include <asm/processor.h>
#include <asm/time.h>
#include <asm/tlbflush.h>
#include <asm/cacheflush.h>
#include <loongson.h>

DEFINE_PER_CPU(int, cpu_state);
DEFINE_PER_CPU_SHARED_ALIGNED(irq_cpustat_t, irq_stat);
EXPORT_PER_CPU_SYMBOL(irq_stat);

#define MAX_CPUS 64

#define STATUS  0x00
#define EN      0x04
#define SET     0x08
#define CLEAR   0x0c
#define MBUF    0x20

extern unsigned long long smp_group[MAX_PACKAGES];
static u32 core_offsets[4] = {0x000, 0x100, 0x200, 0x300};

static void *ipi_set_regs[MAX_CPUS];
static void *ipi_clear_regs[MAX_CPUS];
static void *ipi_status_regs[MAX_CPUS];
static void *ipi_en_regs[MAX_CPUS];
static void *ipi_mailbox_buf[MAX_CPUS];

u32 (*ipi_read_clear)(int cpu);
void (*ipi_write_action)(int cpu, u32 action);

enum ipi_msg_type {
	IPI_RESCHEDULE,
	IPI_CALL_FUNCTION,
};

static const char *ipi_types[NR_IPI] __tracepoint_string = {
	[IPI_RESCHEDULE] = "Rescheduling interrupts",
	[IPI_CALL_FUNCTION] = "Call Function interrupts",
};

void show_ipi_list(struct seq_file *p, int prec)
{
	unsigned int cpu, i;

	for (i = 0; i < NR_IPI; i++) {
		seq_printf(p, "%*s%u:%s", prec - 1, "IPI", i, prec >= 4 ? " " : "");
		for_each_online_cpu(cpu)
			seq_printf(p, "%10u ", per_cpu(irq_stat, cpu).ipi_irqs[i]);
		seq_printf(p, " LoongArch     %s\n", ipi_types[i]);
	}
}

/* Send mail buffer via Mail_Send */
static void csr_mail_send(uint64_t data, int cpu, int mailbox)
{
	uint64_t val;

	/* Send high 32 bits */
	val = IOCSR_MBUF_SEND_BLOCKING;
	val |= (IOCSR_MBUF_SEND_BOX_HI(mailbox) << IOCSR_MBUF_SEND_BOX_SHIFT);
	val |= (cpu << IOCSR_MBUF_SEND_CPU_SHIFT);
	val |= (data & IOCSR_MBUF_SEND_H32_MASK);
	iocsr_writeq(val, LOONGARCH_IOCSR_MBUF_SEND);

	/* Send low 32 bits */
	val = IOCSR_MBUF_SEND_BLOCKING;
	val |= (IOCSR_MBUF_SEND_BOX_LO(mailbox) << IOCSR_MBUF_SEND_BOX_SHIFT);
	val |= (cpu << IOCSR_MBUF_SEND_CPU_SHIFT);
	val |= (data << IOCSR_MBUF_SEND_BUF_SHIFT);
	iocsr_writeq(val, LOONGARCH_IOCSR_MBUF_SEND);
};

static u32 csr_ipi_read_clear(int cpu)
{
	u32 action;

	/* Load the ipi register to figure out what we're supposed to do */
	action = iocsr_readl(LOONGARCH_IOCSR_IPI_STATUS);
	/* Clear the ipi register to clear the interrupt */
	iocsr_writel(action, LOONGARCH_IOCSR_IPI_CLEAR);

	return action;
}

static void csr_ipi_write_action(int cpu, u32 action)
{
	unsigned int irq = 0;

	while ((irq = ffs(action))) {
		uint32_t val = IOCSR_IPI_SEND_BLOCKING;

		val |= (irq - 1);
		val |= (cpu << IOCSR_IPI_SEND_CPU_SHIFT);
		iocsr_writel(val, LOONGARCH_IOCSR_IPI_SEND);
		action &= ~BIT(irq - 1);
	}
}

static u32 legacy_ipi_read_clear(int cpu)
{
	u32 action;

	/* Load the ipi register to figure out what we're supposed to do */
	action = xconf_readl(ipi_status_regs[cpu]);
	/* Clear the ipi register to clear the interrupt */
	xconf_writel(action, ipi_clear_regs[cpu]);

	return action;
}

static void legacy_ipi_write_action(int cpu, u32 action)
{
	xconf_writel((u32)action, ipi_set_regs[cpu]);
}

static void ipi_method_init(void)
{
	if (cpu_has_csripi) {
		ipi_read_clear = csr_ipi_read_clear;
		ipi_write_action = csr_ipi_write_action;
	} else {
		ipi_read_clear = legacy_ipi_read_clear;
		ipi_write_action = legacy_ipi_write_action;
	}
}

static void ipi_regaddrs_init(void)
{
	int i, node, core;

	for (i = 0; i < MAX_CPUS; i++) {
		node = i / 4;
		core = i % 4;
		ipi_set_regs[i] = (void *)
			(smp_group[node] + core_offsets[core] + SET);
		ipi_clear_regs[i] = (void *)
			(smp_group[node] + core_offsets[core] + CLEAR);
		ipi_status_regs[i] = (void *)
			(smp_group[node] + core_offsets[core] + STATUS);
		ipi_en_regs[i] = (void *)
			(smp_group[node] + core_offsets[core] + EN);
		ipi_mailbox_buf[i] = (void *)
			(smp_group[node] + core_offsets[core] + MBUF);
	}
}

/*
 * Simple enough, just poke the appropriate ipi register
 */
static void loongson3_send_ipi_single(int cpu, unsigned int action)
{
	ipi_write_action(cpu_logical_map(cpu), (u32)action);
}

static void
loongson3_send_ipi_mask(const struct cpumask *mask, unsigned int action)
{
	unsigned int i;

	for_each_cpu(i, mask)
		ipi_write_action(cpu_logical_map(i), (u32)action);
}

void loongson3_ipi_interrupt(int irq)
{
	unsigned int action;
	unsigned int cpu = smp_processor_id();

	action = ipi_read_clear(cpu_logical_map(cpu));

	smp_mb();

	if (action & SMP_RESCHEDULE) {
		scheduler_ipi();
		per_cpu(irq_stat, cpu).ipi_irqs[IPI_RESCHEDULE]++;
	}

	if (action & SMP_CALL_FUNCTION) {
		irq_enter();
		generic_smp_call_function_interrupt();
		per_cpu(irq_stat, cpu).ipi_irqs[IPI_CALL_FUNCTION]++;
		irq_exit();
	}
}

/*
 * SMP init and finish on secondary CPUs
 */
static void loongson3_init_secondary(void)
{
	unsigned int cpu = smp_processor_id();
	unsigned int imask = ECFGF_PC | ECFGF_TIMER | ECFGF_IPI | ECFGF_IP1 | ECFGF_IP0;

	/* Set interrupt mask, but don't enable */
	change_csr_ecfg(ECFG0_IM, imask);

	if (cpu_has_csripi)
		iocsr_writel(0xffffffff, LOONGARCH_IOCSR_IPI_EN);
	else
		xconf_writel(0xffffffff, ipi_en_regs[cpu_logical_map(cpu)]);

	per_cpu(cpu_state, cpu) = CPU_ONLINE;
	cpu_set_core(&cpu_data[cpu],
		     cpu_logical_map(cpu) % loongson_sysconf.cores_per_package);
	cpu_set_cluster(&cpu_data[cpu],
		     cpu_logical_map(cpu) / loongson_sysconf.cores_per_package);
	cpu_data[cpu].package =
		     cpu_logical_map(cpu) / loongson_sysconf.cores_per_package;
}

static void loongson3_smp_finish(void)
{
	int cpu = smp_processor_id();

	local_irq_enable();

	if (cpu_has_csripi)
		iocsr_writeq(0, LOONGARCH_IOCSR_MBUF0);
	else
		xconf_writeq(0, (void *)(ipi_mailbox_buf[cpu_logical_map(cpu)]+0x0));

	pr_info("CPU#%d finished\n", smp_processor_id());
}

static void __init loongson3_smp_setup(void)
{
	ipi_method_init();
	ipi_regaddrs_init();

	if (cpu_has_csripi)
		iocsr_writel(0xffffffff, LOONGARCH_IOCSR_IPI_EN);
	else
		xconf_writel(0xffffffff, ipi_en_regs[cpu_logical_map(0)]);

	pr_info("Detected %i available CPU(s)\n", loongson_sysconf.nr_cpus);

	cpu_set_core(&cpu_data[0],
		     cpu_logical_map(0) % loongson_sysconf.cores_per_package);
	cpu_set_cluster(&cpu_data[0],
		     cpu_logical_map(0) / loongson_sysconf.cores_per_package);
	cpu_data[0].package = cpu_logical_map(0) / loongson_sysconf.cores_per_package;
}

static void __init loongson3_prepare_cpus(unsigned int max_cpus)
{
	int i = 0;

	for (i = 0; i < loongson_sysconf.nr_cpus; i++) {
		set_cpu_present(i, true);

		if (cpu_has_csripi)
			csr_mail_send(0, __cpu_logical_map[i], 0);
		else
			xconf_writeq(0, (void *)(ipi_mailbox_buf[__cpu_logical_map[i]]+0x0));
	}

	per_cpu(cpu_state, smp_processor_id()) = CPU_ONLINE;
}

/*
 * Setup the PC, SP, and TP of a secondary processor and start it running!
 */
static int loongson3_boot_secondary(int cpu, struct task_struct *idle)
{
	unsigned long startargs[4];

	pr_info("Booting CPU#%d...\n", cpu);

	/* startargs[] are initial PC, SP and TP for secondary CPU */
	startargs[0] = (unsigned long)&smpboot_entry;
	startargs[1] = (unsigned long)__KSTK_TOS(idle);
	startargs[2] = (unsigned long)task_thread_info(idle);
	startargs[3] = 0;

	pr_debug("CPU#%d, func_pc=%lx, sp=%lx, tp=%lx\n",
			cpu, startargs[0], startargs[1], startargs[2]);

	if (cpu_has_csripi) {
		csr_mail_send(startargs[3], cpu_logical_map(cpu), 3);
		csr_mail_send(startargs[2], cpu_logical_map(cpu), 2);
		csr_mail_send(startargs[1], cpu_logical_map(cpu), 1);
		csr_mail_send(startargs[0], cpu_logical_map(cpu), 0);
	} else {
		xconf_writeq(startargs[3], (void *)(ipi_mailbox_buf[cpu_logical_map(cpu)]+0x18));
		xconf_writeq(startargs[2], (void *)(ipi_mailbox_buf[cpu_logical_map(cpu)]+0x10));
		xconf_writeq(startargs[1], (void *)(ipi_mailbox_buf[cpu_logical_map(cpu)]+0x8));
		xconf_writeq(startargs[0], (void *)(ipi_mailbox_buf[cpu_logical_map(cpu)]+0x0));
	}
	return 0;
}

#ifdef CONFIG_HOTPLUG_CPU

static int loongson3_cpu_disable(void)
{
	unsigned long flags;
	unsigned int cpu = smp_processor_id();

	if (cpu == 0)
		return -EBUSY;

	set_cpu_online(cpu, false);
	calculate_cpu_foreign_map();
	local_irq_save(flags);
	fixup_irqs();
	local_irq_restore(flags);
	local_flush_tlb_all();

	return 0;
}


static void loongson3_cpu_die(unsigned int cpu)
{
	while (per_cpu(cpu_state, cpu) != CPU_DEAD)
		cpu_relax();

	mb();
}

/* To shutdown a core in Loongson 3, the target core should go to XKPRANGE
 * and flush all L1 entries at first. Then, another core (usually Core 0)
 * can safely disable the clock of the target core. loongson3_play_dead()
 * is called via XKPRANGE (uncached and unmmaped) */
static void loongson3_play_dead(int *state_addr)
{
	register int val;
	register long cpuid, core, node, count;
	register void *addr, *base, *initfunc;

	__asm__ __volatile__(
		"   li.d %[addr], 0x8000000000000000\n"
		"1: cacop 0x8, %[addr], 0           \n" /* flush L1 ICache */
		"   cacop 0x8, %[addr], 1           \n"
		"   cacop 0x8, %[addr], 2           \n"
		"   cacop 0x8, %[addr], 3           \n"
		"   cacop 0x9, %[addr], 0           \n" /* flush L1 DCache */
		"   cacop 0x9, %[addr], 1           \n"
		"   cacop 0x9, %[addr], 2           \n"
		"   cacop 0x9, %[addr], 3           \n"
		"   addi.w %[sets], %[sets], -1     \n"
		"   addi.d %[addr], %[addr], 0x40   \n"
		"   bnez  %[sets], 1b               \n"
		"   li.d %[addr], 0x8000000000000000\n"
		"2: cacop 0xa, %[addr], 0           \n" /* flush L1 VCache */
		"   cacop 0xa, %[addr], 1           \n"
		"   cacop 0xa, %[addr], 2           \n"
		"   cacop 0xa, %[addr], 3           \n"
		"   cacop 0xa, %[addr], 4           \n"
		"   cacop 0xa, %[addr], 5           \n"
		"   cacop 0xa, %[addr], 6           \n"
		"   cacop 0xa, %[addr], 7           \n"
		"   cacop 0xa, %[addr], 8           \n"
		"   cacop 0xa, %[addr], 9           \n"
		"   cacop 0xa, %[addr], 10          \n"
		"   cacop 0xa, %[addr], 11          \n"
		"   cacop 0xa, %[addr], 12          \n"
		"   cacop 0xa, %[addr], 13          \n"
		"   cacop 0xa, %[addr], 14          \n"
		"   cacop 0xa, %[addr], 15          \n"
		"   addi.w %[vsets], %[vsets], -1   \n"
		"   addi.d %[addr], %[addr], 0x40   \n"
		"   bnez  %[vsets], 2b              \n"
		"   li.w    %[val], 0x7             \n" /* *state_addr = CPU_DEAD; */
		"   st.w  %[val], %[state_addr], 0  \n"
		"   dbar 0                          \n"
		"   cacop 0x11, %[state_addr], 0    \n" /* flush entry of *state_addr */
		: [addr] "=&r" (addr), [val] "=&r" (val)
		: [state_addr] "r" (state_addr),
		  [sets] "r" (cpu_data[smp_processor_id()].dcache.sets),
		  [vsets] "r" (cpu_data[smp_processor_id()].vcache.sets));

	__asm__ __volatile__(
		"   csrrd  %[cpuid], 0x20              \n"
		"   andi   %[cpuid], %[cpuid], 0x1ff   \n"
		"   li.d    %[base], 0x800000001fe01000\n"
		"   andi   %[core], %[cpuid], 0x3      \n"
		"   slli.w %[core], %[core], 8         \n" /* get core id */
		"   or     %[base], %[base], %[core]   \n"
		"   andi   %[node], %[cpuid], 0xc      \n"
		"   slli.d %[node], %[node], 42        \n" /* get node id */
		"   or     %[base], %[base], %[node]   \n"
		"1: li.w     %[count], 0x100           \n" /* wait for init loop */
		"2: addi.w %[count], %[count], -1      \n"
		"   bnez   %[count], 2b                \n" /* limit mailbox access */
		"   ld.w   %[initfunc], %[base], 0x20  \n" /* check PC */
		"   beqz   %[initfunc], 1b             \n"
		"   ld.d   $a1, %[base], 0x38          \n"
		"   ld.d   $tp, %[base], 0x30          \n" /* get TP via mailbox */
		"   ld.d   $sp, %[base], 0x28          \n" /* get SP via mailbox */
		"   ld.d   %[initfunc], %[base], 0x20  \n" /* get PC via mailbox */
		"   jirl   $zero, %[initfunc], 0       \n" /* jump to initial PC */
		"   nop                                \n"
		: [core] "=&r" (core), [node] "=&r" (node),
		  [base] "=&r" (base), [cpuid] "=&r" (cpuid),
		  [count] "=&r" (count), [initfunc] "=&r" (initfunc)
		: /* No Input */
		: "a1");

	unreachable();
}

void play_dead(void)
{
	int *state_addr;
	unsigned int cpu = smp_processor_id();
	void (*play_dead_uncached)(int *s);

	idle_task_exit();
	play_dead_uncached = (void *)TO_UNCAC(__pa((unsigned long)loongson3_play_dead));
	state_addr = &per_cpu(cpu_state, cpu);
	mb();
	play_dead_uncached(state_addr);
}

static int loongson3_disable_clock(unsigned int cpu)
{
	uint64_t core_id = cpu_core(&cpu_data[cpu]);
	uint64_t package_id = cpu_data[cpu].package;

	LOONGSON_FREQCTRL(package_id) &= ~(1 << (core_id * 4 + 3));

	return 0;
}

static int loongson3_enable_clock(unsigned int cpu)
{
	uint64_t core_id = cpu_core(&cpu_data[cpu]);
	uint64_t package_id = cpu_data[cpu].package;

	LOONGSON_FREQCTRL(package_id) |= 1 << (core_id * 4 + 3);

	return 0;
}

static int register_loongson3_notifier(void)
{
	return cpuhp_setup_state_nocalls(CPUHP_LOONGARCH_SOC_PREPARE,
					 "loongarch/loongson:prepare",
					 loongson3_enable_clock,
					 loongson3_disable_clock);
}
early_initcall(register_loongson3_notifier);

#endif

const struct plat_smp_ops loongson3_smp_ops = {
	.send_ipi_single = loongson3_send_ipi_single,
	.send_ipi_mask = loongson3_send_ipi_mask,
	.smp_setup = loongson3_smp_setup,
	.prepare_cpus = loongson3_prepare_cpus,
	.boot_secondary = loongson3_boot_secondary,
	.init_secondary = loongson3_init_secondary,
	.smp_finish = loongson3_smp_finish,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_disable = loongson3_cpu_disable,
	.cpu_die = loongson3_cpu_die,
#endif
};

/*
 * Power management
 */
#ifdef CONFIG_PM

static int loongson3_ipi_suspend(void)
{
	return 0;
}

static void loongson3_ipi_resume(void)
{
	if (cpu_has_csripi)
		iocsr_writel(0xffffffff, LOONGARCH_IOCSR_IPI_EN);
	else
		xconf_writel(0xffffffff, ipi_en_regs[cpu_logical_map(0)]);
}

static struct syscore_ops loongson3_ipi_syscore_ops = {
	.resume         = loongson3_ipi_resume,
	.suspend        = loongson3_ipi_suspend,
};

/*
 * Enable boot cpu ipi before enabling nonboot cpus
 * during syscore_resume.
 */
static int __init ipi_pm_init(void)
{
	register_syscore_ops(&loongson3_ipi_syscore_ops);
	return 0;
}

core_initcall(ipi_pm_init);
#endif
