/*
 * Copyright (c) 2018-2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <drivers/arm/pl011.h>
#include <drivers/console.h>
#include <lib/aarch64/arch_helpers.h>
#include <lib/tftf_lib.h>
#include <lib/xlat_tables/xlat_mmu_helpers.h>
#include <lib/xlat_tables/xlat_tables_v2.h>

#include <ffa_helpers.h>
#include <plat_arm.h>
#include <platform.h>
#include <platform_def.h>
#include <sp_debug.h>
#include <sp_helpers.h>
#include <spm_helpers.h>
#include <std_svc.h>

#include "sp_def.h"
#include "cactus.h"

#include <cactus_test_cmds.h>
#include <events.h>
#include <platform.h>

//scmi entry point
#include <arch_main.h>

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

#define FFA_SCMI_CAPS_SHARED_BUFFER	(0x1 << 1)

extern void secondary_cold_entry(void);

/* Global ffa_id */
ffa_id_t g_ffa_id;

/* Global FFA_MSG_DIRECT_REQ source ID */
ffa_id_t g_dir_req_source_id;

#define PRINT_CMD(smc_ret)						\
	VERBOSE("cmd %lx; args: %lx, %lx, %lx, %lx\n",	 		\
		smc_ret.arg3, smc_ret.arg4, smc_ret.arg5, 		\
		smc_ret.arg6, smc_ret.arg7)

void __attribute__((__noreturn__)) do_panic(const char *file, int line)
{
	printf("PANIC in file: %s line: %d\n", file, line);

	console_flush();

	while (1)
		continue;
}

void __attribute__((__noreturn__)) do_bug_unreachable(const char *file, int line)
{
	mp_printf("BUG: Unreachable code!\n");
	do_panic(file, line);
}

/*
 * Retrieve shared memory.
 */
static void *scmi_memory_retrieve(ffa_id_t source, ffa_id_t vm_id, uint64_t handle, struct mailbox_buffers *mb)
{
	struct ffa_memory_region *m;
	struct ffa_composite_memory_region *composite;
	int ret;
	unsigned int mem_attrs;
	void *ptr;
	ffa_memory_region_flags_t retrv_flags = 0;
	bool non_secure = true;

	if (!memory_retrieve(mb, &m, handle, source, vm_id, retrv_flags)){
		ERROR("Failed to received memory region!\n");
		return 0;
	}

	composite = ffa_memory_region_get_composite(m, 0);

	/* This test is only concerned with RW permissions. */
	if (ffa_get_data_access_attr(
			m->receivers[0].receiver_permissions.permissions) !=
		FFA_DATA_ACCESS_RW) {
		ERROR("Permissions not expected!\n");
		return 0;
	}

	mem_attrs = MT_RW_DATA | MT_EXECUTE_NEVER;

	if (non_secure) {
		mem_attrs |= MT_NS;
	}

	ret = mmap_add_dynamic_region(
			(uint64_t)composite->constituents[0].address,
			(uint64_t)composite->constituents[0].address,
			composite->constituents[0].page_count * PAGE_SIZE,
			mem_attrs);

	if (ret != 0) {
		ERROR("Failed to map received memory region(%d)!\n", ret);
		return 0;
	}

	ptr = (void *) composite->constituents[0].address;

	if (ffa_func_id(ffa_rx_release()) != FFA_SUCCESS_SMC32) {
		ERROR("Failed to release buffer!\n");
		return 0;
	}

	return ptr;
}

/**
 * Traverses command table from section ".cactus_handler", searches for a
 * registered command and invokes the respective handler.
 */
bool cactus_handle_cmd(struct ffa_value *cmd_args, struct ffa_value *ret,
		       struct mailbox_buffers *mb)
{
	uint64_t in_cmd;

	if (cmd_args == NULL || ret == NULL) {
		ERROR("Invalid arguments passed to %s!\n", __func__);
		return false;
	}

	/* Get the source of the Direct Request message. */
	if (ffa_func_id(*cmd_args) == FFA_MSG_SEND_DIRECT_REQ_SMC32 ||
	    ffa_func_id(*cmd_args) == FFA_MSG_SEND_DIRECT_REQ_SMC64) {
		g_dir_req_source_id = ffa_dir_msg_source(*cmd_args);
	}

	PRINT_CMD((*cmd_args));

	in_cmd = cactus_get_cmd(*cmd_args);

