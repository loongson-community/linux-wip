/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#ifndef _ASM_IO_H
#define _ASM_IO_H

#define ARCH_HAS_IOREMAP_WC

#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/types.h>

#include <asm/addrspace.h>
#include <asm/bug.h>
#include <asm/byteorder.h>
#include <asm/cpu.h>
#include <asm-generic/iomap.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/string.h>

#define ioswabb(a, x)		(x)
#define ioswabw(a, x)		(x)
#define ioswabl(a, x)		(x)
#define ioswabq(a, x)		(x)
#define __mem_ioswabb(a, x)	(x)
#define __mem_ioswabw(a, x)	(x)
#define __mem_ioswabl(a, x)	(x)
#define __mem_ioswabq(a, x)	(x)
#define __raw_ioswabb(a, x)	(x)
#define __raw_ioswabw(a, x)	(x)
#define __raw_ioswabl(a, x)	(x)
#define __raw_ioswabq(a, x)	(x)

#define IO_SPACE_LIMIT 0xffff

/*
 * On LoongArch I/O ports are memory mapped, so we access them using normal
 * load/store instructions. loongarch_io_port_base is the virtual address to
 * which all ports are being mapped.  For sake of efficiency some code
 * assumes that this is an address that can be loaded with a single lui
 * instruction, so the lower 16 bits must be zero. Should be true on any
 * sane architecture; generic code does not use this assumption.
 */
extern unsigned long loongarch_io_port_base;

static inline void set_io_port_base(unsigned long base)
{
	loongarch_io_port_base = base;
}

/*
 * Provide the necessary definitions for generic iomap. We make use of
 * loongarch_io_port_base for iomap(), but we don't reserve any low addresses
 * for use with I/O ports.
 */

#define HAVE_ARCH_PIO_SIZE
#define PIO_OFFSET	loongarch_io_port_base
#define PIO_MASK	IO_SPACE_LIMIT
#define PIO_RESERVED	0x0UL

/*
 * ISA I/O bus memory addresses are 1:1 with the physical address.
 */
static inline unsigned long isa_virt_to_bus(volatile void *address)
{
	return virt_to_phys(address);
}

static inline void *isa_bus_to_virt(unsigned long address)
{
	return phys_to_virt(address);
}

/*
 * However PCI ones are not necessarily 1:1 and therefore these interfaces
 * are forbidden in portable PCI drivers.
 *
 * Allow them for x86 for legacy drivers, though.
 */
#define virt_to_bus virt_to_phys
#define bus_to_virt phys_to_virt

/*
 * Change "struct page" to physical address.
 */
#define page_to_phys(page)	((dma_addr_t)page_to_pfn(page) << PAGE_SHIFT)

extern void __init __iomem *early_ioremap(u64 phys_addr, unsigned long size);
extern void __init early_iounmap(void __iomem *addr, unsigned long size);

#define early_memremap early_ioremap
#define early_memunmap early_iounmap

static inline void __iomem *ioremap_prot(phys_addr_t offset, unsigned long size,
	unsigned long prot_val)
{
	/* This only works for !HIGHMEM currently */
	return (void __iomem *)(unsigned long)(IO_BASE + offset);
}

/*
 * ioremap     -   map bus memory into CPU space
 * @offset:    bus address of the memory
 * @size:      size of the resource to map
 *
 * ioremap performs a platform specific sequence of operations to
 * make bus memory CPU accessible via the readb/readw/readl/writeb/
 * writew/writel functions and the other mmio helpers. The returned
 * address is not guaranteed to be usable directly as a virtual
 * address.
 */
#define ioremap(offset, size)					\
	ioremap_prot((offset), (size), _CACHE_SUC)
#define ioremap_uc ioremap

/*
 * ioremap_cache -	map bus memory into CPU space
 * @offset:	    bus address of the memory
 * @size:	    size of the resource to map
 *
 * ioremap_cache performs a platform specific sequence of operations to
 * make bus memory CPU accessible via the readb/readw/readl/writeb/
 * writew/writel functions and the other mmio helpers. The returned
 * address is not guaranteed to be usable directly as a virtual
 * address.
 *
 * This version of ioremap ensures that the memory is marked cachable by
 * the CPU.  Also enables full write-combining.	 Useful for some
 * memory-like regions on I/O busses.
 */
