// SPDX-License-Identifier: GPL-2.0
/*
 * Processor capabilities determination functions.
 *
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ptrace.h>
#include <linux/smp.h>
#include <linux/stddef.h>
#include <linux/export.h>
#include <linux/printk.h>
#include <linux/uaccess.h>

#include <asm/cpu.h>
#include <asm/cpu-features.h>
#include <asm/fpu.h>
#include <asm/loongarchregs.h>
#include <asm/elf.h>
#include <asm/pgtable-bits.h>

/* Hardware capabilities */
unsigned int elf_hwcap __read_mostly;
EXPORT_SYMBOL_GPL(elf_hwcap);

/*
 * Determine the FCSR mask for FPU hardware.
 */
static inline void cpu_set_fpu_fcsr_mask(struct cpuinfo_loongarch *c)
{
	unsigned long sr, mask, fcsr, fcsr0, fcsr1;

	fcsr = c->fpu_csr0;
	mask = FPU_CSR_ALL_X | FPU_CSR_ALL_E | FPU_CSR_ALL_S | FPU_CSR_RM;

	sr = read_csr_euen();
	enable_fpu();

	fcsr0 = fcsr & mask;
	write_fcsr(LOONGARCH_FCSR0, fcsr0);
	fcsr0 = read_fcsr(LOONGARCH_FCSR0);

	fcsr1 = fcsr | ~mask;
	write_fcsr(LOONGARCH_FCSR0, fcsr1);
	fcsr1 = read_fcsr(LOONGARCH_FCSR0);

	write_fcsr(LOONGARCH_FCSR0, fcsr);

	write_csr_euen(sr);

	c->fpu_mask = ~(fcsr0 ^ fcsr1) & ~mask;
}

static inline void set_elf_platform(int cpu, const char *plat)
{
	if (cpu == 0)
		__elf_platform = plat;
}

/* MAP BASE */
unsigned long vm_map_base;
EXPORT_SYMBOL_GPL(vm_map_base);

void cpu_probe_addrbits(struct cpuinfo_loongarch *c)
{
#ifdef __NEED_ADDRBITS_PROBE
	c->pabits = (read_cpucfg(LOONGARCH_CPUCFG1) & CPUCFG1_PABITS) >> 4;
	c->vabits = (read_cpucfg(LOONGARCH_CPUCFG1) & CPUCFG1_VABITS) >> 12;
	vm_map_base = 0UL - (1UL << c->vabits);
#endif
}

static void set_isa(struct cpuinfo_loongarch *c, unsigned int isa)
{
	switch (isa) {
	case LOONGARCH_CPU_ISA_LA64:
		c->isa_level |= LOONGARCH_CPU_ISA_LA64;
		fallthrough;
	case LOONGARCH_CPU_ISA_LA32S:
		c->isa_level |= LOONGARCH_CPU_ISA_LA32S;
		fallthrough;
	case LOONGARCH_CPU_ISA_LA32R:
		c->isa_level |= LOONGARCH_CPU_ISA_LA32R;
		break;
	}
}