	switch (in_cmd) {
	case FFA_SCMI_CMD_CAPABILITIES:
		*ret = cactus_send_response32(ffa_dir_msg_dest(*cmd_args),
				       ffa_dir_msg_source(*cmd_args),
				       0, // success
				       FFA_SCMI_CAPS_SHARED_BUFFER, // direct req mode
				       0, 0, 0);
		break;

	case FFA_SCMI_CMD_GET_CHANNEL:
		{
		ffa_id_t source = ffa_dir_msg_source(*cmd_args);
		ffa_id_t vm_id = ffa_dir_msg_dest(*cmd_args);
		uint64_t channel = cmd_args->arg4;
		uint64_t handle = cmd_args->arg5;
		void *buffer;

		NOTICE("scmi_get_channel VM id: %x chnl: %llx mem hdl: %llx\n",
				vm_id, channel, handle);

		buffer = scmi_memory_retrieve(source, vm_id, handle, mb);
		NOTICE("scmi_get_channel VM id: %x ptr: %p\n",
				vm_id, buffer);

		channel = scmi_get_device(channel, vm_id, buffer);

		/* if channel == -1, then release the memory */

		*ret = cactus_send_response32(ffa_dir_msg_dest(*cmd_args),
				    ffa_dir_msg_source(*cmd_args),
			       0, // success
			       channel,
			       0, 0, 0);
		}
		break;

	case FFA_SCMI_CMD_MSG_SEND_DIRECT_REQ:
		{
		uint32_t channel = (uint32_t)cmd_args->arg4;
		size_t msg_size = cmd_args->arg5;
		ffa_id_t vm_id = ffa_dir_msg_dest(*cmd_args);

		NOTICE("scmi_process_msg VM id: %x chnl: %x size: %lu\n",
		      vm_id, channel, msg_size);

		scmi_process_mbx_msg(channel, vm_id, &msg_size);

		*ret = cactus_send_response32(ffa_dir_msg_dest(*cmd_args),
				      ffa_dir_msg_source(*cmd_args),
				      0, // success
				      channel,
				      msg_size,
				      0, 0);
		}
		break;

	default:
		*ret = cactus_error_resp(ffa_dir_msg_dest(*cmd_args),
				 ffa_dir_msg_source(*cmd_args),
				 CACTUS_ERROR_UNHANDLED);
	}

	return true;
}

/*
 *
 * Message loop function
 * Notice we cannot use regular print functions because this serves to both
 * "primary" and "secondary" VMs. Secondary VM cannot access UART directly
 * but rather through Hafnium print hypercall.
 *
 */

static void __dead2 message_loop(ffa_id_t vm_id, struct mailbox_buffers *mb)
{
	struct ffa_value ffa_ret;
	ffa_id_t destination;

	/*
	* This initial wait call is necessary to inform SPMD that
	* SP initialization has completed. It blocks until receiving
	* a direct message request.
	*/

	ffa_ret = ffa_msg_wait();

	for (;;) {
		VERBOSE("Woke up with func id: %x\n", ffa_func_id(ffa_ret));

		if (ffa_func_id(ffa_ret) == FFA_ERROR) {
			ERROR("Error: %x\n", ffa_error_code(ffa_ret));
			break;
		}

		if (ffa_func_id(ffa_ret) != FFA_MSG_SEND_DIRECT_REQ_SMC32 &&
		    ffa_func_id(ffa_ret) != FFA_MSG_SEND_DIRECT_REQ_SMC64 &&
		    ffa_func_id(ffa_ret) != FFA_INTERRUPT &&
		    ffa_func_id(ffa_ret) != FFA_RUN) {
			ERROR("%s(%u) unknown func id 0x%x\n",
				__func__, vm_id, ffa_func_id(ffa_ret));
			break;
		}

		if ((ffa_func_id(ffa_ret) == FFA_INTERRUPT) ||
		    (ffa_func_id(ffa_ret) == FFA_RUN)) {
			/*
			 * Received FFA_INTERRUPT in waiting state.
			 * The interrupt id is passed although this is just
			 * informational as we're running with virtual
			 * interrupts unmasked and the interrupt is processed
			 * by the interrupt handler.
			 *
			 * Received FFA_RUN in waiting state, the endpoint
			 * simply returns by FFA_MSG_WAIT.
			 */
			ffa_ret = ffa_msg_wait();
			continue;
		}

		destination = ffa_dir_msg_dest(ffa_ret);
		if (destination != vm_id) {
			ERROR("%s(%u) invalid vm id 0x%x\n",
				__func__, vm_id, destination);
			break;
		}

		if (!cactus_handle_cmd(&ffa_ret, &ffa_ret, mb)) {
			break;
		}
	}

	panic();
}

static const mmap_region_t cactus_mmap[] __attribute__((used)) = {
	/* PLAT_ARM_DEVICE0 area includes UART2 necessary to console */
	MAP_REGION_FLAT(PLAT_ARM_DEVICE0_BASE, PLAT_ARM_DEVICE0_SIZE,
			MT_DEVICE | MT_RW),
	{0}
};

static void cactus_print_memory_layout(unsigned int vm_id)
{
	INFO("Secure Partition memory layout:\n");

	INFO("  Text region            : %p - %p\n",
		(void *)CACTUS_TEXT_START, (void *)CACTUS_TEXT_END);

	INFO("  Read-only data region  : %p - %p\n",
		(void *)CACTUS_RODATA_START, (void *)CACTUS_RODATA_END);

	INFO("  Data region            : %p - %p\n",
		(void *)CACTUS_DATA_START, (void *)CACTUS_DATA_END);

	INFO("  BSS region             : %p - %p\n",
		(void *)CACTUS_BSS_START, (void *)CACTUS_BSS_END);

	INFO("  RX                     : %p - %p\n",
		(void *)get_sp_rx_start(vm_id),
		(void *)get_sp_rx_end(vm_id));

	INFO("  TX                     : %p - %p\n",
		(void *)get_sp_tx_start(vm_id),
		(void *)get_sp_tx_end(vm_id));
}

