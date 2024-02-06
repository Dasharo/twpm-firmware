/* Microsoft Reference Implementation for TPM 2.0
 *
 *  The copyright in this software is being made available under the BSD License,
 *  included below. This software may be subject to other third party and
 *  contributor rights, including patent rights, and no such rights are granted
 *  under this license.
 *
 *  Copyright (c) Microsoft Corporation
 *
 *  All rights reserved.
 *
 *  BSD License
 *
 *  Redistribution and use in source and binary forms, with or without modification,
 *  are permitted provided that the following conditions are met:
 *
 *  Redistributions of source code must retain the above copyright notice, this list
 *  of conditions and the following disclaimer.
 *
 *  Redistributions in binary form must reproduce the above copyright notice, this
 *  list of conditions and the following disclaimer in the documentation and/or other
 *  materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ""AS IS""
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "TpmBuildSwitches.h"
#include "zephyr/kernel.h"
#include "zephyr/sys/util.h"
#include <twpm/platform.h>

#include <zephyr/kernel.h>
#if defined(CONFIG_HWINFO)
#include <zephyr/drivers/hwinfo.h>
#endif
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#if defined (CONFIG_TWPM_USE_PUF)
#include <zephyr/drivers/syscon.h>
#endif

#include <wolfssl/wolfcrypt/sha512.h>

LOG_MODULE_REGISTER(unique);

#if defined (CONFIG_TWPM_USE_PUF)
#define TWPM_PUF_N_WORDS 3 // 96 bits of unique data
#define TWPM_PUF_N_SAMPLES 128 // probably not enough, default was 4096, reduced to improve performance
#define TWPM_PUF_HYST_HI (TWPM_PUF_N_SAMPLES - (TWPM_PUF_N_SAMPLES / 32)) // min number of times bit x in all samples has to be set to be identified as '1'
#define TWPM_PUF_HYST_LO (TWPM_PUF_N_SAMPLES / 32) // max number of times bit x in all samples has to be clear to be identified as '0'

#define TWPM_PUF_CTRL 0x00
#define TPWM_PUF_CTRL_ENABLE (1 << 0)
#define TWPM_PUF_CTRL_TRIGGER (1 << 1)
#define TWPM_PUF_ID(word) (0x04 + word * 4)

void puf_sample(const struct device *dev, uint32_t *out) {
	uint32_t ctrl;

	syscon_write_reg(dev, TWPM_PUF_CTRL, TPWM_PUF_CTRL_ENABLE | TWPM_PUF_CTRL_TRIGGER);
	while (true) {
		syscon_read_reg(dev, TWPM_PUF_CTRL, &ctrl);
		// Auto clears when sampling is done
		if (ctrl & TWPM_PUF_CTRL_TRIGGER) {
			// Don't block Zephyr
			k_yield();
			continue;
		}

		syscon_write_reg(dev, TWPM_PUF_CTRL, 0);
		for (int j = 0; j < TWPM_PUF_N_WORDS; j++)
			syscon_read_reg(dev, TWPM_PUF_ID(j), &out[j]);

		break;
	}

	
}

void get_puf_id(const struct device *dev, uint32_t *puf_data) {
	uint32_t x, y;
	uint32_t puf_raw[TWPM_PUF_N_WORDS];
	uint32_t cnt[96];
	uint32_t tmp;
	uint32_t res[3];
	uint32_t val[3];

	for (x = 0; x < 96; x++) {
		cnt[x] = 0;
	}

	// count how often each bit of the 96-bit ID across all samples
	for (x = 0; x < TWPM_PUF_N_SAMPLES; x++) {
		LOG_DBG("PUF sampling ... (%d out of %d)", x, TWPM_PUF_N_SAMPLES);
		// get 96-bit raw ID sample
		puf_sample(dev, puf_raw);

		// test every single bit if set or cleared
		for (y = 0; y < 96; y++) {
			if (y >= 64) tmp = puf_raw[2];
			else if (y >= 32) tmp = puf_raw[1];
			else tmp = puf_raw[0];

			tmp >>= (y % 32);

			// increment "set-bits" counter
			if (tmp & 1)
				cnt[y]++;
		}
	}

	// construct final ID
	res[0] = 0; res[1] = 0; res[2] = 0;
	val[0] = 0; val[1] = 0; val[2] = 0;

	// compute each bit of the final ID independently
	for (x = 0; x < 96; x++) {
		// bit value - majority decision
		if (cnt[x] >= TWPM_PUF_N_SAMPLES / 2)
			tmp = 1; // bit is considered '1' if more than half of all samples had this bit set
		else
			tmp = 0; // bit is considered '0' if less than half of all samples had this bit cleared

		tmp <<= (x % 32);

		if (x >= 64) res[0] |= tmp;
		else if (x>=32) res[1] |= tmp;
		else res[2] |= tmp;

		// bit valid - hysteresis decision
		// bit is only valid if it is "set most of the times" or "cleared most of the times"
		if (cnt[x] >= TWPM_PUF_HYST_HI || cnt[x] <= TWPM_PUF_HYST_LO)
			tmp = 1; // bit valid
		else
			tmp = 0; // bit invalid
		tmp <<= (x % 32);

		if (x >= 64) val[0] |= tmp;
		else if (x >= 32) val[1] |= tmp;
		else val[2] |= tmp;
	}

	// final ID, unstable/noisy bits are masked out
	puf_data[0] = res[0] & val[0];
	puf_data[1] = res[1] & val[1];
	puf_data[2] = res[2] & val[2];
}

K_THREAD_STACK_DEFINE(puf_thread_stack, 16384);
struct k_thread puf_thread;
k_tid_t puf_thread_id;

static void puf_main(void *p0, void *p1, void *p2) {
	const size_t size = TWPM_PUF_N_WORDS * 4;
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(twpmpuf));

	union {
		uint8_t u8[TWPM_PUF_N_WORDS * 4];
		uint32_t u32[TWPM_PUF_N_WORDS];
	} id;

	LOG_DBG("sampling PUF ...");
	// This is going to take a long time ... flush pending messages
	while (log_process()) {}
	get_puf_id(dev, id.u32);
	for (int i = 0; i < ARRAY_SIZE(id.u32); i++)
		LOG_DBG("PUF ID[%d]: 0x%08x", i, id.u32[i]);
}
#endif

static char tpm_unique[WC_SHA512_DIGEST_SIZE] = {0};

/**
 * This function initializes TPM unique value which is unique to each device.
 * 
 */
