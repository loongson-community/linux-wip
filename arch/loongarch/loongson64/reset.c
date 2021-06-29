// SPDX-License-Identifier: GPL-2.0
/*
 * Author: Huacai Chen, chenhuacai@loongson.cn
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#include <linux/acpi.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/efi.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <acpi/reboot.h>
#include <asm/bootinfo.h>
#include <asm/delay.h>
#include <asm/idle.h>
#include <asm/reboot.h>
#include <boot_param.h>

static void loongson_restart(void)
{
#ifdef CONFIG_EFI
	if (efi_capsule_pending(NULL))
		efi_reboot(REBOOT_WARM, NULL);
	else
		efi_reboot(REBOOT_COLD, NULL);
#endif
	if (!acpi_disabled)
		acpi_reboot();

	while (1) {
		__arch_cpu_idle();
	}
}

static void loongson_poweroff(void)
{
#ifdef CONFIG_EFI
	efi.reset_system(EFI_RESET_SHUTDOWN, EFI_SUCCESS, 0, NULL);
#endif
	while (1) {
		__arch_cpu_idle();
	}
}

static int __init loongarch_reboot_setup(void)
{
	pm_restart = loongson_restart;
	pm_power_off = loongson_poweroff;

	return 0;
}

arch_initcall(loongarch_reboot_setup);
