/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#ifndef __ASM_CMPXCHG_H
#define __ASM_CMPXCHG_H

#include <linux/bug.h>
#include <asm/compiler.h>

/*
 * These functions doesn't exist, so if they are called you'll either:
 *
 * - Get an error at compile-time due to __compiletime_error, if supported by
 *   your compiler.
 *
 * or:
 *
 * - Get an error at link-time due to the call to the missing function.
 */
extern unsigned long __cmpxchg_called_with_bad_pointer(void)
	__compiletime_error("Bad argument size for cmpxchg");
extern unsigned long __xchg_called_with_bad_pointer(void)
	__compiletime_error("Bad argument size for xchg");

#define __xchg_asm(amswap_db, m, val)		\
({						\
		__typeof(val) __ret;		\
						\
		__asm__ __volatile__ (		\
		" "amswap_db" %1, %z2, %0 \n"	\
		: "+ZB" (*m), "=&r" (__ret)	\
		: "Jr" (val)			\
		: "memory");			\
						\
		__ret;				\
})

extern unsigned long __xchg_small(volatile void *ptr, unsigned long val,
				  unsigned int size);

static inline unsigned long __xchg(volatile void *ptr, unsigned long x,
				   int size)
{
	switch (size) {
	case 1:
	case 2:
		return __xchg_small(ptr, x, size);

	case 4:
		return __xchg_asm("amswap_db.w", (volatile u32 *)ptr, (u32)x);

	case 8:
		if (!IS_ENABLED(CONFIG_64BIT))
			return __xchg_called_with_bad_pointer();

		return __xchg_asm("amswap_db.d", (volatile u64 *)ptr, (u64)x);

	default:
		return __xchg_called_with_bad_pointer();
	}
}

#define arch_xchg(ptr, x)							\
({									\
	__typeof__(*(ptr)) __res;					\
									\
	__res = (__typeof__(*(ptr)))					\
		__xchg((ptr), (unsigned long)(x), sizeof(*(ptr)));	\
									\
	__res;								\
})

#define __cmpxchg_asm(ld, st, m, old, new)				\
({									\
	__typeof(old) __ret;						\
									\
	__asm__ __volatile__(						\
	"1:	" ld "	%0, %2		# __cmpxchg_asm \n"		\
	"	bne	%0, %z3, 2f			\n"		\
	"	or	$t0, %z4, $zero			\n"		\
	"	" st "	$t0, %1				\n"		\
	"	beq	$zero, $t0, 1b			\n"		\
	"2:						\n"		\
	: "=&r" (__ret), "=ZB"(*m)					\
	: "ZB"(*m), "Jr" (old), "Jr" (new)				\
	: "t0", "memory");						\
									\
	__ret;								\
})

extern unsigned long __cmpxchg_small(volatile void *ptr, unsigned long old,
				     unsigned long new, unsigned int size);

static inline unsigned long __cmpxchg(volatile void *ptr, unsigned long old,
				      unsigned long new, unsigned int size)
{
	switch (size) {
	case 1:
	case 2:
		return __cmpxchg_small(ptr, old, new, size);

	case 4:
		return __cmpxchg_asm("ll.w", "sc.w", (volatile u32 *)ptr,
				     (u32)old, new);

	case 8:
		/* lld/scd are only available for LoongArch64 */
		if (!IS_ENABLED(CONFIG_64BIT))
			return __cmpxchg_called_with_bad_pointer();

		return __cmpxchg_asm("ll.d", "sc.d", (volatile u64 *)ptr,
				     (u64)old, new);

	default:
		return __cmpxchg_called_with_bad_pointer();
	}
}

#define arch_cmpxchg_local(ptr, old, new)					\
	((__typeof__(*(ptr)))						\
		__cmpxchg((ptr),					\
			  (unsigned long)(__typeof__(*(ptr)))(old),	\
			  (unsigned long)(__typeof__(*(ptr)))(new),	\
			  sizeof(*(ptr))))

#define arch_cmpxchg(ptr, old, new)						\
({									\
	__typeof__(*(ptr)) __res;					\
									\
	__res = arch_cmpxchg_local((ptr), (old), (new));			\
									\
	__res;								\
})

#ifdef CONFIG_64BIT
#define arch_cmpxchg64_local(ptr, o, n)					\
  ({									\
	BUILD_BUG_ON(sizeof(*(ptr)) != 8);				\
	arch_cmpxchg_local((ptr), (o), (n));					\
  })

#define arch_cmpxchg64(ptr, o, n)						\
  ({									\
	BUILD_BUG_ON(sizeof(*(ptr)) != 8);				\
	arch_cmpxchg((ptr), (o), (n));					\
  })
#else
#include <asm-generic/cmpxchg-local.h>
#define arch_cmpxchg64_local(ptr, o, n) __generic_cmpxchg64_local((ptr), (o), (n))
#define arch_cmpxchg64(ptr, o, n) arch_cmpxchg64_local((ptr), (o), (n))
#endif

#endif /* __ASM_CMPXCHG_H */
