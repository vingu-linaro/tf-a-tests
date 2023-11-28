#
# Copyright (c) 2020, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

FVP_SCMI_BASE		= spm/scmi/plat/arm/fvp

PLAT_INCLUDES		+= -I${FVP_SCMI_BASE}/include/

# Add the FDT source
SCMI_DTS		= ${FVP_SCMI_BASE}/fdts/scmi.dts

# List of FDTS to copy
#FDTS_CP_LIST		= ${FVP_SCMI_BASE}/fdts/scmi.dts
