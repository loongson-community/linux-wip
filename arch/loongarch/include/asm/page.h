/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#ifndef _ASM_PAGE_H
#define _ASM_PAGE_H

#include <linux/const.h>

/*
 * PAGE_SHIFT determines the page size
 */
#ifdef CONFIG_PAGE_SIZE_4KB
#define PAGE_SHIFT	12
#endif
#ifdef CONFIG_PAGE_SIZE_16KB
#define PAGE_SHIFT	14
#endif
#ifdef CONFIG_PAGE_SIZE_64KB
#define PAGE_SHIFT	16
#endif
#define PAGE_SIZE	(_AC(1, UL) << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE - 1))

#define HPAGE_SHIFT	(PAGE_SHIFT + PAGE_SHIFT - 3)
#define HPAGE_SIZE	(_AC(1, UL) << HPAGE_SHIFT)
#define HPAGE_MASK	(~(HPAGE_SIZE - 1))
#define HUGETLB_PAGE_ORDER	(HPAGE_SHIFT - PAGE_SHIFT)

#ifndef __ASSEMBLY__

#include <linux/kernel.h>
#include <linux/pfn.h>

/*
 * It's normally defined only for FLATMEM config but it's
 * used in our early mem init code for all memory models.
 * So always define it.
 */
#define ARCH_PFN_OFFSET	PFN_UP(PHYS_OFFSET)

extern void clear_page(void *page);
extern void copy_page(void *to, void *from);

extern unsigned long shm_align_mask;

struct page;

static inline void clear_user_page(void *addr, unsigned long vaddr,
	struct page *page)
{
	clear_page(addr);
}

struct vm_area_struct;
extern void copy_user_highpage(struct page *to, struct page *from,
	unsigned long vaddr, struct vm_area_struct *vma);

#define __HAVE_ARCH_COPY_USER_HIGHPAGE

/*
 * These are used to make use of C type-checking..
 */
typedef struct { unsigned long pte; } pte_t;
#define pte_val(x)	((x).pte)
#define __pte(x)	((pte_t) { (x) })
typedef struct page *pgtable_t;

/*
 * Right now we don't support 4-level pagetables, so all pud-related
 * definitions come from <asm-generic/pgtable-nopud.h>.
 */

/*
 * Finall the top of the hierarchy, the pgd
 */
typedef struct { unsigned long pgd; } pgd_t;
#define pgd_val(x)	((x).pgd)
#define __pgd(x)	((pgd_t) { (x) })

/*
 * Manipulate page protection bits
 */
typedef struct { unsigned long pgprot; } pgprot_t;
#define pgprot_val(x)	((x).pgprot)
#define __pgprot(x)	((pgprot_t) { (x) })
#define pte_pgprot(x)	__pgprot(pte_val(x) & ~_PFN_MASK)

#define ptep_buddy(x)	((pte_t *)((unsigned long)(x) ^ sizeof(pte_t)))

/*
 * __pa()/__va() should be used only during mem init.
 */
#define __pa(x)		PHYSADDR(x)
#define __va(x)		((void *)((unsigned long)(x) + PAGE_OFFSET - PHYS_OFFSET))

#ifndef __pa_symbol
#define __pa_symbol(x)	__pa(RELOC_HIDE((unsigned long)(x), 0))
#endif

#define pfn_to_kaddr(pfn)	__va((pfn) << PAGE_SHIFT)

/*
 *     virt_to_phys    -       map virtual addresses to physical
 *     @address: address to remap
 *
 *     The returned physical address is the physical (CPU) mapping for
 *     the memory address given. It is only valid to use this function on
 *     addresses directly mapped or allocated via kmalloc.
 *
 *     This function does not give bus mappings for DMA transfers. In
 *     almost all conceivable cases a device driver should not be using
 *     this function
 */
static inline unsigned long virt_to_phys(volatile const void *address)
{
	return __pa(address);
}

/*
 *     phys_to_virt    -       map physical address to virtual
 *     @address: address to remap
 *
 *     The returned virtual address is a current CPU mapping for
 *     the memory address given. It is only valid to use this function on
 *     addresses that have a kernel mapping
 *
 *     This function does not handle bus mappings for DMA transfers. In
 *     almost all conceivable cases a device driver should not be using
 *     this function
 */
static inline void *phys_to_virt(unsigned long address)
{
	return __va(address);
}

#ifdef CONFIG_FLATMEM

static inline int pfn_valid(unsigned long pfn)
{
	/* avoid <linux/mm.h> include hell */
	extern unsigned long max_mapnr;
	unsigned long pfn_offset = ARCH_PFN_OFFSET;

	return pfn >= pfn_offset && pfn < max_mapnr;
}

#endif

#define virt_to_pfn(kaddr)	PFN_DOWN(virt_to_phys((void *)(kaddr)))
#define virt_to_page(kaddr)	pfn_to_page(virt_to_pfn(kaddr))

extern int __virt_addr_valid(const volatile void *kaddr);
#define virt_addr_valid(kaddr)						\
	__virt_addr_valid((const volatile void *) (kaddr))

#define VM_DATA_DEFAULT_FLAGS \
	(VM_READ | VM_WRITE | \
	 ((current->personality & READ_IMPLIES_EXEC) ? VM_EXEC : 0) | \
	 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

extern unsigned long __kaslr_offset;
static inline unsigned long kaslr_offset(void)
{
	return __kaslr_offset;
}

#include <asm-generic/memory_model.h>
#include <asm-generic/getorder.h>

#endif /* !__ASSEMBLY__ */

#endif /* _ASM_PAGE_H */
