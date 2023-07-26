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

// ms-tpm denotes exported functions with this macro. We want to avoid including
// ms-tpm headers due to conflict with Zephyr headers so we define this macro
// ourselves.
#define LIB_EXPORT
#include "ExecCommand_fp.h"

#include <setjmp.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(cmd);

static jmp_buf s_jumpBuffer;
static bool jb_valid = false;

/**
 * This version of RunCommand will set up a jum_buf and call ExecuteCommand(). If
 * the command executes without failing, it will return and RunCommand will return.
 * If there is a failure in the command, then _plat__Fail() is called and it will
 * longjump back to RunCommand which will call ExecuteCommand again. However, this
 * time, the TPM will be in failure mode so ExecuteCommand will simply build
 * a failure response and return.
 *
 * @param[in] requestSize command buffer size
 * @param[in] request command buffer
 * @param[in,out] responseSize response buffer size
 * @param[in,out] response response buffer
 */
void twpm_run_command(unsigned int requestSize, unsigned char *request,
		       unsigned int *responseSize, unsigned char **response)
{
	setjmp(s_jumpBuffer);
	jb_valid = true;
	ExecuteCommand((uint32_t)requestSize, request, (uint32_t*)responseSize, response);
}

//***_plat__Fail()
// This is the platform depended failure exit for the TPM.
FUNC_NORETURN void _plat__FailDetailed(char *file, int line, const char *func)
{
	LOG_ERR("%s (%s@%d)", func, file, line);
	if (jb_valid) {
		jb_valid = false;
		longjmp(&s_jumpBuffer[0], 1);
	} else {
		// jump buffer is initialized only by twpm_run_command, if we
		// are executing command directly and that command fails we
		// could jump into nowhere.
		LOG_ERR("Critical TPM error, halting ...");
		while (1) {
			k_thread_suspend(k_current_get());
		}
	}
}