#define ioremap_cache(offset, size)				\
	ioremap_prot((offset), (size), _CACHE_CC)

/*
 * ioremap_wc     -   map bus memory into CPU space
 * @offset:    bus address of the memory
 * @size:      size of the resource to map
 *
 * ioremap_wc performs a platform specific sequence of operations to
 * make bus memory CPU accessible via the readb/readw/readl/writeb/
 * writew/writel functions and the other mmio helpers. The returned
 * address is not guaranteed to be usable directly as a virtual
 * address.
 *
 * This version of ioremap ensures that the memory is marked uncachable
 * but accelerated by means of write-combining feature. It is specifically
 * useful for PCIe prefetchable windows, which may vastly improve a
 * communications performance. If it was determined on boot stage, what
 * CPU CCA doesn't support WUC, the method shall fall-back to the
 * _CACHE_SUC option (see cpu_probe() method).
 */
#define ioremap_wc(offset, size)				\
	ioremap_prot((offset), (size), boot_cpu_data.writecombine)

static inline void iounmap(const volatile void __iomem *addr)
{
}

#define io_reorder_wmb()		wmb()

#define __BUILD_MEMORY_SINGLE(pfx, bwlq, type)				\
									\
static inline void pfx##write##bwlq(type val,				\
				    volatile void __iomem *mem)		\
{									\
	volatile type *__mem;						\
	type __val;							\
									\
	io_reorder_wmb();						\
									\
	__mem = (void *)(unsigned long)(mem);				\
									\
	__val = pfx##ioswab##bwlq(__mem, val);				\
									\
	if (sizeof(type) != sizeof(u64) || sizeof(u64) == sizeof(long)) \
		*__mem = __val;						\
	else								\
		BUG();							\
}									\
									\
static inline type pfx##read##bwlq(const volatile void __iomem *mem)	\
{									\
	volatile type *__mem;						\
	type __val;							\
									\
	__mem = (void *)(unsigned long)(mem);				\
									\
	if (sizeof(type) != sizeof(u64) || sizeof(u64) == sizeof(long)) \
		__val = *__mem;						\
	else {								\
		__val = 0;						\
		BUG();							\
	}								\
									\
	/* prevent prefetching of coherent DMA data prematurely */	\
	rmb();								\
	return pfx##ioswab##bwlq(__mem, __val);				\
}

#define __BUILD_IOPORT_SINGLE(pfx, bwlq, type, p)			\
									\
static inline void pfx##out##bwlq##p(type val, unsigned long port)	\
{									\
	volatile type *__addr;						\
	type __val;							\
									\
	io_reorder_wmb();						\
									\
	__addr = (void *)(loongarch_io_port_base + port);		\
									\
	__val = pfx##ioswab##bwlq(__addr, val);				\
									\
	/* Really, we want this to be atomic */				\
	BUILD_BUG_ON(sizeof(type) > sizeof(unsigned long));		\
									\
	*__addr = __val;						\
}									\
									\
static inline type pfx##in##bwlq##p(unsigned long port)			\
{									\
	volatile type *__addr;						\
	type __val;							\
									\
	__addr = (void *)(loongarch_io_port_base + port);		\
									\
	BUILD_BUG_ON(sizeof(type) > sizeof(unsigned long));		\
									\
	__val = *__addr;						\
									\
	/* prevent prefetching of coherent DMA data prematurely */	\
	rmb();								\
	return pfx##ioswab##bwlq(__addr, __val);			\
}

#define __BUILD_MEMORY_PFX(bus, bwlq, type)				\
									\
__BUILD_MEMORY_SINGLE(bus, bwlq, type)

#define BUILDIO_MEM(bwlq, type)						\
									\
__BUILD_MEMORY_PFX(__raw_, bwlq, type)					\
__BUILD_MEMORY_PFX(, bwlq, type)					\
__BUILD_MEMORY_PFX(__mem_, bwlq, type)					\

BUILDIO_MEM(b, u8)
BUILDIO_MEM(w, u16)
BUILDIO_MEM(l, u32)
BUILDIO_MEM(q, u64)

#define __BUILD_IOPORT_PFX(bus, bwlq, type)				\
	__BUILD_IOPORT_SINGLE(bus, bwlq, type,)				\
	__BUILD_IOPORT_SINGLE(bus, bwlq, type, _p)

