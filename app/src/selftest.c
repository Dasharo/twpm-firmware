#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include <twpm/tpm.h>
#include <twpm/test.h>

#include <stdio.h>

#define SHA256_DIGEST_SIZE 32

#define TPM_TAG_RSP_COMMAND 0x00c4
#define TPM_ST_NO_SESSIONS 0x8001

LOG_MODULE_REGISTER(test);

typedef struct {
	uint16_t tag;
	uint32_t size;
	uint32_t command;
} tpm_command_header;

static uint8_t cmd_tpm_startup[] = {0x80, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x01, 0x44, 0x00, 0x00};
// static uint8_t cmd_tpm_cap_get_algos[] = {8001000000160000017a00000000000000010000007f}

// TPM response on error:
// u16 - TPM_TAG_RSP_COMMAND or TPM_ST_NO_SESSIONS
// u32 - response size, always 10
// u32 - response code, one of TPM2_RC_*
static uint8_t* execute_command(uint8_t *request, size_t size, bool dump, uint32_t *out_resp_size) {
	static uint8_t resp_buffer[512];
	uint8_t *resp = resp_buffer;
	uint32_t resp_size = sizeof resp_buffer;
	twpm_run_command(size, request, &resp_size, &resp);
	if (out_resp_size) {
		*out_resp_size = resp_size;
	}

	if (dump) {
		LOG_INF("TPM command result: %s", tpmdbg_decode_rc(&resp[6]));
	}

	return resp;
}

static void tpm_hash(uint8_t *buffer, uint8_t size) {
	uint16_t tag = sys_cpu_to_be16(TPM_ST_NO_SESSIONS);
	uint32_t total_size_cpu = size + 12 + 6;
	uint32_t total_size = sys_cpu_to_be32(total_size_cpu);
	uint32_t command = sys_cpu_to_be32(0x0000017d); /* TPM2_CC_Hash */

	uint8_t *request = k_malloc(total_size_cpu);
	memcpy(&request[0], &tag, sizeof tag);
	memcpy(&request[2], &total_size, sizeof total_size);
	memcpy(&request[6], &command, sizeof command);

	*(uint16_t*)&request[10] = sys_cpu_to_be16(size);
	memcpy(&request[12], buffer, size);
	request[12 + size + 0] = 0x00;
	request[12 + size + 1] = 0x0b; /* sha256 */
	request[12 + size + 2] = 0x40;
	request[12 + size + 3] = 0x00;
	request[12 + size + 4] = 0x00;
	request[12 + size + 5] = 0x01;

	k_free(request);
	uint32_t resp_size;
	uint8_t *resp = execute_command(request, total_size_cpu, false, &resp_size);
	if (resp_size < 10) {
		LOG_ERR("TPM response too short");
		return;
	}

	if (sys_get_be32(&resp[6]) == 0) {
		uint32_t real_resp_size = sys_get_be32(&resp[2]);
		if (real_resp_size - 12 < SHA256_DIGEST_SIZE) {
			LOG_ERR("TPM response too short");
			return;
		}
		uint16_t hash_size = sys_get_be16(&resp[10]);

		if (hash_size != SHA256_DIGEST_SIZE) {
			LOG_ERR("HASH: invalid response size %d", real_resp_size);
			LOG_HEXDUMP_ERR(resp, resp_size, "Response:");
		} else {
			char buf[SHA256_DIGEST_SIZE * 2 + 1];
			for (int i = 0; i < SHA256_DIGEST_SIZE; i++) {
				uint8_t b = resp[12 + i];
				snprintf(&buf[i * 2], 3, "%02x", b);
			}
			buf[sizeof buf - 1] = 0;
			LOG_INF("HASH: %s", buf);
		}
	} else {
		LOG_ERR("HASH failed: %s", tpmdbg_decode_rc(resp));
	}
}

void twpm_selftest() {
	execute_command(cmd_tpm_startup, sizeof cmd_tpm_startup, true, NULL);
	tpm_hash("test12345678901234567890987654321\n", 34);
}
