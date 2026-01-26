/*
 * Copyright (c) 2018-2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <assert_macros.S>
#include <lib/xlat_tables/xlat_tables_defs.h>


/* ---------------------------------------------------------------------------
 * Helper to fixup Global Offset table (GOT) and dynamic relocations
 * (.rela.dyn) at runtime.
 *
 * This function is meant to be used when the firmware is compiled with -fpie
 * and linked with -pie options. We rely on the linker script exporting
 * appropriate markers for start and end of the section. For GOT, we
 * expect __GOT_START__ and __GOT_END__. Similarly for .rela.dyn, we expect
 * __RELA_START__ and __RELA_END__.
 *
 * The function takes the limits of the memory to apply fixups to as
 * arguments (which is usually the limits of the relocable BL image).
 *   x0 -  the start of the fixup region
 *   x1 -  the limit of the fixup region
 * These addresses have to be page (4KB aligned).
 * ---------------------------------------------------------------------------
 */
.globl fixup_gdt_reloc
func fixup_gdt_reloc
	mov	x6, x0
	mov	x7, x1

	/* Test if the limits are 4K aligned */
#if ENABLE_ASSERTIONS
	orr	x0, x0, x1
	tst	x0, #(PAGE_SIZE - 1)
	ASM_ASSERT(eq)
#endif
	/*
	 * Calculate the offset based on return address in x30.
	 * Assume that this function is called within a page at the start of
	 * fixup region.
	 */
	and	x2, x30, #~(PAGE_SIZE - 1)
	sub	x0, x2, x6	/* Diff(S) = Current Address - Compiled Address */

	adrp	x1, __GOT_START__
	add	x1, x1, :lo12:__GOT_START__
	adrp	x2, __GOT_END__
	add	x2, x2, :lo12:__GOT_END__

	/*
	 * GOT is an array of 64_bit addresses which must be fixed up as
	 * new_addr = old_addr + Diff(S).
	 * The new_addr is the address currently the binary is executing from
	 * and old_addr is the address at compile time.
	 */
1:
	ldr	x3, [x1]
	/* Skip adding offset if address is < lower limit */
	cmp	x3, x6
	b.lo	2f
	/* Skip adding offset if address is >= upper limit */
	cmp	x3, x7
	b.ge	2f
	add	x3, x3, x0
	str	x3, [x1]
2:
	add	x1, x1, #8
	cmp	x1, x2
	b.lo	1b

	/* Starting dynamic relocations. Use adrp/adr to get RELA_START and END */
	adrp	x1, __RELA_START__
	add	x1, x1, :lo12:__RELA_START__
	adrp	x2, __RELA_END__
	add	x2, x2, :lo12:__RELA_END__
	/*
	 * According to ELF-64 specification, the RELA data structure is as
	 * follows:
	 *	typedef struct
	 * 	{
	 *		Elf64_Addr r_offset;
	 *		Elf64_Xword r_info;
	 *		Elf64_Sxword r_addend;
	 *	} Elf64_Rela;
	 *
	 * r_offset is address of reference
	 * r_info is symbol index and type of relocation (in this case
	 * 0x403 which corresponds to R_AARCH64_RELATIVE).
	 * r_addend is constant part of expression.
	 *
	 * Size of Elf64_Rela structure is 24 bytes.
	 */
1:
	/* Assert that the relocation type is R_AARCH64_RELATIVE */
#if ENABLE_ASSERTIONS
	ldr	x3, [x1, #8]
	cmp	x3, #0x403
	ASM_ASSERT(eq)
#endif
	ldr	x3, [x1]	/* r_offset */
	add	x3, x0, x3
	ldr	x4, [x1, #16]	/* r_addend */

	/* Skip adding offset if r_addend is < lower limit */
	cmp	x4, x6
	b.lo	2f
	/* Skip adding offset if r_addend entry is >= upper limit */
	cmp	x4, x7
	b.ge	2f

	add	x4, x0, x4	/* Diff(S) + r_addend */
	str	x4, [x3]

2:	add	x1, x1, #24
	cmp	x1, x2
	b.lo	1b

	ret
endfunc fixup_gdt_reloc