void twpm_init_unique() {
#if defined(CONFIG_TWPM_USE_HWINFO)
	union {
		char u8[32];
	} id;
	// Nucleo-TPM used more data (like flash size, device model, revision, etc.)
	// to generate unique. Here we take only device id/serial because of
	// Zephyr API limitations and minimal benefit. We could also program
	// unique value during TPM manufacturing to avoid potential problems
	// with hardware IDs (like non-unique IDs).
	ssize_t size = hwinfo_get_device_id(id.u8, sizeof(id.u8));
	if (size < 0) {
		LOG_ERR("Could not obtain hardware device ID: %d\n", size);
		return;
	}

	wc_Sha512 hasher;
	wc_InitSha512(&hasher);
	wc_Sha512Update(&hasher, id.u8, size);
	wc_Sha512Final(&hasher, tpm_unique);
#elif defined(CONFIG_TWPM_USE_PUF)
	// Spawn in separate thread, doing it here causes zephyr to hang.
	// When spawned in separate thread computing ID using 128 samples takes
	// about 10 minutes.
	k_thread_create(
		&puf_thread, puf_thread_stack,
		K_THREAD_STACK_SIZEOF(puf_thread_stack),
		puf_main, NULL, NULL, NULL, 7, 0, K_NO_WAIT);
#endif
}

/**
 *
 * This function places the unique value in the provided buffer ('b')
 * and returns the number of bytes transferred. The function will not
 * copy more data than 'bSize'.
 * NOTE: If a platform unique value has unequal distribution of uniqueness
 * and 'bSize' is smaller than the size of the unique value, the 'bSize'
 * portion with the most uniqueness should be returned.
 */
uint32_t _plat__GetUnique(uint32_t which, uint32_t bSize, unsigned char *b)
{
	const char *from = tpm_unique;
	uint32_t retVal = 0;

	if(which == 0) // the authorities value
	{
		for(retVal = 0; retVal < bSize; retVal++)
		{
			*b++ = *from++;
		}
	}
	else
	{
#define uSize  sizeof(tpm_unique)
		b = &b[((bSize < uSize) ? bSize : uSize) - 1];
		for(retVal = 0; retVal < bSize; retVal++)
		{
			*b-- = *from++;
		}
	}
	return retVal;
}
