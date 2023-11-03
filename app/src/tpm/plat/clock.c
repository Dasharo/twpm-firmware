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

#include <assert.h>
#include <stdint.h>

static bool clock_was_stopped = true;
static bool clock_was_reset = true;
static uint32_t clock32_previous = 0;
static uint64_t clock64 = 0;

/**
 * This function provides access to the tick timer of the platform. The TPM code
 * uses this value to drive the TPM Clock.
 * 
 * Platform clock starts at boot and is never stopped.
 */
uint64_t _plat__TimerRead(void)
{
	uint32_t diff_abs;
	bool overflow = false;

	uint32_t clock = k_cycle_get_32() * 8;
	if (clock >= clock32_previous)
		diff_abs = clock - clock32_previous;
	else {
		diff_abs = 0xffffffff - clock32_previous + clock + 1;
		overflow = true;
	}

	clock32_previous = clock;

	uint64_t clock64_prev = clock64;
	clock64 += diff_abs;

	// On 64-bit counter overflow we need to signal TPM.
	if (clock64 < clock64_prev)
		clock_was_reset = true;

	return k_cyc_to_ms_floor64(clock64);
}

//*** _plat__TimerWasReset()
// 
//
// If the resetFlag parameter is SET, then the flag will be CLEAR before the 
// function returns.

/**
 * This function is used to interrogate the flag indicating if the tick timer has
 * been reset. Reset happens only on 64-bit clock overflow.
 * 
 * @return true timer was reset from last call to _plat__TimerWasReset
 * @return false timer was not reset
 */
bool _plat__TimerWasReset(void)
{
	bool value = clock_was_reset;
	clock_was_reset = false;
	return value;
}

/**
 * This function is used to interrogate the flag indicating if the tick timer has 
 * been stopped. If so, this is typically a reason to roll the nonce.
 * 
 * @return true timer was stopped from last call to _plat__TimerWasStopped
 * @return false timer was not stopped
 */
bool _plat__TimerWasStopped(void)
{
	// Timer is never stopped as long as TPM is powered.
	bool value = clock_was_stopped;
	clock_was_stopped = false;
	return value;
}

/**
 * Clock rate adjusting
 */
void _plat__ClockAdjustRate(int adjust)
{
	// See https://github.com/lpn-plant/zephyr-spi-app/issues/6
	assert(0);
}
