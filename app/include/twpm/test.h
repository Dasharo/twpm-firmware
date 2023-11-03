#pragma once

#include <stdint.h>

void twpm_selftest();

// Functions below are used only for debugging, should not be even compiled-in
// into release builds.

/**
 * @brief Decodes TPM_RC_* values into human-readable string.
 * 
 * @param in Pointer to 4 byte buffer containing return code.
 */
char* tpmdbg_decode_rc(uint8_t* in);

/**
 * @brief Decodes TPM_CC_* values (TPM commands) into human-readable string.
 * 
 * @param in Pointer to 4 byte buffer containing command code.
 */
char* tpmdbg_decode_cc(uint8_t* in);
