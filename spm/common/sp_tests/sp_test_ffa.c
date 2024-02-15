/*
 * Copyright (c) 2018-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ffa_helpers.h"
#include <assert.h>
#include <debug.h>
#include <errno.h>

#include <sp_def.h>
#include <ffa_endpoints.h>
#include <sp_helpers.h>
#include <spm_helpers.h>
#include <spm_common.h>
#include <lib/libc/string.h>
#include <xlat_tables_defs.h>
#include <lib/xlat_tables/xlat_tables_v2.h>

/* FFA version test helpers */
#define FFA_MAJOR 1U
#define FFA_MINOR 1U

static uint32_t spm_version;

static const struct ffa_uuid sp_uuids[] = {
		{PRIMARY_UUID}, {SECONDARY_UUID}, {TERTIARY_UUID}, {IVY_UUID}, {EL3_SPMD_LP_UUID}
	};

static const struct ffa_partition_info ffa_expected_partition_info[] = {
	/* Primary partition info */
	{
		.id = SP_ID(1),
		.exec_context = PRIMARY_EXEC_CTX_COUNT,
		.properties = (FFA_PARTITION_AARCH64_EXEC |
			       FFA_PARTITION_DIRECT_REQ_RECV |
			       FFA_PARTITION_DIRECT_REQ_SEND |
			       FFA_PARTITION_NOTIFICATION),
		.uuid = {PRIMARY_UUID}
	},
	/* Secondary partition info */
	{
		.id = SP_ID(2),
		.exec_context = SECONDARY_EXEC_CTX_COUNT,
		.properties = (FFA_PARTITION_AARCH64_EXEC |
			       FFA_PARTITION_DIRECT_REQ_RECV |
			       FFA_PARTITION_DIRECT_REQ_SEND |
			       FFA_PARTITION_NOTIFICATION),
		.uuid = {SECONDARY_UUID}
	},
	/* Tertiary partition info */
	{
		.id = SP_ID(3),
		.exec_context = TERTIARY_EXEC_CTX_COUNT,
		.properties = (FFA_PARTITION_AARCH64_EXEC |
			       FFA_PARTITION_DIRECT_REQ_RECV |
			       FFA_PARTITION_DIRECT_REQ_SEND |
			       FFA_PARTITION_NOTIFICATION),
		.uuid = {TERTIARY_UUID}
	},
	/* Ivy partition info */
	{
		.id = SP_ID(4),
		.exec_context = IVY_EXEC_CTX_COUNT,
		.properties = (FFA_PARTITION_AARCH64_EXEC |
			       FFA_PARTITION_DIRECT_REQ_RECV |
			       FFA_PARTITION_DIRECT_REQ_SEND),
		.uuid = {IVY_UUID}
	},
	/* EL3 SPMD logical partition */
	{
		.id = SP_ID(0x7FC0),
		.exec_context = EL3_SPMD_LP_EXEC_CTX_COUNT,
		.properties = (FFA_PARTITION_AARCH64_EXEC |
			       FFA_PARTITION_DIRECT_REQ_SEND),
		.uuid = {EL3_SPMD_LP_UUID}
	},
};

/*
 * Test FFA_FEATURES interface.
 */
static void ffa_features_test(void)
{
	struct ffa_value ffa_ret;
	unsigned int expected_ret;
	const struct ffa_features_test *ffa_feature_test_target;
	unsigned int i, test_target_size =
		get_ffa_feature_test_target(&ffa_feature_test_target);
	struct ffa_features_test test_target;

	INFO("Test FFA_FEATURES.\n");

	for (i = 0U; i < test_target_size; i++) {
		test_target = ffa_feature_test_target[i];

		ffa_ret = ffa_features_with_input_property(test_target.feature,
							   test_target.param);
		expected_ret = FFA_VERSION_COMPILED
				>= test_target.version_added ?
				test_target.expected_ret : FFA_ERROR;

		if (ffa_func_id(ffa_ret) != expected_ret) {
			ERROR("Unexpected return: %x (expected %x)."
			      " FFA_FEATURES test: %s.\n",
			      ffa_func_id(ffa_ret), expected_ret,
			      test_target.test_name);
		}

		if (expected_ret == FFA_ERROR) {
			if (ffa_error_code(ffa_ret) !=
			    FFA_ERROR_NOT_SUPPORTED) {
				ERROR("Unexpected error code: %x (expected %x)."
				      " FFA_FEATURES test: %s.\n",
				      ffa_error_code(ffa_ret), expected_ret,
				      test_target.test_name);
			}
		}
	}
}

