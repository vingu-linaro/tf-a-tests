#
# Copyright (c) 2018-2023, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include branch_protection.mk
include lib/xlat_tables_v2/xlat_tables.mk

# Include cactus platform make file
SCMI_PLAT_PATH	:= $(shell find spm/scmi/plat -wholename '*/${PLAT}')
ifneq (${SCMI_PLAT_PATH},)
	include ${SCMI_PLAT_PATH}/platform.mk
endif

SCMI_DTB		:= $(BUILD_PLAT)/scmi.dtb
SECURE_PARTITIONS	+= scmi

SCMI_INCLUDES :=					\
	-Ispm/scmi/include/ext				\
	-Ispm/scmi/include/ext/common			\
	-Ispm/scmi/include/ext/lib			\
	-Ispm/scmi/include/ext/lib/${ARCH}		\
	-Ispm/scmi/include/ext/lib/extensions		\
	-Ispm/scmi/include/ext/lib/xlat_tables		\
	-Ispm/scmi/${ARCH}				\
	-Ispm/scmi/include				\
	-Ispm/scmi/include/${ARCH}			\
	-Ispm/scmi 					\
	-Ispm/scmi/${ARCH}				\
	-Ispm/scmi/plat/arm/fvp/include/ 		\
	-Ispm/scmi/include/ext/plat/arm/common/

SCMI_INCLUDES += -I${INCLUDE_SCMI}
SCMI_LDFLAGS += -L${LIB_SCMI}
SCMI_LDFLAGS += -lscmi-fw-all --static

SCMI_SOURCES	:=					\
	$(addprefix spm/scmi/,				\
		${ARCH}/cactus_entrypoint.S		\
		${ARCH}/cactus_exceptions.S		\
		cactus_interrupt.c			\
		cactus_main.c				\
		sp_debug.c				\
		${ARCH}/framework/asm_debug.S		\
		${ARCH}/ffa_arch_helpers.S		\
		ffa_helpers.c				\
		spm_common.c				\
		${ARCH}/framework/exception_report.c	\
		${ARCH}/drivers/pl011/pl011_console.S	\
		${ARCH}/lib/cache_helpers.S		\
		${ARCH}/lib/misc_helpers.S		\
		${ARCH}/lib/smc/asm_smc.S		\
		${ARCH}/lib/smc/smc.c			\
		${ARCH}/lib/smc/hvc.c			\
		${ARCH}/lib/exceptions/sync.c		\
		${ARCH}/lib/locks/spinlock.S		\
		plat/arm/fvp/${ARCH}/plat_helpers.S	\
		mp_printf.c				\
		${ARCH}/lib/xlat_tables_v2/enable_mmu.S	\
		${ARCH}/lib/xlat_tables_v2/xlat_tables_arch.c	\
		xlat_tables_context.c			\
		xlat_tables_core.c			\
		xlat_tables_utils.c 			\
	)

SCMI_SOURCES	+=	$(addprefix spm/scmi/libc/,	\
			abort.c				\
			assert.c			\
			exit.c				\
			memchr.c			\
			memcmp.c			\
			memcpy.c			\
			memmove.c			\
			memset.c			\
			printf.c			\
			putchar.c			\
			puts.c				\
			rand.c				\
			snprintf.c			\
			strchr.c			\
			strcmp.c			\
			strlcpy.c			\
			strlen.c			\
			strncmp.c			\
			strncpy.c			\
			strnlen.c			\
			strrchr.c)

ifeq (${ARCH},aarch64)
SCMI_SOURCES	+=	$(addprefix spm/scmi/libc/aarch64/,	\
			setjmp.S)
endif

SCMI_LINKERFILE	:=	spm/scmi/cactus.ld.S

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
scmi: $(FDTS)

