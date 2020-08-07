// SPDX-License-Identifier: GPL-2.0
/*
 * Loongson64 constant timer driver
 *
 * Copyright (C) 2020 Huacai Chen <chenhc@lemote.com>
 * Copyright (C) 2020 Jiaxun Yang <jiaxun.yang@fluygoat.com>
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/clockchips.h>
#include <linux/cpuhotplug.h>
#include <linux/sched_clock.h>
#include <asm/time.h>
#include <asm/mipsregs.h>
#include <asm/cevt-r4k.h>
#include <loongson.h>
#include <loongson_regs.h>

static unsigned int constant_timer_irq = -1;

static DEFINE_PER_CPU(struct clock_event_device, constant_clockevent_device);

static inline unsigned int calc_const_freq(void)
{
	unsigned int res;
	unsigned int base_freq;
	unsigned int cfm, cfd;

	res = read_cpucfg(LOONGSON_CFG2);
	if (!(res & LOONGSON_CFG2_LLFTP))
		return 0;

	res = read_cpucfg(LOONGSON_CFG5);
	cfm = res & 0xffff;
	cfd = (res >> 16) & 0xffff;
	base_freq = read_cpucfg(LOONGSON_CFG4);

	if (!base_freq || !cfm || !cfd)
		return 0;
	else
		return (base_freq * cfm / cfd);
}

static irqreturn_t handle_constant_timer(int irq, void *data)
{
	int cpu = smp_processor_id();
	struct clock_event_device *cd;

	if ((cp0_perfcount_irq < 0) && perf_irq() == IRQ_HANDLED)
		return IRQ_HANDLED;

	cd = &per_cpu(mips_clockevent_device, cpu);


	if (clockevent_state_detached(cd) || clockevent_state_shutdown(cd))
		return IRQ_NONE;

	if (read_c0_cause() & CAUSEF_TI) {
		/* Clear Count/Compare Interrupt */
		write_c0_compare(read_c0_compare());
		cd = &per_cpu(constant_clockevent_device, cpu);
		cd->event_handler(cd);

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static int constant_set_state_oneshot(struct clock_event_device *evt)
{
	u64 cfg;

	cfg = csr_readq(LOONGSON_CSR_TIMER_CFG);

	cfg |= CONSTANT_TIMER_CFG_EN;
	cfg &= ~CONSTANT_TIMER_CFG_PERIODIC;

	csr_writeq(cfg, LOONGSON_CSR_TIMER_CFG);

	set_c0_config6(LOONGSON_CONF6_EXTIMER);

	return 0;
}

static int constant_set_state_periodic(struct clock_event_device *evt)
{
	unsigned int period;
	u64 cfg;

	cfg = csr_readq(LOONGSON_CSR_TIMER_CFG);

	cfg &= CONSTANT_TIMER_INITVAL_RESET;
	cfg |= (CONSTANT_TIMER_CFG_PERIODIC | CONSTANT_TIMER_CFG_EN);

	period = calc_const_freq() / HZ;

	csr_writeq(cfg | period, LOONGSON_CSR_TIMER_CFG);

	set_c0_config6(LOONGSON_CONF6_EXTIMER);

	return 0;
}

static int constant_set_state_shutdown(struct clock_event_device *evt)
{
	u64 cfg;

	clear_c0_config6(LOONGSON_CONF6_EXTIMER);
	cfg = csr_readq(LOONGSON_CSR_TIMER_CFG);
	cfg &= ~CONSTANT_TIMER_CFG_EN;
	csr_writeq(cfg, LOONGSON_CSR_TIMER_CFG);

	return 0;
}

static int constant_next_event(unsigned long delta,
				struct clock_event_device *evt)
{
	csr_writeq(delta | CONSTANT_TIMER_CFG_EN, LOONGSON_CSR_TIMER_CFG);
	return 0;
}

static int constant_init_percpu(unsigned int cpu)
{
	unsigned int const_freq;
	struct clock_event_device *cd;
	unsigned long min_delta = 0x600;
	unsigned long max_delta = (1UL << 48) - 1;

	cd = &per_cpu(constant_clockevent_device, cpu);

	const_freq = calc_const_freq();

	cd->name = "constant";
	cd->features = CLOCK_EVT_FEAT_ONESHOT | CLOCK_EVT_FEAT_PERIODIC |
		       CLOCK_EVT_FEAT_PERCPU;

	cd->rating = 500; /* Higher than cevt-r4k */
	cd->irq = constant_timer_irq;
	cd->cpumask = cpumask_of(cpu);
	cd->set_state_oneshot = constant_set_state_oneshot;
	cd->set_state_periodic = constant_set_state_periodic;
	cd->set_state_shutdown = constant_set_state_shutdown;
	cd->set_next_event = constant_next_event;

	clockevents_config_and_register(cd, const_freq, min_delta, max_delta);

	return 0;
}

static int __init constant_clockevent_init(void)
{
	unsigned long long flags = IRQF_PERCPU | IRQF_TIMER | IRQF_SHARED;

	constant_timer_irq = get_c0_compare_int();
	if (constant_timer_irq <= 0)
		return -ENXIO;

	/* Init on CPU0 firstly to keep away from IRQ storm */
	constant_init_percpu(0);

	if (request_irq(constant_timer_irq, handle_constant_timer, flags,
			"constant_timer", handle_constant_timer)) {
		pr_err("Failed to request irq %d (constant_timer)\n", constant_timer_irq);
		return -ENXIO;
	}

	cpuhp_setup_state_nocalls(CPUHP_AP_MIPS_CONST_TIMER_STARTING,
			  "clockevents/mips/constant/timer:starting",
			   constant_init_percpu, NULL);

	return 0;
}

static u64 read_const_counter(void)
{
	u64 count;

	__asm__ __volatile__(
		" .set push\n"
		" .set mips64r2\n"
		" rdhwr   %0, $30\n"
		" .set pop\n"
		: "=r" (count));

	return count;
}

static u64 csrc_read_const_counter(struct clocksource *clk)
{
	return read_const_counter();
}

static struct clocksource clocksource_const = {
	.name = "constant",
	.rating = 8000,
	.read = csrc_read_const_counter,
	.mask = CLOCKSOURCE_MASK(64),
	.flags = CLOCK_SOURCE_IS_CONTINUOUS,
	.vdso_clock_mode = VDSO_CLOCKMODE_CONST,
	.mult = 0,
	.shift = 10,
};

static int __init constant_clocksource_init(unsigned long freq)
{

	clocksource_const.mult =
		clocksource_hz2mult(freq, clocksource_const.shift);

	return clocksource_register_hz(&clocksource_const, freq);
}

static void __init constant_sched_clock_init(unsigned long freq)
{
	plat_have_sched_clock = true;
	sched_clock_register(read_const_counter, 64, freq);
}

int __init constant_timer_init(void)
{
	u32 ver;
	unsigned long freq;

	if (!cpu_has_extimer)
		return -ENODEV;

	if (!cpu_has_csr())
		return -ENODEV;

	ver = read_cpucfg(LOONGSON_CFG2);
	ver &= LOONGSON_CFG2_LLFTPREV;
	ver >>= LOONGSON_CFG2_LLFTPREVB;

	if (ver < 2)
		return -ENODEV;

	freq = calc_const_freq();
	if (!freq)
		return -ENODEV;

	constant_clockevent_init();
	constant_clocksource_init(freq);
	constant_sched_clock_init(freq);

	return 0;
}
