/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#ifndef _ASM_PGTABLE_H
#define _ASM_PGTABLE_H

#include <linux/mm_types.h>
#include <linux/mmzone.h>
#include <asm/pgtable-bits.h>
#include <asm/pgtable-64.h>

struct mm_struct;
struct vm_area_struct;

#define PAGE_NONE	__pgprot(_PAGE_PROTNONE | _PAGE_NO_READ | \
				 _PAGE_USER | _CACHE_CC)
#define PAGE_SHARED	__pgprot(_PAGE_PRESENT | _PAGE_WRITE | \
				 _PAGE_USER | _CACHE_CC)
#define PAGE_READONLY	__pgprot(_PAGE_PRESENT | _PAGE_USER | _CACHE_CC)

#define PAGE_KERNEL	__pgprot(_PAGE_PRESENT | __READABLE | __WRITEABLE | \
				 _PAGE_GLOBAL | _PAGE_KERN | _CACHE_CC)
#define PAGE_KERNEL_SUC __pgprot(_PAGE_PRESENT | __READABLE | __WRITEABLE | \
				 _PAGE_GLOBAL | _PAGE_KERN |  _CACHE_SUC)
#define PAGE_KERNEL_WUC __pgprot(_PAGE_PRESENT | __READABLE | __WRITEABLE | \
				 _PAGE_GLOBAL | _PAGE_KERN |  _CACHE_WUC)

/*
 * Dummy values to fill the table in mmap.c
 * The real values will be generated at runtime
 * See setup_protection_map
 */
#define __P000 __pgprot(0)
#define __P001 __pgprot(0)
#define __P010 __pgprot(0)
#define __P011 __pgprot(0)
#define __P100 __pgprot(0)
#define __P101 __pgprot(0)
#define __P110 __pgprot(0)
#define __P111 __pgprot(0)

#define __S000 __pgprot(0)
#define __S001 __pgprot(0)
#define __S010 __pgprot(0)
#define __S011 __pgprot(0)
#define __S100 __pgprot(0)
#define __S101 __pgprot(0)
#define __S110 __pgprot(0)
#define __S111 __pgprot(0)

/*
 * ZERO_PAGE is a global shared page that is always zero; used
 * for zero-mapped memory areas etc..
 */

extern unsigned long empty_zero_page;
extern unsigned long zero_page_mask;

#define ZERO_PAGE(vaddr) \
	(virt_to_page((void *)(empty_zero_page + (((unsigned long)(vaddr)) & zero_page_mask))))
#define __HAVE_COLOR_ZERO_PAGE

extern void paging_init(void);

/*
 * Conversion functions: convert a page and protection to a page entry,
 * and a page entry and page directory to the page they refer to.
 */
#define pmd_phys(pmd)		virt_to_phys((void *)pmd_val(pmd))

#define __pmd_page(pmd)		(pfn_to_page(pmd_phys(pmd) >> PAGE_SHIFT))
#ifndef CONFIG_TRANSPARENT_HUGEPAGE
#define pmd_page(pmd)		__pmd_page(pmd)
#endif /* CONFIG_TRANSPARENT_HUGEPAGE  */

#define pmd_page_vaddr(pmd)	pmd_val(pmd)

static inline void set_pte_at(struct mm_struct *mm, unsigned long addr,
			      pte_t *ptep, pte_t pteval);

#define pte_none(pte)		(!(pte_val(pte) & ~_PAGE_GLOBAL))
#define pte_present(pte)	(pte_val(pte) & (_PAGE_PRESENT | _PAGE_PROTNONE))
#define pte_no_exec(pte)	(pte_val(pte) & _PAGE_NO_EXEC)

static inline void set_pte(pte_t *ptep, pte_t pteval)
{
	*ptep = pteval;
	if (pte_val(pteval) & _PAGE_GLOBAL) {
		pte_t *buddy = ptep_buddy(ptep);
		/*
		 * Make sure the buddy is global too (if it's !none,
		 * it better already be global)
		 */
		if (pte_none(*buddy))
			pte_val(*buddy) = pte_val(*buddy) | _PAGE_GLOBAL;
	}
}

static inline void pte_clear(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
	/* Preserve global status for the pair */
	if (pte_val(*ptep_buddy(ptep)) & _PAGE_GLOBAL)
		set_pte_at(mm, addr, ptep, __pte(_PAGE_GLOBAL));
	else
		set_pte_at(mm, addr, ptep, __pte(0));
}

static inline void set_pte_at(struct mm_struct *mm, unsigned long addr,
			      pte_t *ptep, pte_t pteval)
{
	extern void __update_cache(unsigned long address, pte_t pte);

	if (!pte_present(pteval))
		goto cache_sync_done;

	if (pte_present(*ptep) && (pte_pfn(*ptep) == pte_pfn(pteval)))
		goto cache_sync_done;

	__update_cache(addr, pteval);
cache_sync_done:
	set_pte(ptep, pteval);
}

/*
 * (pmds are folded into puds so this doesn't get actually called,
 * but the define is needed for a generic inline function.)
 */
