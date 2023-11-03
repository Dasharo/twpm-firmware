/*
 * Copyright (c) 2022 3mdeb
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
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
K_SEM_DEFINE(tpm_resp_sem, 0, 1);

uint8_t *tpm_cmd = NULL;
uint32_t tpm_cmd_size = 0;
uint8_t *tpm_response = NULL;
uint32_t tpm_response_size = 0;

void execute_tpm_command(uint8_t *cmd, size_t size) {
	tpm_cmd = cmd;
	tpm_cmd_size = size;
	k_sem_reset(&tpm_resp_sem);
	k_sem_give(&tpm_cmd_sem);
}

static void tpm_thread_entry(void *p0, void *p1, void *p2) {
	while(1) {
		if (k_sem_take(&tpm_cmd_sem, K_MSEC(1000)) == 0) {
			LOG_INF("Executing %s", tpmdbg_decode_cc(&tpm_cmd[6]));
			twpm_run_command(tpm_cmd_size, tpm_cmd, &tpm_response_size, &tpm_response);
			k_sem_give(&tpm_resp_sem);
			LOG_INF("TPM command result: %s", tpmdbg_decode_rc(&tpm_response[6]));
		} else {
			// Update internal state of 64-bit emulated clock (STM32
			// provides only 32-bit clock). We need to this
			// periodically to detect clock overflows.
			_plat__TimerRead();
		}
	}
}

void main(void)
{
	LOG_INF("Starting TwPM on %s", CONFIG_BOARD);

	twpm_init();
	twpm_selftest();

	tpm_thread_id = k_thread_create(&tpm_thread, tpm_thread_stack,
					K_THREAD_STACK_SIZEOF(tpm_thread_stack),
					tpm_thread_entry, NULL, NULL, NULL, 5, 0,
					K_NO_WAIT);
}