static void decode_configs(struct cpuinfo_loongarch *c)
{
	unsigned int config;
	unsigned long asid_mask;

	config = read_cpucfg(LOONGARCH_CPUCFG1);
	if (config & CPUCFG1_UAL) {
		c->options |= LOONGARCH_CPU_UAL;
		elf_hwcap |= HWCAP_LOONGARCH_UAL;
	}

	config = read_cpucfg(LOONGARCH_CPUCFG2);
	if (config & CPUCFG2_LAM) {
		c->options |= LOONGARCH_CPU_LAM;
		elf_hwcap |= HWCAP_LOONGARCH_LAM;
	}
	if (config & CPUCFG2_FP) {
		c->options |= LOONGARCH_CPU_FPU;
		elf_hwcap |= HWCAP_LOONGARCH_FPU;
	}
	if (config & CPUCFG2_COMPLEX) {
		c->options |= LOONGARCH_CPU_COMPLEX;
		elf_hwcap |= HWCAP_LOONGARCH_COMPLEX;
	}
	if (config & CPUCFG2_CRYPTO) {
		c->options |= LOONGARCH_CPU_CRYPTO;
		elf_hwcap |= HWCAP_LOONGARCH_CRYPTO;
	}
	if (config & CPUCFG2_LVZP) {
		c->options |= LOONGARCH_CPU_LVZ;
		elf_hwcap |= HWCAP_LOONGARCH_LVZ;
	}

	config = read_cpucfg(LOONGARCH_CPUCFG6);
	if (config & CPUCFG6_PMP)
		c->options |= LOONGARCH_CPU_PMP;

	config = iocsr_readl(LOONGARCH_IOCSR_FEATURES);
	if (config & IOCSRF_CSRIPI)
		c->options |= LOONGARCH_CPU_CSRIPI;
	if (config & IOCSRF_EXTIOI)
		c->options |= LOONGARCH_CPU_EXTIOI;
	if (config & IOCSRF_FREQSCALE)
		c->options |= LOONGARCH_CPU_SCALEFREQ;
	if (config & IOCSRF_VM)
		c->options |= LOONGARCH_CPU_HYPERVISOR;

	config = csr_readl(LOONGARCH_CSR_ASID);
	config = (config & CSR_ASID_BIT) >> CSR_ASID_BIT_SHIFT;
	asid_mask = GENMASK(config - 1, 0);
	set_cpu_asid_mask(c, asid_mask);

	config = read_csr_prcfg1();
	c->kscratch_mask = GENMASK((config & CSR_CONF1_KSNUM) - 1, 0);
	c->kscratch_mask &= ~(EXC_KSCRATCH_MASK | PERCPU_KSCRATCH_MASK | KVM_KSCRATCH_MASK);

	config = read_csr_prcfg3();
	switch (config & CSR_CONF3_TLBTYPE) {
	case 0:
		c->tlbsizemtlb = 0;
		c->tlbsizestlbsets = 0;
		c->tlbsizestlbways = 0;
		c->tlbsize = 0;
		break;
	case 1:
		c->tlbsizemtlb = ((config & CSR_CONF3_MTLBSIZE) >> CSR_CONF3_MTLBSIZE_SHIFT) + 1;
		c->tlbsizestlbsets = 0;
		c->tlbsizestlbways = 0;
		c->tlbsize = c->tlbsizemtlb + c->tlbsizestlbsets * c->tlbsizestlbways;
		break;
	case 2:
		c->tlbsizemtlb = ((config & CSR_CONF3_MTLBSIZE) >> CSR_CONF3_MTLBSIZE_SHIFT) + 1;
		c->tlbsizestlbsets = 1 << ((config & CSR_CONF3_STLBIDX) >> CSR_CONF3_STLBIDX_SHIFT);
		c->tlbsizestlbways = ((config & CSR_CONF3_STLBWAYS) >> CSR_CONF3_STLBWAYS_SHIFT) + 1;
		c->tlbsize = c->tlbsizemtlb + c->tlbsizestlbsets * c->tlbsizestlbways;
		break;
	default:
		pr_warn("Warning: unimplemented tlb type\n");
	}
}

#define MAX_NAME_LEN	32
#define VENDOR_OFFSET	0
#define CPUNAME_OFFSET	9

static char cpu_full_name[MAX_NAME_LEN] = "        -        ";

static inline void cpu_probe_loongson(struct cpuinfo_loongarch *c, unsigned int cpu)
{
	uint64_t *vendor = (void *)(&cpu_full_name[VENDOR_OFFSET]);
	uint64_t *cpuname = (void *)(&cpu_full_name[CPUNAME_OFFSET]);

	c->options = LOONGARCH_CPU_CPUCFG | LOONGARCH_CPU_CSR |
		     LOONGARCH_CPU_TLB | LOONGARCH_CPU_VINT | LOONGARCH_CPU_WATCH;

	decode_configs(c);
	elf_hwcap |= HWCAP_LOONGARCH_CRC32;

	__cpu_full_name[cpu] = cpu_full_name;
	*vendor = iocsr_readq(LOONGARCH_IOCSR_VENDOR);
	*cpuname = iocsr_readq(LOONGARCH_IOCSR_CPUNAME);

	switch (c->processor_id & PRID_IMP_MASK) {
	case PRID_IMP_LOONGSON_32:
		c->cputype = CPU_LOONGSON32;
		set_isa(c, LOONGARCH_CPU_ISA_LA32S);
		c->writecombine = _CACHE_WUC;
		__cpu_family[cpu] = "Loongson-32bit";
		pr_info("Standard 32-bit Loongson Processor probed\n");
		break;
	case PRID_IMP_LOONGSON_64R:
		c->cputype = CPU_LOONGSON64;
		set_isa(c, LOONGARCH_CPU_ISA_LA64);
		c->writecombine = _CACHE_WUC;
		__cpu_family[cpu] = "Loongson-64bit";
		pr_info("Reduced 64-bit Loongson Processor probed\n");
		break;
	case PRID_IMP_LOONGSON_64C:
		c->cputype = CPU_LOONGSON64;
		set_isa(c, LOONGARCH_CPU_ISA_LA64);
		c->writecombine = _CACHE_WUC;
		__cpu_family[cpu] = "Loongson-64bit";
		pr_info("Classic 64-bit Loongson Processor probed\n");
		break;
	case PRID_IMP_LOONGSON_64G:
		c->cputype = CPU_LOONGSON64;
		set_isa(c, LOONGARCH_CPU_ISA_LA64);
		c->writecombine = _CACHE_WUC;
		__cpu_family[cpu] = "Loongson-64bit";
		pr_info("Generic 64-bit Loongson Processor probed\n");
		break;
	default: /* Default to 64 bit */
		c->cputype = CPU_LOONGSON64;
		set_isa(c, LOONGARCH_CPU_ISA_LA64);
		c->writecombine = _CACHE_SUC;
		__cpu_family[cpu] = "Loongson-64bit";
		pr_info("Unknown 64-bit Loongson Processor probed\n");
	}
}

