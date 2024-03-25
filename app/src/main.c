/*
 * Copyright (c) 2022 3mdeb
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/syscon.h>
#include <zephyr/logging/log.h>

#include <twpm/tpm.h>
#include <twpm/test.h>
#include <twpm/platform.h>

#include <string.h>
#include <limits.h>

#include <assert.h>

LOG_MODULE_REGISTER(main);

K_THREAD_STACK_DEFINE(tpm_thread_stack, 16384);
struct k_thread tpm_thread;
k_tid_t tpm_thread_id;
K_SEM_DEFINE(tpm_cmd_sem, 0, 1);

#ifdef CONFIG_TWPM_RNG_TEST
K_THREAD_STACK_DEFINE(rng_thread_stack, 512);
struct k_thread rng_thread;
#endif

static uint8_t tpm_cmd[2048];
static uint32_t tpm_cmd_size = 0;

extern unsigned char _g_locality;

#define TWPM_RAM_BASE       ((void *)DT_REG_ADDR(DT_NODELABEL(twpmram)))

static void tpm_thread_entry(void *p0, void *p1, void *p2) {
	while(1) {
		if (k_sem_take(&tpm_cmd_sem, K_MSEC(1000)) == 0) {
			uint8_t tpm_rsp_buf[2048];
			uint8_t *tpm_response = tpm_rsp_buf;
			uint32_t tpm_response_size = sizeof tpm_rsp_buf;
			const struct device *const dev = DEVICE_DT_GET(DT_NODELABEL(twpm));

			LOG_HEXDUMP_DBG(tpm_cmd, tpm_cmd_size, "TPM command:");
			LOG_INF("Executing %s", tpmdbg_decode_cc(&tpm_cmd[6]));
			twpm_run_command(tpm_cmd_size, tpm_cmd, &tpm_response_size, &tpm_response);

			if (tpm_response_size > 2048) {
				LOG_ERR("Illegal response size %x", tpm_response_size);
				k_oops();
			}

			LOG_HEXDUMP_DBG(tpm_response, tpm_response_size, "TPM response:");
			LOG_INF("TPM command result: %s", tpmdbg_decode_rc(&tpm_response[6]));

			memcpy(TWPM_RAM_BASE, tpm_response, tpm_response_size);
			syscon_write_reg(dev, TWPM_REG_COMPLETE, 1);

			irq_enable(TWPM_IRQ);
		} else {
			// Update internal state of 64-bit emulated clock (STM32
			// provides only 32-bit clock). We need to do this
			// periodically to detect clock overflows.
			_plat__TimerRead();
		}
	}
}

static void twpm_isr(const struct device *const dev)
{
	uint32_t op_type = 0;
	uint32_t locality = 0;

	irq_disable(TWPM_IRQ);

	syscon_read_reg(dev, TWPM_REG_OP_TYPE, &op_type);
	syscon_read_reg(dev, TWPM_REG_BUF_SIZE, &tpm_cmd_size);
	syscon_read_reg(dev, TWPM_REG_LOCALITY, &locality);

	LOG_INF("IRQ: op_type = %x, cmd_size = %x, locality = %x",
	        op_type, tpm_cmd_size, locality);

	if (op_type == TWPM_REG_OP_TYPE_CMD) {
		if (tpm_cmd_size > 2048) {
			LOG_ERR("Illegal command size %x", tpm_cmd_size);
			k_oops();
		}
		if (locality > 4) {
			LOG_ERR("Illegal locality %x", locality);
			k_oops();
		}
		_g_locality = locality & 0x0F;

		memcpy(tpm_cmd, TWPM_RAM_BASE, tpm_cmd_size);
		k_sem_give(&tpm_cmd_sem);
	} else {
		LOG_ERR("Unknown operation requested, giving up");
		k_oops();
	}
}

#ifdef CONFIG_TWPM_RNG_TEST
extern void twpm_test_rng();
#endif

int main(void)
{
	LOG_INF("Starting TwPM");

	IRQ_CONNECT(TWPM_IRQ, 0, twpm_isr, DEVICE_DT_GET(DT_NODELABEL(twpm)), 0);
	irq_enable(TWPM_IRQ);

	twpm_init();
	//twpm_selftest();

	tpm_thread_id = k_thread_create(&tpm_thread, tpm_thread_stack,
					K_THREAD_STACK_SIZEOF(tpm_thread_stack),
					tpm_thread_entry, NULL, NULL, NULL, 5, 0,
					K_NO_WAIT);

#ifdef CONFIG_TWPM_RNG_TEST
	k_thread_create(&rng_thread, rng_thread_stack,
			K_THREAD_STACK_SIZEOF(rng_thread_stack), twpm_test_rng,
			NULL, NULL, NULL, 5, 0, K_NO_WAIT);
#endif

	return 0;
}