static void cactus_plat_configure_mmu(unsigned int vm_id)
{
	mmap_add_region(CACTUS_TEXT_START,
			CACTUS_TEXT_START,
			CACTUS_TEXT_END - CACTUS_TEXT_START,
			MT_CODE);
	mmap_add_region(CACTUS_RODATA_START,
			CACTUS_RODATA_START,
			CACTUS_RODATA_END - CACTUS_RODATA_START,
			MT_RO_DATA);
	mmap_add_region(CACTUS_DATA_START,
			CACTUS_DATA_START,
			CACTUS_DATA_END - CACTUS_DATA_START,
			MT_RW_DATA);
	mmap_add_region(CACTUS_BSS_START,
			CACTUS_BSS_START,
			CACTUS_BSS_END - CACTUS_BSS_START,
			MT_RW_DATA);

	mmap_add_region(get_sp_rx_start(vm_id),
			get_sp_rx_start(vm_id),
			(SP_RX_TX_SIZE / 2),
			MT_RO_DATA);

	mmap_add_region(get_sp_tx_start(vm_id),
			get_sp_tx_start(vm_id),
			(SP_RX_TX_SIZE / 2),
			MT_RW_DATA);

	mmap_add(cactus_mmap);
	init_xlat_tables();
}

static void register_secondary_entrypoint(void)
{
	smc_args args;

	args.fid = FFA_SECONDARY_EP_REGISTER_SMC64;
	args.arg1 = (u_register_t)&secondary_cold_entry;

	tftf_smc(&args);
}

void __dead2 cactus_main(bool primary_cold_boot,
			 struct ffa_boot_info_header *boot_info_header)
{
	assert(IS_IN_EL1() != 0);

	struct mailbox_buffers mb;
	struct ffa_value ret;

	/* Get current FFA id */
	struct ffa_value ffa_id_ret = ffa_id_get();
	ffa_id_t ffa_id = ffa_endpoint_id(ffa_id_ret);
	if (ffa_func_id(ffa_id_ret) != FFA_SUCCESS_SMC32) {
		ERROR("FFA_ID_GET failed.\n");
		panic();
	}

	if (primary_cold_boot == true) {
		/* Clear BSS */
		memset((void *)CACTUS_BSS_START,
		       0, CACTUS_BSS_END - CACTUS_BSS_START);

		/* Configure and enable Stage-1 MMU, enable D-Cache */
		cactus_plat_configure_mmu(ffa_id);

		/* Initialize locks for tail end interrupt handler */
		sp_handler_spin_lock_init();

		if (boot_info_header != NULL) {
			/*
			 * TODO: Currently just validating that cactus can
			 * access the boot info descriptors. In case we want to
			 * use the boot info contents, we should check the
			 * blob and remap if the size is bigger than one page.
			 * Only then access the contents.
			 */
			mmap_add_dynamic_region(
				(unsigned long long)boot_info_header,
				(uintptr_t)boot_info_header,
				PAGE_SIZE, MT_RO_DATA);
		}
	}
	mb.send = (void *) get_sp_tx_start(ffa_id);
	mb.recv = (void *) get_sp_rx_start(ffa_id);

	/*
	 * The local ffa_id value is held on the stack. The global g_ffa_id
	 * value is set after BSS is cleared.
	 */
	g_ffa_id = ffa_id;

	enable_mmu_el1(0);

	/* Enable IRQ/FIQ */
	enable_irq();
//	enable_fiq();

	if (primary_cold_boot == false) {
		goto msg_loop;
	}

	if (true) {
		console_init(CACTUS_PL011_UART_BASE,
			     CACTUS_PL011_UART_CLK_IN_HZ,
			     PL011_BAUDRATE);

		set_putc_impl(PL011_AS_STDOUT);

	} else {
		set_putc_impl(FFA_SVC_SMC_CALL_AS_STDOUT);
	}

	NOTICE("Booting Secure Partition (ID: %x)\n",
		ffa_id);

	CONFIGURE_AND_MAP_MAILBOX(mb, PAGE_SIZE, ret);
	if (ffa_func_id(ret) != FFA_SUCCESS_SMC32) {
		ERROR(
		    "Failed to map RXTX buffers. Error: %x\n",
		    ffa_error_code(ret));
		panic();
	}

	cactus_print_memory_layout(ffa_id);

	register_secondary_entrypoint();
	discover_managed_exit_interrupt_id();
	register_maintenance_interrupt_handlers();

	/* Invoking SCMI server */
	VERBOSE("SCMI server init start\n");
	scmi_arch_init();
	VERBOSE("SCMI server init end\n");

msg_loop:
	/* End up to message loop */
	message_loop(ffa_id, &mb);

	/* Not reached */
}
