#
# Copyright (c) 2018-2023, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include build_macros.mk

################################################################################
# Default values for build configurations, and their dependencies
################################################################################
# The Target build architecture. Supported values are: aarch64, aarch32.
ARCH			:= aarch64

# ARM Architecture feature modifiers: none by default
ARM_ARCH_FEATURE	:= none

# ARM Architecture major and minor versions: 8.0 by default.
ARM_ARCH_MAJOR		:= 8
ARM_ARCH_MINOR		:= 0

# Debug/Release build
DEBUG			:= 0

# Build platform
DEFAULT_PLAT		:= fvp

# Build verbosity
V			:= 0

# Select the branch protection features to use
BRANCH_PROTECTION	:= 0
# Use the Firmware Handoff framework to receive configurations from preceding
# bootloader.
TRANSFER_LIST		:= 0

PLAT			:= ${DEFAULT_PLAT}

# Assertions enabled for DEBUG builds by default
ENABLE_ASSERTIONS	:= ${DEBUG}

################################################################################
# Verbose option
################################################################################

ifeq (${V},0)
	Q=@
else
	Q=
endif
export Q

################################################################################
# Toolchain configs
################################################################################
CC			:=	${CROSS_COMPILE}gcc
CPP			:=	${CROSS_COMPILE}cpp
AS			:=	${CROSS_COMPILE}gcc
AR			:=	${CROSS_COMPILE}ar
LD			:=	${CROSS_COMPILE}ld
OC			:=	${CROSS_COMPILE}objcopy
OD			:=	${CROSS_COMPILE}objdump
NM			:=	${CROSS_COMPILE}nm
PP			:=	${CROSS_COMPILE}gcc

ifneq (${DEBUG}, 0)
	BUILD_TYPE	:=	debug
	# Use LOG_LEVEL_INFO by default for debug builds
	LOG_LEVEL       :=      40
else
	BUILD_TYPE	:=	release
	# Use LOG_LEVEL_ERROR by default for release builds
	LOG_LEVEL       :=      20
endif

# Default build string (git branch and commit)
BUILD_STRING  :=  $(shell git describe --always --dirty --tags 2> /dev/null)

VERSION_STRING		:= 	scmi(${PLAT},${BUILD_TYPE}):${BUILD_STRING}

BUILD_BASE		:=	./build
BUILD_PLAT		:=	${BUILD_BASE}/${PLAT}/${BUILD_TYPE}

# List of secure partitions present.
SECURE_PARTITIONS	:=

include scmi.mk

.SUFFIXES:

################################################################################
# Assembler, compiler and linker flags shared across all test images.
################################################################################
COMMON_ASFLAGS		:=
COMMON_CFLAGS		:=
COMMON_LDFLAGS		:=

ifeq (${DEBUG},1)
COMMON_CFLAGS		+= 	-g -gdwarf-4
COMMON_ASFLAGS		+= 	-g -Wa,--gdwarf-4
endif

# Set the compiler's target architecture profile based on ARM_ARCH_MINOR option
ifeq (${ARM_ARCH_MINOR},0)
march64-directive	= 	-march=armv${ARM_ARCH_MAJOR}-a
else
march64-directive	= 	-march=armv${ARM_ARCH_MAJOR}.${ARM_ARCH_MINOR}-a
endif

# Get architecture feature modifiers
arch-features		=	${ARM_ARCH_FEATURE}

# Set the compiler's architecture feature modifiers
ifneq ($(arch-features), none)
march64-directive	:=	$(march64-directive)+$(arch-features)
# Print features
$(info Arm Architecture Features specified: $(subst +, ,$(arch-features)))
endif	# arch-features

################################################################################
# Compiler settings
################################################################################
ifneq ($(findstring clang,$(notdir $(CC))),)
CLANG_CFLAGS_aarch64	:=	-target aarch64-elf

CPP			:=	$(CC) -E $(COMMON_CFLAGS_$(ARCH))
PP			:=	$(CC) -E $(COMMON_CFLAGS_$(ARCH))

CLANG_WARNINGS		+=	-nostdinc -ffreestanding -Wall	\
				-Wmissing-include-dirs $(CLANG_CFLAGS_$(ARCH))	\
				-Wlogical-op-parentheses \
				-Wno-initializer-overrides \
				-Wno-sometimes-uninitialized \
				-Wno-unused-function \
				-Wno-unused-variable \
				-Wno-unused-parameter \
				-Wno-tautological-compare \
				-Wno-memset-transposed-args \
				-Wno-parentheses