#ifdef CONFIG_64BIT
/* For use by uaccess.h */
u64 __ua_limit;
EXPORT_SYMBOL(__ua_limit);
#endif

const char *__cpu_family[NR_CPUS];
const char *__cpu_full_name[NR_CPUS];
const char *__elf_platform;

void cpu_probe(void)
{
	struct cpuinfo_loongarch *c = &current_cpu_data;
	unsigned int cpu = smp_processor_id();

	/*
	 * Set a default elf platform, cpu probe may later
	 * overwrite it with a more precise value
	 */
	set_elf_platform(cpu, "loongarch");

	c->cputype	= CPU_UNKNOWN;
	c->processor_id = read_cpucfg(LOONGARCH_CPUCFG0);
	c->fpu_vers	= (read_cpucfg(LOONGARCH_CPUCFG2) >> 3) & 0x3;
	c->writecombine = _CACHE_SUC;

	c->fpu_csr0	= FPU_CSR_RN;
	c->fpu_mask	= FPU_CSR_RSVD;

	switch (c->processor_id & PRID_COMP_MASK) {
	case PRID_COMP_LOONGSON:
		cpu_probe_loongson(c, cpu);
		break;
	}

	BUG_ON(!__cpu_family[cpu]);
	BUG_ON(c->cputype == CPU_UNKNOWN);

	cpu_probe_addrbits(c);

#ifdef CONFIG_64BIT
	if (cpu == 0)
		__ua_limit = ~((1ull << cpu_vabits) - 1);
#endif
}

void cpu_report(void)
{
	struct cpuinfo_loongarch *c = &current_cpu_data;

	pr_info("CPU%d revision is: %08x (%s)\n",
		smp_processor_id(), c->processor_id, cpu_family_string());
	if (c->options & LOONGARCH_CPU_FPU)
		pr_info("FPU%d revision is: %08x\n", smp_processor_id(), c->fpu_vers);
}

void cpu_set_cluster(struct cpuinfo_loongarch *cpuinfo, unsigned int cluster)
{
	/* Ensure the core number fits in the field */
	WARN_ON(cluster > (LOONGARCH_GLOBALNUMBER_CLUSTER >>
			   LOONGARCH_GLOBALNUMBER_CLUSTER_SHF));

	cpuinfo->globalnumber &= ~LOONGARCH_GLOBALNUMBER_CLUSTER;
	cpuinfo->globalnumber |= cluster << LOONGARCH_GLOBALNUMBER_CLUSTER_SHF;
}

void cpu_set_core(struct cpuinfo_loongarch *cpuinfo, unsigned int core)
{
	/* Ensure the core number fits in the field */
	WARN_ON(core > (LOONGARCH_GLOBALNUMBER_CORE >> LOONGARCH_GLOBALNUMBER_CORE_SHF));

	cpuinfo->globalnumber &= ~LOONGARCH_GLOBALNUMBER_CORE;
	cpuinfo->globalnumber |= core << LOONGARCH_GLOBALNUMBER_CORE_SHF;
}