#define BUILDIO_IOPORT(bwlq, type)					\
	__BUILD_IOPORT_PFX(, bwlq, type)				\
	__BUILD_IOPORT_PFX(__mem_, bwlq, type)

BUILDIO_IOPORT(b, u8)
BUILDIO_IOPORT(w, u16)
BUILDIO_IOPORT(l, u32)
#ifdef CONFIG_64BIT
BUILDIO_IOPORT(q, u64)
#endif

#define readb_relaxed			readb
#define readw_relaxed			readw
#define readl_relaxed			readl
#define readq_relaxed			readq

#define writeb_relaxed			writeb
#define writew_relaxed			writew
#define writel_relaxed			writel
#define writeq_relaxed			writeq

#define readb_be(addr)							\
	__raw_readb((__force unsigned *)(addr))
#define readw_be(addr)							\
	be16_to_cpu(__raw_readw((__force unsigned *)(addr)))
#define readl_be(addr)							\
	be32_to_cpu(__raw_readl((__force unsigned *)(addr)))
#define readq_be(addr)							\
	be64_to_cpu(__raw_readq((__force unsigned *)(addr)))

#define writeb_be(val, addr)						\
	__raw_writeb((val), (__force unsigned *)(addr))
#define writew_be(val, addr)						\
	__raw_writew(cpu_to_be16((val)), (__force unsigned *)(addr))
#define writel_be(val, addr)						\
	__raw_writel(cpu_to_be32((val)), (__force unsigned *)(addr))
#define writeq_be(val, addr)						\
	__raw_writeq(cpu_to_be64((val)), (__force unsigned *)(addr))

/*
 * Some code tests for these symbols
 */
#define readq				readq
#define writeq				writeq

#define __BUILD_MEMORY_STRING(bwlq, type)				\
									\
static inline void writes##bwlq(volatile void __iomem *mem,		\
				const void *addr, unsigned int count)	\
{									\
	const volatile type *__addr = addr;				\
									\
	while (count--) {						\
		__mem_write##bwlq(*__addr, mem);			\
		__addr++;						\
	}								\
}									\
									\
static inline void reads##bwlq(volatile void __iomem *mem, void *addr,	\
			       unsigned int count)			\
{									\
	volatile type *__addr = addr;					\
									\
	while (count--) {						\
		*__addr = __mem_read##bwlq(mem);			\
		__addr++;						\
	}								\
}

#define __BUILD_IOPORT_STRING(bwlq, type)				\
									\
static inline void outs##bwlq(unsigned long port, const void *addr,	\
			      unsigned int count)			\
{									\
	const volatile type *__addr = addr;				\
									\
	while (count--) {						\
		__mem_out##bwlq(*__addr, port);				\
		__addr++;						\
	}								\
}									\
									\
static inline void ins##bwlq(unsigned long port, void *addr,		\
			     unsigned int count)			\
{									\
	volatile type *__addr = addr;					\
									\
	while (count--) {						\
		*__addr = __mem_in##bwlq(port);				\
		__addr++;						\
	}								\
}

#define BUILDSTRING(bwlq, type)						\
									\
__BUILD_MEMORY_STRING(bwlq, type)					\
__BUILD_IOPORT_STRING(bwlq, type)

BUILDSTRING(b, u8)
BUILDSTRING(w, u16)
BUILDSTRING(l, u32)
#ifdef CONFIG_64BIT
BUILDSTRING(q, u64)
#endif

#define mmiowb() asm volatile ("dbar 0" ::: "memory")

/*
 * String version of I/O memory access operations.
 */
extern void __memset_io(volatile void __iomem *dst, int c, size_t count);
extern void __memcpy_toio(volatile void __iomem *to, const void *from, size_t count);
extern void __memcpy_fromio(void *to, const volatile void __iomem *from, size_t count);
#define memset_io(c, v, l)     __memset_io((c), (v), (l))
#define memcpy_fromio(a, c, l) __memcpy_fromio((a), (c), (l))
#define memcpy_toio(c, a, l)   __memcpy_toio((c), (a), (l))

/*
 * Convert a physical pointer to a virtual kernel pointer for /dev/mem
 * access
 */
#define xlate_dev_mem_ptr(p)	__va(p)

/*
 * Convert a virtual cached pointer to an uncached pointer
 */
#define xlate_dev_kmem_ptr(p)	p

#endif /* _ASM_IO_H */