CLANG_CFLAGS		+= 	-Wno-error=deprecated-declarations \
				-Wno-error=cpp \
				$(CLANG_WARNINGS)
endif #(clang)

ifneq ($(findstring gcc,$(notdir $(CC))),)
GCC_CFLAGS_aarch32	:=	${march32-directive} -mno-unaligned-access
GCC_CFLAGS_aarch64	:=	-mgeneral-regs-only

GCC_ASFLAGS_aarch32	:=	${march32-directive}
GCC_ASFLAGS_aarch64	:=	-mgeneral-regs-only ${march64-directive}

GCC_WARNINGS		+=	-nostdinc -ffreestanding -Wall -Werror 	\
				-Wmissing-include-dirs  $(GCC_CFLAGS_$(ARCH)) \
				-std=gnu99 -Os

# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105523
GCC_CFLAGS		+=	$(call cc_option, --param=min-pagesize=0)
GCC_CFLAGS		+= 	$(GCC_WARNINGS)
endif #(gcc)

COMMON_CFLAGS_aarch64	+=	${march64-directive} -mstrict-align \
				$(CLANG_CFLAGS_$(ARCH)) $(GCC_CFLAGS_$(ARCH))

COMMON_CFLAGS		+=	$(COMMON_CFLAGS_$(ARCH))
COMMON_CFLAGS		+=	-ffunction-sections -fdata-sections

# Get the content of CFLAGS user defined value last so they are appended after
# the options defined in the Makefile
COMMON_CFLAGS 		+=	${CLANG_CFLAGS} ${GCC_CFLAGS} ${INCLUDES}

COMMON_ASFLAGS		+=	-nostdinc -ffreestanding -Wa,--fatal-warnings	\
				-Werror -Wmissing-include-dirs			\
				-D__ASSEMBLY__ $(GCC_ASFLAGS_$(ARCH))	\
				${INCLUDES}

COMMON_LDFLAGS		+=	${LDFLAGS} --fatal-warnings -O1 --gc-sections --build-id=none

# With ld.bfd version 2.39 and newer new warnings are added. Skip those since we
# are not loaded by a elf loader.
COMMON_LDFLAGS		+=	$(call ld_option, --no-warn-rwx-segments)

################################################################################

ifneq (${BP_OPTION},none)
SCMI_CFLAGS		+= -mbranch-protection=${BP_OPTION}
endif

#####################################################################################
ifneq ($(findstring gcc,$(notdir $(LD))),)
	PIE_LDFLAGS	+=	-Wl,-pie -Wl,--no-dynamic-linker
else
	PIE_LDFLAGS	+=	-pie --no-dynamic-linker
endif

#####################################################################################

SCMI_CFLAGS		+= ${COMMON_CFLAGS} -fpie
SCMI_ASFLAGS		+= ${COMMON_ASFLAGS}
SCMI_LDFLAGS		+= ${COMMON_LDFLAGS} $(PIE_LDFLAGS)

.PHONY: clean
clean:
			@echo "  CLEAN"
			${Q}rm -rf ${BUILD_PLAT}

.PHONY: realclean distclean
realclean distclean:
			@echo "  REALCLEAN"
			${Q}rm -rf ${BUILD_BASE}
			${Q}rm -f ${CURDIR}/cscope.*


MAKE_DEP = -Wp,-MD,$(DEP) -MT $$@

define MAKE_C

$(eval OBJ := $(1)/$(patsubst %.c,%.o,$(notdir $(2))))
$(eval DEP := $(patsubst %.o,%.d,$(OBJ)))

$(OBJ) : $(2)
	@echo "  CC      $$<"
	$$(Q)$$(CC) $$($(3)_CFLAGS) ${$(3)_INCLUDES} ${$(3)_DEFINES} -DIMAGE_$(3) $(MAKE_DEP) -c $$< -o $$@

-include $(DEP)
endef


define MAKE_S

$(eval OBJ := $(1)/$(patsubst %.S,%.o,$(notdir $(2))))
$(eval DEP := $(patsubst %.o,%.d,$(OBJ)))

