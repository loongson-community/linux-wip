// SPDX-License-Identifier: GPL-2.0
/*
 * Count register synchronisation.
 */

#include <linux/kernel.h>
#include <linux/irqflags.h>
#include <linux/cpumask.h>
#include <linux/spinlock.h>
#include <linux/smp.h>

#include <asm/r4k-timer.h>
#include <linux/atomic.h>
#include <asm/barrier.h>
#include <asm/mipsregs.h>

#define STAGE_START		0
#define STAGE_MASTER_READY	1
#define STAGE_SLAVE_SYNCED	2

static unsigned int cur_count = 0;
static unsigned int fini_count = 0;
static atomic_t sync_stage = ATOMIC_INIT(0);
static DEFINE_RAW_SPINLOCK(sync_r4k_lock);

#define MAX_LOOPS	1000

void synchronise_count_master(void *unused)
{
	unsigned long flags;
	long delta;
	int i;

	if (atomic_read(&sync_stage) != STAGE_START)
		BUG();

	local_irq_save(flags);

	cur_count = read_c0_count();
	smp_wmb();
	atomic_inc(&sync_stage); /* inc to STAGE_MASTER_READY */

	for (i = 0; i < MAX_LOOPS; i++) {
		cur_count = read_c0_count();
		smp_wmb();
		if (atomic_read(&sync_stage) == STAGE_SLAVE_SYNCED)
			break;
	}

	delta = read_c0_count() - fini_count;

	local_irq_restore(flags);

	if (i == MAX_LOOPS)
		pr_err("sync-r4k: Master: synchronise timeout\n");
	else
		pr_info("sync-r4k: Master: synchronise succeed, maxium delta: %ld\n", delta);

	return;
}

void synchronise_count_slave(int cpu)
{
	int i;
	unsigned long flags;
	call_single_data_t csd;

	raw_spin_lock(&sync_r4k_lock);

	/* Let variables get attention from cache */
	for (i = 0; i < MAX_LOOPS; i++) {
		cur_count++;
		fini_count += cur_count;
		cur_count += fini_count;
	}

	atomic_set(&sync_stage, STAGE_START);
	csd.func = synchronise_count_master;

	/* Master count is always CPU0 */
	if (smp_call_function_single_async(0, &csd)) {
		pr_err("sync-r4k: Salve: Failed to call master\n");
		raw_spin_unlock(&sync_r4k_lock);
		return;
	}

	local_irq_save(flags);

	/* Wait until master ready */
	while (atomic_read(&sync_stage) != STAGE_MASTER_READY)
		cpu_relax();

	write_c0_count(cur_count);
	fini_count = read_c0_count();
	smp_wmb();
	atomic_inc(&sync_stage); /* inc to STAGE_SLAVE_SYNCED */

	local_irq_restore(flags);

	raw_spin_unlock(&sync_r4k_lock);
}
