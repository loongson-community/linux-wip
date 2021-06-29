/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LOONGARCH_SPARSEMEM_H
#define _LOONGARCH_SPARSEMEM_H

#ifdef CONFIG_SPARSEMEM

/*
 * SECTION_SIZE_BITS		2^N: how big each section will be
 * MAX_PHYSMEM_BITS		2^N: how much memory we can have in that space
 */
#define SECTION_SIZE_BITS	29
#define MAX_PHYSMEM_BITS	48

#endif /* CONFIG_SPARSEMEM */

#ifdef CONFIG_MEMORY_HOTPLUG
int memory_add_physaddr_to_nid(u64 addr);
#define memory_add_physaddr_to_nid memory_add_physaddr_to_nid
#endif

#endif /* _LOONGARCH_SPARSEMEM_H */
