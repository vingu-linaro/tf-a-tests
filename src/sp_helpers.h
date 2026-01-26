/*
 * Copyright (c) 2018-2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SP_HELPERS_H
#define SP_HELPERS_H

#include <stdint.h>
#include <spm_common.h>
#include <spinlock.h>

/* Currently, Hafnium/SPM supports 1024 virtual interrupt IDs. */
#define NUM_VINT_ID	1024

void sp_handler_spin_lock_init(void);

/* Handler invoked by SP while processing interrupt. */
extern void (*sp_interrupt_handler[NUM_VINT_ID])(void);

/* Register the handler. */
void sp_register_interrupt_handler(void (*handler)(void),
						uint32_t interrupt_id);

/* Un-register the handler. */
void sp_unregister_interrupt_handler(uint32_t interrupt_id);

void discover_managed_exit_interrupt_id(void);

void register_maintenance_interrupt_handlers(void);

#endif /* SP_HELPERS_H */
