/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#ifndef _ASM_UACCESS_H
#define _ASM_UACCESS_H

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/extable.h>
#include <asm-generic/extable.h>

/*
 * The fs value determines whether argument validity checking should be
 * performed or not.  If get_fs() == USER_DS, checking is performed, with
 * get_fs() == KERNEL_DS, checking is bypassed.
 *
 * For historical reasons, these macros are grossly misnamed.
 */

#ifdef CONFIG_64BIT

extern u64 __ua_limit;

#define __UA_LIMIT	__ua_limit

#define __UA_ADDR	".dword"
#define __UA_ADDU	"add.d"
#define __UA_LA		"la.abs"

#endif /* CONFIG_64BIT */

/*
 * Is a address valid? This does a straightforward calculation rather
 * than tests.
 *
 * Address valid if:
 *  - "addr" doesn't have any high-bits set
 *  - AND "size" doesn't have any high-bits set
 *  - AND "addr+size" doesn't have any high-bits set
 *  - OR we are in kernel mode.
 *
 * __ua_size() is a trick to avoid runtime checking of positive constant
 * sizes; for those we already know at compile time that the size is ok.
 */
#define __ua_size(size)							\
	((__builtin_constant_p(size) && (signed long) (size) > 0) ? 0 : (size))

/*
 * access_ok: - Checks if a user space pointer is valid
 * @addr: User space pointer to start of block to check
 * @size: Size of block to check
 *
 * Context: User context only. This function may sleep if pagefaults are
 *          enabled.
 *
 * Checks if a pointer to a block of memory in user space is valid.
 *
 * Returns true (nonzero) if the memory block may be valid, false (zero)
 * if it is definitely invalid.
 *
 * Note that, depending on architecture, this function probably just
 * checks that the pointer is in the user space range - after calling
 * this function, memory access functions may still return -EFAULT.
 */
static inline int __access_ok(const void __user *p, unsigned long size)
{
	unsigned long addr = (unsigned long)p;
	unsigned long end = addr + size - !!size;

	return (__UA_LIMIT & (addr | end | __ua_size(size))) == 0;
}

#define access_ok(addr, size)					\
	likely(__access_ok((addr), (size)))

/*
 * get_user: - Get a simple variable from user space.
 * @x:	 Variable to store result.
 * @ptr: Source address, in user space.
 *
 * Context: User context only. This function may sleep if pagefaults are
 *          enabled.
 *
 * This macro copies a single simple variable from user space to kernel
 * space.  It supports simple types like char and int, but not larger
 * data types like structures or arrays.
 *
 * @ptr must have pointer-to-simple-variable type, and the result of
 * dereferencing @ptr must be assignable to @x without a cast.
 *
 * Returns zero on success, or -EFAULT on error.
 * On error, the variable @x is set to zero.
 */
#define get_user(x, ptr) \
({									\
	int __gu_err = -EFAULT;						\
	const __typeof__(*(ptr)) __user *__gu_ptr = (ptr);		\
									\
	might_fault();							\
	if (likely(access_ok(__gu_ptr, sizeof(*(ptr))))) {		\
		__get_user_common((x), sizeof(*(ptr)), __gu_ptr);	\
	} else								\
		(x) = 0;						\
									\
	__gu_err;							\
})

/*
 * put_user: - Write a simple value into user space.
 * @x:	 Value to copy to user space.
 * @ptr: Destination address, in user space.
 *
 * Context: User context only. This function may sleep if pagefaults are
 *          enabled.
 *
 * This macro copies a single simple value from kernel space to user
 * space.  It supports simple types like char and int, but not larger
 * data types like structures or arrays.
 *
 * @ptr must have pointer-to-simple-variable type, and @x must be assignable
 * to the result of dereferencing @ptr.
 *
 * Returns zero on success, or -EFAULT on error.
 */
#define put_user(x, ptr) \
({									\
	__typeof__(*(ptr)) __user *__pu_addr = (ptr);			\
	__typeof__(*(ptr)) __pu_val = (x);				\
	int __pu_err = -EFAULT;						\
									\
	might_fault();							\
	if (likely(access_ok(__pu_addr, sizeof(*(ptr))))) {		\
		__put_user_common(__pu_addr, sizeof(*(ptr)));		\
	}								\
									\
	__pu_err;							\
})

/*
 * __get_user: - Get a simple variable from user space, with less checking.
 * @x:	 Variable to store result.
 * @ptr: Source address, in user space.
 *
 * Context: User context only. This function may sleep if pagefaults are
 *          enabled.
 *
 * This macro copies a single simple variable from user space to kernel
 * space.  It supports simple types like char and int, but not larger
 * data types like structures or arrays.
 *
 * @ptr must have pointer-to-simple-variable type, and the result of
 * dereferencing @ptr must be assignable to @x without a cast.
 *
 * Caller must check the pointer with access_ok() before calling this
 * function.
 *
 * Returns zero on success, or -EFAULT on error.
 * On error, the variable @x is set to zero.
 */
