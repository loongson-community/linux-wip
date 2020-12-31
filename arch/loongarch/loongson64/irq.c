// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#include <linux/compiler.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irqchip.h>
#include <linux/irqdomain.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/stddef.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/loongarchregs.h>
#include <loongson.h>

struct acpi_madt_lio_pic *acpi_liointc;
struct acpi_madt_eio_pic *acpi_eiointc;
struct acpi_madt_ht_pic *acpi_htintc;
struct acpi_madt_lpc_pic *acpi_pchlpc;
struct acpi_madt_msi_pic *acpi_pchmsi;
struct acpi_madt_bio_pic *acpi_pchpic[MAX_PCH_PICS];

struct fwnode_handle *acpi_liointc_handle;
struct fwnode_handle *acpi_msidomain_handle;
struct fwnode_handle *acpi_picdomain_handle[MAX_PCH_PICS];

int find_pch_pic(u32 gsi)
{
	int i, start, end;

	/* Find the PCH_PIC that manages this GSI. */
	for (i = 0; i < loongson_sysconf.nr_pch_pics; i++) {
		struct acpi_madt_bio_pic *irq_cfg = acpi_pchpic[i];

		start = irq_cfg->gsi_base;
		end   = irq_cfg->gsi_base + irq_cfg->size;
		if (gsi >= start && gsi < end)
			return i;
	}

	pr_err("ERROR: Unable to locate PCH_PIC for GSI %d\n", gsi);
	return -1;
}

void __init setup_IRQ(void)
{
	int i;
	struct fwnode_handle *pch_parent_handle;

	if (!acpi_eiointc)
		cpu_data[0].options &= ~LOONGARCH_CPU_EXTIOI;

	loongarch_cpu_irq_init(NULL, NULL);
	acpi_liointc_handle = liointc_acpi_init(acpi_liointc);

	if (cpu_has_extioi) {
		pr_info("Using EIOINTC interrupt mode\n");
		pch_parent_handle = eiointc_acpi_init(acpi_eiointc);
	} else {
		pr_info("Using HTVECINTC interrupt mode\n");
		pch_parent_handle = htvec_acpi_init(acpi_liointc_handle, acpi_htintc);
	}

	for (i = 0; i < loongson_sysconf.nr_pch_pics; i++)
		acpi_picdomain_handle[i] = pch_pic_acpi_init(pch_parent_handle, acpi_pchpic[i]);

	acpi_msidomain_handle = pch_msi_acpi_init(pch_parent_handle, acpi_pchmsi);
	irq_set_default_host(irq_find_matching_fwnode(acpi_picdomain_handle[0], DOMAIN_BUS_ANY));

	pch_lpc_acpi_init(acpi_picdomain_handle[0], acpi_pchlpc);
}

void __init arch_init_irq(void)
{
	clear_csr_ecfg(ECFG0_IM);
	clear_csr_estat(ESTATF_IP);

	setup_IRQ();

	set_csr_ecfg(ECFGF_IP0 | ECFGF_IP1 | ECFGF_IPI | ECFGF_PC);
}