#define set_pmd(pmdptr, pmdval) do { *(pmdptr) = (pmdval); } while (0)

#ifndef __PAGETABLE_PMD_FOLDED
/*
 * (puds are folded into pgds so this doesn't get actually called,
 * but the define is needed for a generic inline function.)
 */
#define set_pud(pudptr, pudval) do { *(pudptr) = (pudval); } while (0)
#endif

#define PGD_T_LOG2	(__builtin_ffs(sizeof(pgd_t)) - 1)
#define PMD_T_LOG2	(__builtin_ffs(sizeof(pmd_t)) - 1)
#define PTE_T_LOG2	(__builtin_ffs(sizeof(pte_t)) - 1)

extern pgd_t swapper_pg_dir[];
extern pgd_t invalid_pg_dir[];

/*
 * The following only work if pte_present() is true.
 * Undefined behaviour if not..
 */
static inline int pte_write(pte_t pte)	{ return pte_val(pte) & _PAGE_WRITE; }
static inline int pte_dirty(pte_t pte)	{ return pte_val(pte) & _PAGE_DIRTY; }
static inline int pte_young(pte_t pte)	{ return pte_val(pte) & _PAGE_VALID; }

static inline pte_t pte_wrprotect(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_WRITE | _PAGE_SILENT_WRITE);
	return pte;
}

static inline pte_t pte_mkclean(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_DIRTY | _PAGE_SILENT_WRITE);
	return pte;
}

static inline pte_t pte_mkold(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_VALID | _PAGE_SILENT_READ);
	return pte;
}

static inline pte_t pte_mkwrite(pte_t pte)
{
	pte_val(pte) |= _PAGE_WRITE;
	if (pte_val(pte) & _PAGE_DIRTY)
		pte_val(pte) |= _PAGE_SILENT_WRITE;
	return pte;
}

static inline pte_t pte_mkdirty(pte_t pte)
{
	pte_val(pte) |= _PAGE_DIRTY;
	if (pte_val(pte) & _PAGE_WRITE)
		pte_val(pte) |= _PAGE_SILENT_WRITE;
	return pte;
}

static inline pte_t pte_mkyoung(pte_t pte)
{
	pte_val(pte) |= _PAGE_VALID;
	if (!(pte_val(pte) & _PAGE_NO_READ))
		pte_val(pte) |= _PAGE_SILENT_READ;
	return pte;
}

static inline int pte_huge(pte_t pte)	{ return pte_val(pte) & _PAGE_HUGE; }

static inline pte_t pte_mkhuge(pte_t pte)
{
	pte_val(pte) |= _PAGE_HUGE;
	return pte;
}

#if defined(CONFIG_ARCH_HAS_PTE_SPECIAL)
static inline int pte_special(pte_t pte)	{ return pte_val(pte) & _PAGE_SPECIAL; }
static inline pte_t pte_mkspecial(pte_t pte)	{ pte_val(pte) |= _PAGE_SPECIAL; return pte; }
#endif /* CONFIG_ARCH_HAS_PTE_SPECIAL */

#define pte_accessible pte_accessible
static inline unsigned long pte_accessible(struct mm_struct *mm, pte_t a)
{
	if (pte_val(a) & _PAGE_PRESENT)
		return true;

	if ((pte_val(a) & _PAGE_PROTNONE) &&
			mm_tlb_flush_pending(mm))
		return true;

	return false;
}

/*
 * Macro to make mark a page protection value as "uncacheable".	 Note
 * that "protection" is really a misnomer here as the protection value
 * contains the memory attribute bits, dirty bits, and various other
 * bits as well.
 */
#define pgprot_noncached pgprot_noncached

static inline pgprot_t pgprot_noncached(pgprot_t _prot)
{
	unsigned long prot = pgprot_val(_prot);

	prot = (prot & ~_CACHE_MASK) | _CACHE_SUC;

	return __pgprot(prot);
}

#define pgprot_writecombine pgprot_writecombine

static inline pgprot_t pgprot_writecombine(pgprot_t _prot)
{
	unsigned long prot = pgprot_val(_prot);

	/* cpu_data[0].writecombine is already shifted by _CACHE_SHIFT */
	prot = (prot & ~_CACHE_MASK) | cpu_data[0].writecombine;

	return __pgprot(prot);
}

/*
 * Conversion functions: convert a page and protection to a page entry,
 * and a page entry and page directory to the page they refer to.
 */
#define mk_pte(page, pgprot)	pfn_pte(page_to_pfn(page), (pgprot))

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	return __pte((pte_val(pte) & _PAGE_CHG_MASK) |
		     (pgprot_val(newprot) & ~_PAGE_CHG_MASK));
}

extern void __update_tlb(struct vm_area_struct *vma, unsigned long address,
	pte_t *ptep);

static inline void update_mmu_cache(struct vm_area_struct *vma,
	unsigned long address, pte_t *ptep)
{
	__update_tlb(vma, address, ptep);
}

static inline void update_mmu_cache_pmd(struct vm_area_struct *vma,
	unsigned long address, pmd_t *pmdp)
{
	__update_tlb(vma, address, (pte_t *)pmdp);
}

