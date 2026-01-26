/*
 * Copyright (c) 2021-2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <ffa_helpers.h>
#include <ffa_svc.h>
#include <lib/extensions/sve.h>
#include <spm_common.h>
#include <xlat_tables_v2.h>

bool memory_retrieve(struct mailbox_buffers *mb,
		     struct ffa_memory_region **retrieved, uint64_t handle,
		     ffa_id_t sender, ffa_id_t receiver,
		     ffa_memory_region_flags_t flags,
		     uint32_t mem_func)
{
	struct ffa_value ret;
	uint32_t fragment_size;
	uint32_t total_size;
	uint32_t descriptor_size;
	const enum ffa_instruction_access inst_access =
				(mem_func == FFA_MEM_SHARE_SMC32)
					? FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED
					: FFA_INSTRUCTION_ACCESS_NX;

	if (retrieved == NULL || mb == NULL) {
		ERROR("Invalid parameters!\n");
		return false;
	}

	descriptor_size = ffa_memory_retrieve_request_init(
	    mb->send, handle, sender, receiver, 0, flags,
	    FFA_DATA_ACCESS_RW,
	    inst_access,
	    FFA_MEMORY_NORMAL_MEM,
	    FFA_MEMORY_CACHE_WRITE_BACK,
	    FFA_MEMORY_INNER_SHAREABLE);

	ret = ffa_mem_retrieve_req(descriptor_size, descriptor_size);

	if (ffa_func_id(ret) != FFA_MEM_RETRIEVE_RESP) {
		ERROR("Couldn't retrieve the memory page. Error: %x\n",
		      ffa_error_code(ret));
		return false;
	}

	/*
	 * Following total_size and fragment_size are useful to keep track
	 * of the state of transaction. When the sum of all fragment_size of all
	 * fragments is equal to total_size, the memory transaction has been
	 * completed.
	 * This is a simple test with only one segment. As such, upon
	 * successful ffa_mem_retrieve_req, total_size must be equal to
	 * fragment_size.
	 */
	total_size = ret.arg1;
	fragment_size = ret.arg2;

	if (total_size != fragment_size) {
		ERROR("Only expect one memory segment to be sent!\n");
		return false;
	}

	if (fragment_size > PAGE_SIZE) {
		ERROR("Fragment should be smaller than RX buffer!\n");
		return false;
	}

	*retrieved = (struct ffa_memory_region *)mb->recv;

	if ((*retrieved)->receiver_count > MAX_MEM_SHARE_RECIPIENTS) {
		VERBOSE("SPMC memory sharing operations support max of %u "
			"receivers!\n", MAX_MEM_SHARE_RECIPIENTS);
		return false;
	}

	VERBOSE("Memory Retrieved!\n");

	return true;
}

bool memory_relinquish(struct ffa_mem_relinquish *m, uint64_t handle,
		       ffa_id_t id)
{
	struct ffa_value ret;

	ffa_mem_relinquish_init(m, handle, 0, id);
	ret = ffa_mem_relinquish();
	if (ffa_func_id(ret) != FFA_SUCCESS_SMC32) {
		ERROR("%s failed to relinquish memory! error: %x\n",
		      __func__, ffa_error_code(ret));
		return false;
	}

	VERBOSE("Memory Relinquished!\n");
	return true;
}
