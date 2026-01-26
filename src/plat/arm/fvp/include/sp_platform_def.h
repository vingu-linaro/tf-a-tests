/*
 * Copyright (c) 2020-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains common defines for a secure partition. The correct
 * platform_def.h header file is selected according to the secure partition
 * and platform being built using the make scripts.
 */

#ifndef SP_PLATFORM_DEF_H
#define SP_PLATFORM_DEF_H

#include <platform_def.h>

#define PLAT_SP_RX_BASE			ULL(0x7300000)
#define PLAT_SP_CORE_COUNT		U(8)

#define PLAT_ARM_DEVICE0_BASE		DEVICE0_BASE
#define PLAT_ARM_DEVICE0_SIZE		DEVICE0_SIZE

#define CACTUS_PL011_UART_BASE		PL011_UART2_BASE
#define CACTUS_PL011_UART_CLK_IN_HZ	PL011_UART2_CLK_IN_HZ

#endif /* SP_PLATFORM_DEF_H */