static void ffa_partition_info_wrong_test(void)
{
	const struct ffa_uuid uuid = { .uuid = {1} };
	struct ffa_value ret = ffa_partition_info_get(uuid);

	VERBOSE("%s: test request wrong UUID.\n", __func__);

	expect(ffa_func_id(ret), FFA_ERROR);
	expect(ffa_error_code(ret), FFA_ERROR_INVALID_PARAMETER);
}

static void ffa_partition_info_get_regs_test(void)
{
	struct ffa_value ret = { 0 };

	VERBOSE("FF-A Partition Info regs interface tests\n");
	ret = ffa_version(MAKE_FFA_VERSION(1, 1));
	uint32_t version = ret.fid;

	if (version == FFA_ERROR_NOT_SUPPORTED) {
		ERROR("FFA_VERSION 1.1 not supported, skipping"
			" FFA_PARTITION_INFO_GET_REGS test.\n");
		return;
	}

	ret = ffa_features(FFA_PARTITION_INFO_GET_REGS_SMC64);
	if (ffa_func_id(ret) != FFA_SUCCESS_SMC32) {
		ERROR("FFA_PARTITION_INFO_GET_REGS not supported skipping tests.\n");
		return;
	}

	expect(ffa_partition_info_regs_helper(sp_uuids[3],
		&ffa_expected_partition_info[3], 1), true);
	expect(ffa_partition_info_regs_helper(sp_uuids[2],
		&ffa_expected_partition_info[2], 1), true);
	expect(ffa_partition_info_regs_helper(sp_uuids[1],
		&ffa_expected_partition_info[1], 1), true);
	expect(ffa_partition_info_regs_helper(sp_uuids[0],
		&ffa_expected_partition_info[0], 1), true);

	/*
	 * Check partition information if there is support for SPMD EL3
	 * partitions. calling partition_info_get_regs with the SPMD EL3
	 * UUID successfully, indicates the presence of it (there is no
	 * spec defined way to discover presence of el3 spmd logical
	 * partitions). If the call fails with a not supported error,
	 * we assume they dont exist and skip further tests to avoid
	 * failures on platforms without el3 spmd logical partitions.
	 */
	ret = ffa_partition_info_get_regs(sp_uuids[4], 0, 0);
	if ((ffa_func_id(ret) == FFA_ERROR) &&
	    ((ffa_error_code(ret) == FFA_ERROR_NOT_SUPPORTED) ||
	    (ffa_error_code(ret) == FFA_ERROR_INVALID_PARAMETER))) {
		INFO("Skipping register based EL3 SPMD Logical partition"
				" discovery\n");
		expect(ffa_partition_info_regs_helper(NULL_UUID,
			ffa_expected_partition_info,
			(ARRAY_SIZE(ffa_expected_partition_info) - 1)), true);
	} else {
		expect(ffa_partition_info_regs_helper(sp_uuids[4],
			&ffa_expected_partition_info[4], 1), true);
		expect(ffa_partition_info_regs_helper(NULL_UUID,
			ffa_expected_partition_info,
			ARRAY_SIZE(ffa_expected_partition_info)), true);
	}
}

static void ffa_partition_info_get_test(struct mailbox_buffers *mb)
{
	INFO("Test FFA_PARTITION_INFO_GET.\n");

	expect(ffa_partition_info_helper(mb, sp_uuids[2],
		&ffa_expected_partition_info[2], 1), true);

	expect(ffa_partition_info_helper(mb, sp_uuids[1],
		&ffa_expected_partition_info[1], 1), true);

	expect(ffa_partition_info_helper(mb, sp_uuids[0],
		&ffa_expected_partition_info[0], 1), true);

	/*
	 * TODO: ffa_partition_info_get_regs returns EL3 SPMD LP information
	 * but partition_info_get does not. Ignore the last entry, that is
	 * assumed to be the EL3 SPMD LP information. ffa_partition_info_get
	 * uses the rx/tx buffer and the SPMD does not support the use of
	 * rx/tx buffer to return SPMD logical partition information.
	 */
	expect(ffa_partition_info_helper(mb, NULL_UUID,
		ffa_expected_partition_info,
		(ARRAY_SIZE(ffa_expected_partition_info) - 1)), true);

	ffa_partition_info_wrong_test();
}

void ffa_version_test(void)
{
	struct ffa_value ret = ffa_version(MAKE_FFA_VERSION(FFA_MAJOR,
							    FFA_MINOR));

	spm_version = (uint32_t)ret.fid;

	bool ffa_version_compatible =
		((spm_version >> FFA_VERSION_MAJOR_SHIFT) == FFA_MAJOR &&
		 (spm_version & FFA_VERSION_MINOR_MASK) >= FFA_MINOR);

	INFO("Test FFA_VERSION. Return %u.%u; Compatible: %i\n",
		spm_version >> FFA_VERSION_MAJOR_SHIFT,
		spm_version & FFA_VERSION_MINOR_MASK,
		(int)ffa_version_compatible);

	expect((int)ffa_version_compatible, (int)true);
}

