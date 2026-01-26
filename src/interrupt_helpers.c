/*
 * Copyright (c) 2021-2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>

#include <mmio.h>
#include <stdint.h>
#include <stdlib.h>
#include <ffa_svc.h>

#include <ffa_helpers.h>
#include "spm_common.h"
#include "sp_helpers.h"
#include "spm_helpers.h"

#include <platform_def.h>

#define NOTIFICATION_PENDING_INTERRUPT_INTID 5

extern ffa_id_t g_ffa_id;
extern ffa_id_t g_dir_req_source_id;
static uint32_t managed_exit_interrupt_id;

spinlock_t sp_handler_lock[NUM_VINT_ID];

void (*sp_interrupt_handler[NUM_VINT_ID])(void);

void sp_handler_spin_lock_init(void)
{
	for (uint32_t i = 0; i < NUM_VINT_ID; i++) {
		init_spinlock(&sp_handler_lock[i]);
	}
}

void sp_register_interrupt_handler(void (*handler)(void),
			uint32_t interrupt_id)
{
	if (interrupt_id >= NUM_VINT_ID) {
		ERROR("Cannot register handler for interrupt %u\n", interrupt_id);
	}

	spin_lock(&sp_handler_lock[interrupt_id]);
	sp_interrupt_handler[interrupt_id] = handler;
	spin_unlock(&sp_handler_lock[interrupt_id]);
}

void sp_unregister_interrupt_handler(uint32_t interrupt_id)
{
	if (interrupt_id >= NUM_VINT_ID) {
		ERROR("Cannot unregister handler for interrupt %u\n", interrupt_id);
	}

	spin_lock(&sp_handler_lock[interrupt_id]);
	sp_interrupt_handler[interrupt_id] = NULL;
	spin_unlock(&sp_handler_lock[interrupt_id]);
}

/*******************************************************************************
 * Hypervisor Calls Wrappers
 ******************************************************************************/

uint32_t spm_interrupt_get(void)
{
	hvc_args args = {
		.fid = SPM_INTERRUPT_GET
	};

	hvc_ret_values ret = tftf_hvc(&args);

	return ret.ret0;
}

/**
 * Hypervisor call to enable/disable SP delivery of a virtual interrupt of
 * int_id value through the IRQ or FIQ vector (pin).
 * Returns 0 on success, or -1 if passing an invalid interrupt id.
 */
int64_t spm_interrupt_enable(uint32_t int_id, bool enable, enum interrupt_pin pin)
{
	hvc_args args = {
		.fid = SPM_INTERRUPT_ENABLE,
		.arg1 = int_id,
		.arg2 = enable,
		.arg3 = pin
	};

	hvc_ret_values ret = tftf_hvc(&args);

	return (int64_t)ret.ret0;
}

/**
 * Hypervisor call to drop the priority and de-activate a secure interrupt.
 * Returns 0 on success, or -1 if passing an invalid interrupt id.
 */
int64_t spm_interrupt_deactivate(uint32_t vint_id)
{
	hvc_args args = {
		.fid = SPM_INTERRUPT_DEACTIVATE,
		.arg1 = vint_id, /* pint_id */
		.arg2 = vint_id
	};

	hvc_ret_values ret = tftf_hvc(&args);

	return (int64_t)ret.ret0;
}

/*
 * Managed exit ID discoverable by querying the SPMC through
 * FFA_FEATURES API.
 */
void discover_managed_exit_interrupt_id(void)
{
	struct ffa_value ffa_ret;

	/* Interrupt ID value is returned through register W2. */
	ffa_ret = ffa_features(FFA_FEATURE_MEI);
	managed_exit_interrupt_id = ffa_feature_intid(ffa_ret);

	VERBOSE("Discovered managed exit interrupt ID: %d\n",
	     managed_exit_interrupt_id);
}

/*
 * SCMI SP does not implement application threads. Hence, once the SCMI SP
 * sends the managed exit response to the direct request originator, execution
 * is still frozen in interrupt handler context.
 * Though it moves to WAITING state, it is not able to accept new direct request
 * message from any endpoint. It can only receive a direct request message with
 * the command CACTUS_RESUME_AFTER_MANAGED_EXIT from the originator of the
 * suspended direct request message in order to return from the interrupt
 * handler context and resume the processing of suspended request.
 */
void send_managed_exit_response(void)
{
	struct ffa_value ffa_ret;
	bool waiting_resume_after_managed_exit;

	/*
	 * A secure partition performs its housekeeping and sends a direct
	 * response to signal interrupt completion. This is a pure virtual
	 * interrupt, no need for deactivation.
	 */
	ffa_ret = ffa_msg_send_direct_resp64(g_ffa_id, g_dir_req_source_id,
			MANAGED_EXIT_INTERRUPT_ID
			, 0, 0, 0, 0);
	waiting_resume_after_managed_exit = true;

	while (waiting_resume_after_managed_exit) {

		waiting_resume_after_managed_exit =
			(ffa_func_id(ffa_ret) != FFA_MSG_SEND_DIRECT_REQ_SMC32 &&
			 ffa_func_id(ffa_ret) != FFA_MSG_SEND_DIRECT_REQ_SMC64) ||
			 ffa_dir_msg_source(ffa_ret) != g_dir_req_source_id;

		if (waiting_resume_after_managed_exit) {
			VERBOSE("Expected a direct message request from endpoint"
			      " %x to resume the command\n",
			       g_dir_req_source_id);
			ffa_ret = ffa_msg_send_direct_resp64(g_ffa_id,
						    ffa_dir_msg_source(ffa_ret),
						    U(-1),
						    0, 0, 0, 0);
		}
	}
	VERBOSE("Resuming the suspended command\n");
}

void notification_pending_interrupt_handler(void)
{
	/* Get which core it is running from. */
	unsigned int core_pos = platform_get_core_pos(
						read_mpidr_el1() & MPID_MASK);

	VERBOSE("NPI handled in core %u\n", core_pos);
}

void register_maintenance_interrupt_handlers(void)
{
	sp_register_interrupt_handler(send_managed_exit_response,
		managed_exit_interrupt_id);
	sp_register_interrupt_handler(notification_pending_interrupt_handler,
		NOTIFICATION_PENDING_INTERRUPT_INTID);
}

void scmi_interrupt_handler_irq(void)
{
	uint32_t intid = spm_interrupt_get();

	/* Invoke the handler registered by the SP. */
	spin_lock(&sp_handler_lock[intid]);
	if (sp_interrupt_handler[intid]) {
		sp_interrupt_handler[intid]();
	} else {
		ERROR("%s: Interrupt ID %x not handled!\n", __func__, intid);
	}
	spin_unlock(&sp_handler_lock[intid]);
}

void scmi_interrupt_handler_fiq(void)
{
	uint32_t intid = spm_interrupt_get();

	if (intid == MANAGED_EXIT_INTERRUPT_ID) {
		/*
		 * A secure partition performs its housekeeping and sends a
		 * direct response to signal interrupt completion.
		 * This is a pure virtual interrupt, no need for deactivation.
		 */
		VERBOSE("vFIQ: Sending ME response to %x\n",
			g_dir_req_source_id);
		send_managed_exit_response();
	} else {
		/*
		 * Currently only managed exit interrupt is supported by vFIQ.
		 */
		ERROR("%s: vFIQ ID %x not supported!\n", __func__, intid);
	}
}
