#
# Copyright (c) 2018-2023, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include branch_protection.mk
XLAT_TABLES_LIB_V2	:=	1

# Include scmi platform make file
PLAT_SCMI_BASE		= src/plat/arm/${PLAT}
PLAT_INCLUDES		+= -I${PLAT_SCMI_BASE}/include/

# Add the FDT source
SCMI_DTS		= ${PLAT_SCMI_BASE}/fdts/scmi.dts

SCMI_DTB		:= $(BUILD_PLAT)/scmi.dtb
SECURE_PARTITIONS	+= scmi

SCMI_INCLUDES :=					\
	-Iinclude					\
	-Iinclude/common				\
	-Iinclude/lib					\
	-Iinclude/lib/${ARCH}				\
	-Iinclude/lib/extensions			\
	-Iinclude/lib/xlat_tables			\
	-Iinclude/lib/libc				\
	-Iinclude/lib/libc/$(ARCH)			\
	-Isrc 						\
	-Isrc/${ARCH}					\
	-Isrc/lib/xlat_tables_v2 			\
	-I${PLAT_SCMI_BASE}/include/ 		

SCMI_INCLUDES += -I${INCLUDE_SCMI}
SCMI_LDFLAGS += -L${LIB_SCMI}
SCMI_LDFLAGS += -lscmi-fw-all --static

SCMI_SOURCES	:=					\
	$(addprefix src/,				\
		${ARCH}/ffa_arch_helpers.S		\
		ffa_helpers.c				\
		memory_helpers.c			\
		interrupt_helpers.c			\
		debug_helpers.c				\
		${ARCH}/scmi_entrypoint.S		\
		${ARCH}/scmi_exceptions.S		\
		scmi_main.c	 			\
		${ARCH}/exception_report.c		\
		${ARCH}/drivers/pl011/pl011_console.S	\
		${ARCH}/lib/cache_helpers.S		\
		${ARCH}/lib/locks/spinlock.S		\
		${ARCH}/lib/misc_helpers.S		\
		${ARCH}/lib/smc/asm_smc.S		\
		${ARCH}/lib/smc/smc.c			\
		${ARCH}/lib/smc/hvc.c			\
		plat/arm/fvp/${ARCH}/plat_helpers.S	\
		${ARCH}/lib/xlat_tables_v2/enable_mmu.S	\
		${ARCH}/lib/xlat_tables_v2/xlat_tables_arch.c	\
		lib/xlat_tables_v2/xlat_tables_context.c	\
		lib/xlat_tables_v2/xlat_tables_core.c		\
		lib/xlat_tables_v2/xlat_tables_utils.c 		\
	)

SCMI_SOURCES	+= 					\
	$(addprefix src/lib/libc/,				\
		abort.c					\
		assert.c				\
		exit.c					\
		memchr.c				\
		memcmp.c				\
		memcpy.c				\
		memmove.c				\
		memset.c				\
		printf.c				\
		putchar.c				\
		puts.c					\
		rand.c					\
		snprintf.c				\
		strchr.c				\
		strcmp.c				\
		strlcpy.c				\
		strlen.c				\
		strncmp.c				\
		strncpy.c				\
		strnlen.c				\
		strrchr.c 				\
	)

SCMI_LINKERFILE	:=	src/scmi.ld.S

SCMI_DEFINES	:=

$(eval $(call add_define,SCMI_DEFINES,ARM_ARCH_MAJOR))
$(eval $(call add_define,SCMI_DEFINES,ARM_ARCH_MINOR))
$(eval $(call add_define,SCMI_DEFINES,DEBUG))
$(eval $(call add_define,SCMI_DEFINES,ENABLE_ASSERTIONS))
$(eval $(call add_define,SCMI_DEFINES,ENABLE_BTI))
$(eval $(call add_define,SCMI_DEFINES,ENABLE_PAUTH))
$(eval $(call add_define,SCMI_DEFINES,LOG_LEVEL))
$(eval $(call add_define,SCMI_DEFINES,PLAT_${PLAT}))
$(eval $(call add_define,SCMI_DEFINES,PLAT_XLAT_TABLES_DYNAMIC))

include ${PLAT_SCMI_BASE}/platform.mk

$(SCMI_DTB) : $(BUILD_PLAT)/scmi $(BUILD_PLAT)/scmi/scmi.elf
$(SCMI_DTB) : $(SCMI_DTS)
	@echo "  DTBGEN  $@"
	${Q}tools/generate_dtb/generate_dtb.sh \
		scmi ${SCMI_DTS} $(BUILD_PLAT) $(SCMI_DTB)
	@echo
	@echo "Built $@ successfully"
	@echo

scmi: $(SCMI_DTB)

# FDTS_CP copies flattened device tree sources
#   $(1) = output directory
#   $(2) = flattened device tree source file to copy
define FDTS_CP
        $(eval FDTS := $(addprefix $(1)/,$(notdir $(2))))
$(FDTS): $(2) $(SCMI_DTB)
	@echo "  CP      $$<"
	${Q}cp $$< $$@
endef

        $(eval files := $(SCMI_DTS))
        $(eval $(foreach file,$(files),$(call FDTS_CP,$(BUILD_PLAT),$(file))))
scmi: $(FDTS) SP_LAYOUT