#define kern_addr_valid(addr)	(1)

#ifdef CONFIG_TRANSPARENT_HUGEPAGE

/* We don't have hardware dirty/accessed bits, generic_pmdp_establish is fine.*/
#define pmdp_establish generic_pmdp_establish

#define has_transparent_hugepage has_transparent_hugepage
extern int has_transparent_hugepage(void);

static inline int pmd_trans_huge(pmd_t pmd)
{
	return !!(pmd_val(pmd) & _PAGE_HUGE);
}

static inline pmd_t pmd_mkhuge(pmd_t pmd)
{
	pmd_val(pmd) = (pmd_val(pmd) & ~(_PAGE_GLOBAL)) |
		((pmd_val(pmd) & _PAGE_GLOBAL) << (_PAGE_HGLOBAL_SHIFT - _PAGE_GLOBAL_SHIFT));
	pmd_val(pmd) |= _PAGE_HUGE;

	return pmd;
}

extern void set_pmd_at(struct mm_struct *mm, unsigned long addr,
		       pmd_t *pmdp, pmd_t pmd);

#define pmd_write pmd_write
static inline int pmd_write(pmd_t pmd)
{
	return !!(pmd_val(pmd) & _PAGE_WRITE);
}

static inline pmd_t pmd_wrprotect(pmd_t pmd)
{
	pmd_val(pmd) &= ~(_PAGE_WRITE | _PAGE_SILENT_WRITE);
	return pmd;
}

static inline pmd_t pmd_mkwrite(pmd_t pmd)
{
	pmd_val(pmd) |= _PAGE_WRITE;
	if (pmd_val(pmd) & _PAGE_DIRTY)
		pmd_val(pmd) |= _PAGE_SILENT_WRITE;

	return pmd;
}

static inline int pmd_dirty(pmd_t pmd)
{
	return !!(pmd_val(pmd) & _PAGE_DIRTY);
}

static inline pmd_t pmd_mkclean(pmd_t pmd)
{
	pmd_val(pmd) &= ~(_PAGE_DIRTY | _PAGE_SILENT_WRITE);
	return pmd;
}

static inline pmd_t pmd_mkdirty(pmd_t pmd)
{
	pmd_val(pmd) |= _PAGE_DIRTY;
	if (pmd_val(pmd) & _PAGE_WRITE)
		pmd_val(pmd) |= _PAGE_SILENT_WRITE;

	return pmd;
}

static inline int pmd_young(pmd_t pmd)
{
	return !!(pmd_val(pmd) & _PAGE_VALID);
}

static inline pmd_t pmd_mkold(pmd_t pmd)
{
	pmd_val(pmd) &= ~(_PAGE_VALID|_PAGE_SILENT_READ);

	return pmd;
}

static inline pmd_t pmd_mkyoung(pmd_t pmd)
{
	pmd_val(pmd) |= _PAGE_VALID;

	if (!(pmd_val(pmd) & _PAGE_NO_READ))
		pmd_val(pmd) |= _PAGE_SILENT_READ;

	return pmd;
}

/* Extern to avoid header file madness */
extern pmd_t mk_pmd(struct page *page, pgprot_t prot);

static inline unsigned long pmd_pfn(pmd_t pmd)
{
	return (pmd_val(pmd) & _PFN_MASK) >> _PFN_SHIFT;
}

static inline struct page *pmd_page(pmd_t pmd)
{
	if (pmd_trans_huge(pmd))
		return pfn_to_page(pmd_pfn(pmd));

	return pfn_to_page(pmd_phys(pmd) >> PAGE_SHIFT);
}

static inline pmd_t pmd_modify(pmd_t pmd, pgprot_t newprot)
{
	pmd_val(pmd) = (pmd_val(pmd) & _HPAGE_CHG_MASK) |
				(pgprot_val(newprot) & ~_HPAGE_CHG_MASK);
	return pmd;
}

static inline pmd_t pmd_mkinvalid(pmd_t pmd)
{
	pmd_val(pmd) &= ~(_PAGE_PRESENT | _PAGE_VALID | _PAGE_DIRTY | _PAGE_PROTNONE);

	return pmd;
}

/*
 * The generic version pmdp_huge_get_and_clear uses a version of pmd_clear() with a
 * different prototype.
 */
#define __HAVE_ARCH_PMDP_HUGE_GET_AND_CLEAR
static inline pmd_t pmdp_huge_get_and_clear(struct mm_struct *mm,
					    unsigned long address, pmd_t *pmdp)
{
	pmd_t old = *pmdp;

	pmd_clear(pmdp);

	return old;
}

#endif /* CONFIG_TRANSPARENT_HUGEPAGE */

/*
 * We provide our own get_unmapped area to cope with the virtual aliasing
 * constraints placed on us by the cache architecture.
 */
#define HAVE_ARCH_UNMAPPED_AREA
#define HAVE_ARCH_UNMAPPED_AREA_TOPDOWN

#endif /* _ASM_PGTABLE_H */