void ffa_spm_id_get_test(void)
{
	if (spm_version >= MAKE_FFA_VERSION(1, 1)) {
		struct ffa_value ret = ffa_spm_id_get();

		expect(ffa_func_id(ret), FFA_SUCCESS_SMC32);

		ffa_id_t spm_id = ffa_endpoint_id(ret);

		INFO("Test FFA_SPM_ID_GET. Return: 0x%x\n", spm_id);

		/*
		 * Check the SPMC value given in the fvp_spmc_manifest
		 * is returned.
		 */
		expect(spm_id, SPMC_ID);
	} else {
		INFO("FFA_SPM_ID_GET not supported in this version of FF-A."
			" Test skipped.\n");
	}
}

static const struct ffa_uuid scmi_uuid = {TERTIARY_UUID};
static const struct ffa_partition_info ffa_expected_scmi_info = 
	{
		.id = SP_ID(3),
		.exec_context = TERTIARY_EXEC_CTX_COUNT,
		.properties = (FFA_PARTITION_AARCH64_EXEC |
			       FFA_PARTITION_DIRECT_REQ_RECV |
			       FFA_PARTITION_DIRECT_REQ_SEND |
			       FFA_PARTITION_NOTIFICATION),
		.uuid = {TERTIARY_UUID}
	};

enum scmi_ffa_pta_cmd {
	/*
	 * FFA_SCMI_CMD_CAPABILITIES - Get channel capabilities
	 *
	 * [out]    data0: Cmd FFA_SCMI_CMD_CAPABILITIES
	 * [in]    data1: Capability bit mask
	 */
	FFA_SCMI_CMD_CAPABILITIES = 0,

	/*
	 * FFA_SCMI_CMD_GET_CHANNEL - Get channel handle
	 *
	 * [out]    data0: Cmd FFA_SCMI_CMD_GET_CHANNEL
	 * [out]    data1: Channel identifier
	 * [in]     data1: Returned channel handle
	 * [out]    data2: Shared memory handle (optional)
	 */
	FFA_SCMI_CMD_GET_CHANNEL = 1,

	/*
	 * FFA_SCMI_CMD_MSG_SEND_DIRECT_REQ - Process direct SCMI message
	 * with shared memory
	 *
	 * [out]    data0: Cmd FFA_SCMI_CMD_MSG_SEND_DIRECT_REQ
	 * [out]    data1: Channel handle
	 * [in/out] data2: Response size
	 *
	 */
	FFA_SCMI_CMD_MSG_SEND_DIRECT_REQ = 2,

	/*
	 * FFA_SCMI_CMD_SEND_MSG2 - Process SCMI message in RXTX buffer
	 *
	 * Use FFA RX/TX message to exchange request with the SCMI server.
	 */
	FFA_SCMI_CMD_MSG_SEND2 = 3,
};

#ifdef PLAT_XLAT_TABLES_DYNAMIC
/**
 * Each Cactus SP has a memory region dedicated to memory sharing tests
 * described in their partition manifest.
 * This function returns the expected base address depending on the
 * SP ID (should be the same as the manifest).
 */
static void *share_page(ffa_id_t cactus_sp_id)
{
	switch (cactus_sp_id) {
	case SP_ID(1):
		return (void *)CACTUS_SP1_MEM_SHARE_BASE;
	case SP_ID(2):
		return (void *)CACTUS_SP2_MEM_SHARE_BASE;
	case SP_ID(3):
		return (void *)CACTUS_SP3_MEM_SHARE_BASE;
	default:
		ERROR("Helper function expecting a valid Cactus SP ID!\n");
		panic();
	}
}

static void *share_page_non_secure(ffa_id_t cactus_sp_id)
{
	if (cactus_sp_id != SP_ID(3)) {
		ERROR("Helper function expecting a valid Cactus SP ID!\n");
		panic();
	}

	return (void *)CACTUS_SP3_NS_MEM_SHARE_BASE;
}