#define __get_user(x, ptr) \
({									\
	int __gu_err;							\
									\
	__chk_user_ptr(ptr);						\
	__get_user_common((x), sizeof(*(ptr)), ptr);			\
	__gu_err;							\
})

/*
 * __put_user: - Write a simple value into user space, with less checking.
 * @x:	 Value to copy to user space.
 * @ptr: Destination address, in user space.
 *
 * Context: User context only. This function may sleep if pagefaults are
 *          enabled.
 *
 * This macro copies a single simple value from kernel space to user
 * space.  It supports simple types like char and int, but not larger
 * data types like structures or arrays.
 *
 * @ptr must have pointer-to-simple-variable type, and @x must be assignable
 * to the result of dereferencing @ptr.
 *
 * Caller must check the pointer with access_ok() before calling this
 * function.
 *
 * Returns zero on success, or -EFAULT on error.
 */
#define __put_user(x, ptr) \
({									\
	__typeof__(*(ptr)) __pu_val;					\
	int __pu_err = 0;						\
									\
	__pu_val = (x);							\
	__chk_user_ptr(ptr);						\
	__put_user_common(ptr, sizeof(*(ptr)));					\
	__pu_err;							\
})

struct __large_struct { unsigned long buf[100]; };
#define __m(x) (*(struct __large_struct __user *)(x))

#ifdef CONFIG_32BIT
#define __GET_DW(val, insn, ptr) __get_data_asm_ll32(val, insn, ptr)
#endif
#ifdef CONFIG_64BIT
#define __GET_DW(val, insn, ptr) __get_data_asm(val, insn, ptr)
#endif

#define __get_user_common(val, size, ptr)				\
do {									\
	switch (size) {							\
	case 1: __get_data_asm(val, "ld.b", ptr); break;		\
	case 2: __get_data_asm(val, "ld.h", ptr); break;		\
	case 4: __get_data_asm(val, "ld.w", ptr); break;		\
	case 8: __GET_DW(val, "ld.d", ptr); break;			\
	default: BUILD_BUG(); break;					\
	}								\
} while (0)

#define __get_kernel_common(val, size, ptr) __get_user_common(val, size, ptr)

#define __get_data_asm(val, insn, addr)					\
{									\
	long __gu_tmp;							\
									\
	__asm__ __volatile__(						\
	"1:	" insn "	%1, %3				\n"	\
	"2:							\n"	\
	"	.section .fixup,\"ax\"				\n"	\
	"3:	li.w	%0, %4					\n"	\
	"	or	%1, $r0, $r0				\n"	\
	"	b	2b					\n"	\
	"	.previous					\n"	\
	"	.section __ex_table,\"a\"			\n"	\
	"	"__UA_ADDR "\t1b, 3b				\n"	\
	"	.previous					\n"	\
	: "=r" (__gu_err), "=r" (__gu_tmp)				\
	: "0" (0), "o" (__m(addr)), "i" (-EFAULT));			\
									\
	(val) = (__typeof__(*(addr))) __gu_tmp;				\
}

/*
 * Get a long long 64 using 32 bit registers.
 */
#define __get_data_asm_ll32(val, insn, addr)				\
{									\
	union {								\
		unsigned long long	l;				\
		__typeof__(*(addr))	t;				\
	} __gu_tmp;							\
									\
	__asm__ __volatile__(						\
	"1:	ld.w	%1, (%3)				\n"	\
	"2:	ld.w	%D1, 4(%3)				\n"	\
	"3:							\n"	\
	"	.section	.fixup,\"ax\"			\n"	\
	"4:	li.w	%0, %4					\n"	\
	"	slli.d	%1, $r0, 0				\n"	\
	"	slli.d	%D1, $r0, 0				\n"	\
	"	b	3b					\n"	\
	"	.previous					\n"	\
	"	.section	__ex_table,\"a\"		\n"	\
	"	" __UA_ADDR "	1b, 4b				\n"	\
	"	" __UA_ADDR "	2b, 4b				\n"	\
	"	.previous					\n"	\
	: "=r" (__gu_err), "=&r" (__gu_tmp.l)				\
	: "0" (0), "r" (addr), "i" (-EFAULT));				\
									\
	(val) = __gu_tmp.t;						\
}

#ifdef CONFIG_32BIT
#define __PUT_DW(insn, ptr) __put_data_asm_ll32(insn, ptr)
#endif
#ifdef CONFIG_64BIT
#define __PUT_DW(insn, ptr) __put_data_asm(insn, ptr)
#endif

#define __put_user_common(ptr, size)					\
do {									\
	switch (size) {							\
	case 1: __put_data_asm("st.b", ptr); break;			\
	case 2: __put_data_asm("st.h", ptr); break;			\
	case 4: __put_data_asm("st.w", ptr); break;			\
	case 8: __PUT_DW("st.d", ptr); break;				\
	default: BUILD_BUG(); break;					\
	}								\
} while (0)

#define __put_kernel_common(ptr, size) __put_user_common(ptr, size)

