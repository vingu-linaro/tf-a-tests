/*
 * Copyright (c) 2017-2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SCMI_H__
#define __SCMI_H__

#include <stdint.h>

/* Linker symbols used to figure out the memory layout of Cactus. */
extern uintptr_t __TEXT_START__, __TEXT_END__;
#define SCMI_TEXT_START	((uintptr_t)&__TEXT_START__)
#define SCMI_TEXT_END		((uintptr_t)&__TEXT_END__)

extern uintptr_t __RODATA_START__, __RODATA_END__;
#define SCMI_RODATA_START	((uintptr_t)&__RODATA_START__)
#define SCMI_RODATA_END	((uintptr_t)&__RODATA_END__)

extern uintptr_t __DATA_START__, __DATA_END__;
#define SCMI_DATA_START	((uintptr_t)&__DATA_START__)
#define SCMI_DATA_END		((uintptr_t)&__DATA_END__)

extern uintptr_t __BSS_START__, __BSS_END__;
#define SCMI_BSS_START	((uintptr_t)&__BSS_START__)
#define SCMI_BSS_END		((uintptr_t)&__BSS_END__)

#endif /* __SCMI_H__ */