ffa_memory_handle_t ffa_scmi_share_memory(struct mailbox_buffers *mb, ffa_id_t source, ffa_id_t dest)
{
	struct ffa_value ffa_ret;
	uint32_t mem_func = FFA_MEM_SHARE_SMC32;
	ffa_memory_handle_t handle;
	bool non_secure = false;
	void *share_page_addr =
		non_secure ? share_page_non_secure(source) : share_page(source);
	unsigned int mem_attrs;
	int ret;

	VERBOSE("%x requested to send memory to %x (func: %x), page: %llx\n",
		source, dest, mem_func, (uint64_t)share_page_addr);

	const struct ffa_memory_region_constituent constituents[] = {
		{share_page_addr, 1, 0}
	};

	const uint32_t constituents_count = (sizeof(constituents) /
					     sizeof(constituents[0]));

	VERBOSE("Sharing at 0x%llx\n", (uint64_t)constituents[0].address);
	mem_attrs = MT_RW_DATA;

	if (non_secure) {
		mem_attrs |= MT_NS;
	}

	ret = mmap_add_dynamic_region(
		(uint64_t)constituents[0].address,
		(uint64_t)constituents[0].address,
		constituents[0].page_count * PAGE_SIZE,
		mem_attrs);

	if (ret != 0) {
		ERROR("Failed map share memory before sending (%d)!\n",
		      ret);
		return 0;
	}

	handle = memory_init_and_send(
		(struct ffa_memory_region *)mb->send, PAGE_SIZE,
		source, dest, constituents,
		constituents_count, mem_func, &ffa_ret);

	return handle;
}
#else
ffa_memory_handle_t ffa_scmi_share_memory(struct mailbox_buffers *mb, ffa_id_t source, ffa_id_t dest)
{
	return 0;
}

static void *share_page(ffa_id_t cactus_sp_id)
{
	return NULL;
}
#endif

struct scmi_msg_payld {
	uint32_t msg_header;
	uint32_t msg_payload[];
};

void ffa_scmi_server_test(struct mailbox_buffers *mb, ffa_id_t source_id)
{
	ffa_id_t dest_id;
	uint64_t cmd, val0, val1, val2, val3;
	struct ffa_value ret;
	ffa_memory_handle_t mem_id;
	uint32_t channel_id;
	struct scmi_msg_payld *share_page_addr;

	INFO("Test SCMI server partition\n");
	if (ffa_partition_info_helper(mb, scmi_uuid, &ffa_expected_scmi_info, 1))
		INFO("SCMI server partition is present\n");
	else
		return;

	dest_id = ffa_expected_scmi_info.id;

	cmd = FFA_SCMI_CMD_CAPABILITIES;
	val0 = val1 = val2 = val3 = 0;

	ret = ffa_msg_send_direct_req64(source_id, dest_id, cmd, val0, val1, val2, val3);
	if (!is_ffa_direct_response(ret)) {
		INFO("SCMI server fails\n");
		return;
	}

	INFO("SCMI server capabilities 0x%lx\n", ret.arg4);

#ifndef PLAT_XLAT_TABLES_DYNAMIC
	/* stop here because we can't dynamiccally map memory */
	return;
#endif

	mem_id = ffa_scmi_share_memory(mb, source_id, dest_id);

	cmd = FFA_SCMI_CMD_GET_CHANNEL;
	val0 = 0;
	val1 = mem_id;
	val2 = val3 = 0;

	ret = ffa_msg_send_direct_req64(source_id, dest_id, cmd, val0, val1, val2, val3);
	if (!is_ffa_direct_response(ret)) {
		INFO("SCMI server fails\n");
		return;
	}

	channel_id = ret.arg4;
	INFO("SCMI server channel id 0x%x\n", channel_id);

	if (channel_id == 0xffffffff) {
		ffa_mem_reclaim(mem_id, 0);
		return;
	}

	cmd = FFA_SCMI_CMD_MSG_SEND_DIRECT_REQ;
	val0 = channel_id;
	val1 = 16;
	val2 = val3 = 0;

	/* 1-1 PA-VA mapping */
	/* scmi header :
	 * token :       [27:18]
	 * protocol_id : [17:10]
	 * message_type: [9:8]
	 * message_id :  [7:0]
	 */
	share_page_addr = share_page(source_id);
	share_page_addr->msg_header = 0x10 << 10 | 0x3;

//	return;

	ret = ffa_msg_send_direct_req64(source_id, dest_id, cmd, val0, val1, val2, val3);
	if (!is_ffa_direct_response(ret)) {
		INFO("SCMI server fails\n");
		return;
	}

	INFO("SCMI server returned size 0x%lx\n", ret.arg5);

}

void ffa_tests(struct mailbox_buffers *mb, ffa_id_t my_ffa_id)
{
	const char *test_ffa = "FF-A setup and discovery";

	announce_test_section_start(test_ffa);

	ffa_features_test();
	ffa_version_test();
	ffa_spm_id_get_test();
	ffa_partition_info_get_test(mb);
	ffa_partition_info_get_regs_test();
	ffa_scmi_server_test(mb, my_ffa_id);

	announce_test_section_end(test_ffa);
}