#define __put_data_asm(insn, ptr)					\
{									\
	__asm__ __volatile__(						\
	"1:	" insn "	%z2, %3		# __put_user_asm\n"	\
	"2:							\n"	\
	"	.section	.fixup,\"ax\"			\n"	\
	"3:	li.w	%0, %4					\n"	\
	"	b	2b					\n"	\
	"	.previous					\n"	\
	"	.section	__ex_table,\"a\"		\n"	\
	"	" __UA_ADDR "	1b, 3b				\n"	\
	"	.previous					\n"	\
	: "=r" (__pu_err)						\
	: "0" (0), "Jr" (__pu_val), "o" (__m(ptr)),			\
	  "i" (-EFAULT));						\
}

#define __put_data_asm_ll32(insn, ptr)					\
{									\
	__asm__ __volatile__(						\
	"1:	st.w	%2, (%3)	# __put_user_asm_ll32	\n"	\
	"2:	st.w	%D2, 4(%3)				\n"	\
	"3:							\n"	\
	"	.section	.fixup,\"ax\"			\n"	\
	"4:	li.w	%0, %4					\n"	\
	"	b	3b					\n"	\
	"	.previous					\n"	\
	"	.section	__ex_table,\"a\"		\n"	\
	"	" __UA_ADDR "	1b, 4b				\n"	\
	"	" __UA_ADDR "	2b, 4b				\n"	\
	"	.previous"						\
	: "=r" (__pu_err)						\
	: "0" (0), "r" (__pu_val), "r" (ptr),				\
	  "i" (-EFAULT));						\
}

#define HAVE_GET_KERNEL_NOFAULT

#define __get_kernel_nofault(dst, src, type, err_label)			\
do {									\
	int __gu_err;							\
									\
	__get_kernel_common(*((type *)(dst)), sizeof(type),		\
			    (__force type *)(src));			\
	if (unlikely(__gu_err))						\
		goto err_label;						\
} while (0)

#define __put_kernel_nofault(dst, src, type, err_label)			\
do {									\
	type __pu_val;					\
	int __pu_err = 0;						\
									\
	__pu_val = *(__force type *)(src);				\
	__put_kernel_common(((type *)(dst)), sizeof(type));		\
	if (unlikely(__pu_err))						\
		goto err_label;						\
} while (0)

extern __kernel_size_t __copy_user(void *to, const void *from, __kernel_size_t n);

static inline unsigned long
raw_copy_from_user(void *to, const void __user *from, unsigned long n)
{
	return __copy_user(to, from, n);
}

static inline unsigned long
raw_copy_to_user(void __user *to, const void *from, unsigned long n)
{
	return __copy_user(to, from, n);
}

#define INLINE_COPY_FROM_USER
#define INLINE_COPY_TO_USER

static inline unsigned long
raw_copy_in_user(void __user *to, const void __user *from, unsigned long n)
{
	return __copy_user(to, from, n);
}

/*
 * __clear_user: - Zero a block of memory in user space, with less checking.
 * @addr: Destination address, in user space.
 * @size: Number of bytes to zero.
 *
 * Zero a block of memory in user space.  Caller must check
 * the specified block with access_ok() before calling this function.
 *
 * Returns number of bytes that could not be cleared.
 * On success, this will be zero.
 */
extern __kernel_size_t __clear_user(void __user *addr, __kernel_size_t size);

#define clear_user(addr, n)						\
({									\
	void __user *__cl_addr = (addr);				\
	unsigned long __cl_size = (n);					\
	if (__cl_size && access_ok(__cl_addr, __cl_size))		\
		__cl_size = __clear_user(__cl_addr, __cl_size);		\
	__cl_size;							\
})

extern long __strncpy_from_user(char *to, const char __user *from, long len);

/*
 * strncpy_from_user: - Copy a NUL terminated string from userspace.
 * @to:   Destination address, in kernel space.  This buffer must be at
 *	  least @len bytes long.
 * @from: Source address, in user space.
 * @len:  Maximum number of bytes to copy, including the trailing NUL.
 *
 * Copies a NUL-terminated string from userspace to kernel space.
 *
 * On success, returns the length of the string (not including the trailing
 * NUL).
 *
 * If access to userspace fails, returns -EFAULT (some data may have been
 * copied).
 *
 * If @len is smaller than the length of the string, copies @len bytes
 * and returns @len.
 */
static inline long
strncpy_from_user(char *to, const char __user *from, long len)
{
	if (!access_ok(from, len))
		return -EFAULT;

	might_fault();
	return __strncpy_from_user(to, from, len);
}

extern long __strnlen_user(const char __user *s, long n);

/*
 * strnlen_user: - Get the size of a string in user space.
 * @s: The string to measure.
 *
 * Context: User context only. This function may sleep if pagefaults are
 *          enabled.
 *
 * Get the size of a NUL-terminated string in user space.
 *
 * Returns the size of the string INCLUDING the terminating NUL.
 * On exception, returns 0.
 * If the string is too long, returns a value greater than @n.
 */
static inline long strnlen_user(const char __user *s, long n)
{
	if (!access_ok(s, 1))
		return 0;

	might_fault();
	return __strnlen_user(s, n);
}

#endif /* _ASM_UACCESS_H */