$(OBJ) : $(2)
	@echo "  AS      $$<"
	$$(Q)$$(AS) $$($(3)_ASFLAGS) ${$(3)_INCLUDES} ${$(3)_DEFINES} -DIMAGE_$(3) $(MAKE_DEP) -c $$< -o $$@

-include $(DEP)
endef


define MAKE_LD

$(eval DEP := $(1).d)

$(1) : $(2)
	@echo "  PP      $$<"
	$$(Q)$$(AS) $$($(3)_ASFLAGS) ${$(3)_INCLUDES} ${$(3)_DEFINES} -P -E $(MAKE_DEP) -o $$@ $$<

-include $(DEP)
endef


define MAKE_OBJS
	$(eval C_OBJS := $(filter %.c,$(2)))
	$(eval REMAIN := $(filter-out %.c,$(2)))
	$(eval $(foreach obj,$(C_OBJS),$(call MAKE_C,$(1),$(obj),$(3))))

	$(eval S_OBJS := $(filter %.S,$(REMAIN)))
	$(eval REMAIN := $(filter-out %.S,$(REMAIN)))
	$(eval $(foreach obj,$(S_OBJS),$(call MAKE_S,$(1),$(obj),$(3))))

	$(and $(REMAIN),$(error Unexpected source files present: $(REMAIN)))
endef


# NOTE: The line continuation '\' is required in the next define otherwise we
# end up with a line-feed characer at the end of the last c filename.
# Also bare this issue in mind if extending the list of supported filetypes.
define SOURCES_TO_OBJS
	$(notdir $(patsubst %.c,%.o,$(filter %.c,$(1)))) \
	$(notdir $(patsubst %.S,%.o,$(filter %.S,$(1))))
endef

define uppercase
$(shell echo $(1) | tr '[:lower:]' '[:upper:]')
endef

define MAKE_IMG
	$(eval IMG_PREFIX := $(call uppercase, $(1)))
	$(eval BUILD_DIR  := ${BUILD_PLAT}/$(1))
	$(eval SOURCES    := $(${IMG_PREFIX}_SOURCES))
	$(eval OBJS       := $(addprefix $(BUILD_DIR)/,$(call SOURCES_TO_OBJS,$(SOURCES))))
	$(eval OBJS       += $(${IMG_PREFIX}_EXTRA_OBJS))
	$(eval LINKERFILE := $(BUILD_DIR)/$(1).ld)
	$(eval MAPFILE    := $(BUILD_DIR)/$(1).map)
	$(eval ELF        := $(BUILD_DIR)/$(1).elf)
	$(eval DUMP       := $(BUILD_DIR)/$(1).dump)
	$(eval BIN        := $(BUILD_PLAT)/$(1).bin)

	$(eval $(call MAKE_OBJS,$(BUILD_DIR),$(SOURCES),${IMG_PREFIX}))
	$(eval $(call MAKE_LD,$(LINKERFILE),$(${IMG_PREFIX}_LINKERFILE),${IMG_PREFIX}))

$(BUILD_DIR) :
	$$(Q)mkdir -p "$$@"

$(ELF) : $(OBJS) $(LINKERFILE)
	@echo "  LD      $$@"
	@echo 'const char build_message[] = "Built : "__TIME__", "__DATE__; \
               const char version_string[] = "${VERSION_STRING}";' | \
		$$(CC) $$(${IMG_PREFIX}_CFLAGS) ${${IMG_PREFIX}_INCLUDES} ${${IMG_PREFIX}_DEFINES} -c -xc - -o $(BUILD_DIR)/build_message.o
	$$(Q)$$(LD) -o $$@ -Map=$(MAPFILE) \
		-T $(LINKERFILE) $(BUILD_DIR)/build_message.o $(OBJS) \
		$$(${IMG_PREFIX}_LDFLAGS)

$(DUMP) : $(ELF)
	@echo "  OD      $$@"
	$${Q}$${OD} -dx $$< > $$@

$(BIN) : $(ELF)
	@echo "  BIN     $$@"
	$$(Q)$$(OC) -O binary $$< $$@
	@echo
	@echo "Built $$@ successfully"
	@echo

.PHONY : $(1)
$(1) : $(BUILD_DIR) $(BIN) $(DUMP)

all : $(1)

endef

$(eval $(call MAKE_IMG,scmi))

SP_LAYOUT:
	${Q}tools/generate_json/generate_json.sh \
		$(BUILD_PLAT) $(SECURE_PARTITIONS)

