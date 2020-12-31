/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Author: Hanlu Li <lihanlu@loongson.cn>
 *         Huacai Chen <chenhuacai@loongson.cn>
 *
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#ifndef _UAPI_ASM_SIGINFO_H
#define _UAPI_ASM_SIGINFO_H

#if _LOONGARCH_SZLONG == 32
#define __ARCH_SI_PREAMBLE_SIZE (3 * sizeof(int))
#else
#define __ARCH_SI_PREAMBLE_SIZE (4 * sizeof(int))
#endif

#include <asm-generic/siginfo.h>

#endif /* _UAPI_ASM_SIGINFO_H */
