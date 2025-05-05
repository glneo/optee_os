// SPDX-License-Identifier: BSD-2-Clause
/*
 * Texas Instruments System Control Interface Driver
 *
 * Copyright (C) 2023 Texas Instruments Incorporated - https://www.ti.com/
 *	Manorit Chawdhry <m-chawdhry@ti.com>
 */

#include <drivers/ti_sci.h>
#include <inttypes.h>
#include <k3/otp_keywriting_ta.h>
#include <kernel/pseudo_ta.h>
#include <mm/core_memprot.h>

static TEE_Result write_otp_row(uint32_t param_types, TEE_Param params[4])
{
	TEE_Result ret = TEE_SUCCESS;
	const uint32_t exp_param_types =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT,
				TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE);

	/*
	 * Safely get the invocation parameters
	 */
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	ret = ti_sci_write_otp_row(params[0].value.a, params[1].value.a,
				   params[1].value.b);
	if (ret)
		return ret;

	DMSG("Written the value: 0x%08"PRIx32, params[1].value.a);

	return TEE_SUCCESS;
}

static TEE_Result read_otp_mmr(uint32_t param_types, TEE_Param params[4])
{
	TEE_Result ret = TEE_SUCCESS;
	const uint32_t exp_param_types =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT,
				TEE_PARAM_TYPE_VALUE_OUTPUT,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);

	/*
	 * Safely get the invocation parameters
	 */
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	ret = ti_sci_read_otp_mmr(params[0].value.a, &params[1].value.a);
	if (ret)
		return ret;

	DMSG("Got the value: 0x%08"PRIx32, params[1].value.a);

	return TEE_SUCCESS;
}

static TEE_Result lock_otp_row(uint32_t param_types, TEE_Param params[4])
{
	TEE_Result ret = TEE_SUCCESS;
	int hw_write_lock = 0;
	int hw_read_lock = 0;
	int soft_lock = 0;
	const uint32_t exp_param_types =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);

	/*
	 * Safely get the invocation parameters
	 */
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	if (params[0].value.b & K3_OTP_KEYWRITING_SOFT_LOCK)
		soft_lock = 0x5A;
	if (params[0].value.b & K3_OTP_KEYWRITING_HW_READ_LOCK)
		hw_read_lock = 0x5A;
	if (params[0].value.b & K3_OTP_KEYWRITING_HW_WRITE_LOCK)
		hw_write_lock = 0x5A;

	DMSG("hw_write_lock: 0x%#x", hw_write_lock);
	DMSG("hw_read_lock: 0x%#x", hw_read_lock);
	DMSG("soft_lock: 0x%#x", soft_lock);

	ret = ti_sci_lock_otp_row(params[0].value.a, hw_write_lock,
				  hw_read_lock, soft_lock);

	if (ret)
		return ret;

	DMSG("Locked the row: 0x%08"PRIx32, params[1].value.a);

	return TEE_SUCCESS;
}

static TEE_Result set_keyrev(uint32_t param_types, TEE_Param params[4])
{
	uint32_t keyrev = 0;
	void *dual_cert_ptr = NULL;
	paddr_t pa = 0;
	uint32_t cert_addr_hi = 0;
	uint32_t cert_addr_lo = 0;
	TEE_Result ret = TEE_SUCCESS;

	/* Check invocation parameters */
	if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT,
					   TEE_PARAM_TYPE_MEMREF_INPUT,
					   TEE_PARAM_TYPE_NONE,
					   TEE_PARAM_TYPE_NONE))
		return TEE_ERROR_BAD_PARAMETERS;

	keyrev = params[0].value.a;
	dual_cert_ptr = params[1].memref.buffer;

	pa = virt_to_phys(dual_cert_ptr);
	if (!pa)
		return TEE_ERROR_BAD_PARAMETERS;
	reg_pair_from_64(pa, &cert_addr_hi, &cert_addr_lo);

	DMSG("Setting Key Revision: %"PRIu32, keyrev);

	DMSG("dual_cert_ptr: %p", dual_cert_ptr);
	DMSG("cert_addr_hi: %"PRIu32, cert_addr_hi);
	DMSG("cert_addr_lo: %"PRIu32, cert_addr_lo);
	DMSG("dual_cert_size: %zu", params[1].memref.size);

	ret = ti_sci_set_keyrev(keyrev, cert_addr_hi, cert_addr_lo);
	if (ret)
		return ret;

	DMSG("Set Key Revision: %"PRIu32, keyrev);

	return TEE_SUCCESS;
}

static TEE_Result invoke_command(void *session __unused,
				 uint32_t command, uint32_t param_types,
				 TEE_Param params[4])
{
	switch (command) {
	case TA_OTP_KEYWRITING_CMD_READ_MMR:
		return read_otp_mmr(param_types, params);
	case TA_OTP_KEYWRITING_CMD_WRITE_ROW:
		return write_otp_row(param_types, params);
	case TA_OTP_KEYWRITING_CMD_LOCK_ROW:
		return lock_otp_row(param_types, params);
	case TA_OTP_KEYWRITING_CMD_WRITE_KEYREV:
		return set_keyrev(param_types, params);
	default:
		EMSG("Command ID 0x%"PRIx32" is not supported", command);
		return TEE_ERROR_NOT_SUPPORTED;
	}
}

pseudo_ta_register(.uuid = PTA_K3_OTP_KEYWRITING_UUID,
		   .name = PTA_K3_OTP_KEYWRITING_NAME,
		   .flags = PTA_DEFAULT_FLAGS,
		   .invoke_command_entry_point = invoke_command);
