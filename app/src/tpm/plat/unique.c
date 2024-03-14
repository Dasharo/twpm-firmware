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

#include <twpm/platform.h>

#if defined(CONFIG_HWINFO)
#include <zephyr/drivers/hwinfo.h>
#endif
#include <zephyr/logging/log.h>

#include <wolfssl/wolfcrypt/sha512.h>

LOG_MODULE_REGISTER(unique);

static char tpm_unique[WC_SHA512_DIGEST_SIZE] = {0};

/**
 * This function initializes TPM unique value which is unique to each device.
 * 
 */
void twpm_init_unique() {
#if defined(CONFIG_TWPM_USE_HWINFO)
	char id[32];
	// Nucleo-TPM used more data (like flash size, device model, revision, etc.)
	// to generate unique. Here we take only device id/serial because of
	// Zephyr API limitations and minimal benefit. We could also program
	// unique value during TPM manufacturing to avoid potential problems
	// with hardware IDs (like non-unique IDs).
	ssize_t size = hwinfo_get_device_id(id, sizeof id);
	if (size < 0) {
		LOG_ERR("Could not obtain hardware device ID: %d\n", size);
		return;
	}

	wc_Sha512 hasher;
	wc_InitSha512(&hasher);
	wc_Sha512Update(&hasher, id, size);
	wc_Sha512Final(&hasher, tpm_unique);
#elif defined(CONFIG_TWPM_CONST_UNIQUE)
	LOG_WRN("TwPM was built with CONFIG_TWPM_CONST_UNIQUE, the implementation is not secure!");

#define TO_STRING(x) #x
	const char *const string = TO_STRING(CONFIG_TWPM_CONST_UNIQUE_VALUE);

	wc_Sha512 hasher;
	wc_InitSha512(&hasher);
	wc_Sha512Update(&hasher, string, strlen(string));
	wc_Sha512Final(&hasher, tpm_unique);
#else
#error "Must select a method to obtain unique value"
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
