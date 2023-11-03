#include <twpm/tpm.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(init);

// ms-tpm denotes exported functions with this macro. We want to avoid including
// ms-tpm headers due to conflict with Zephyr headers so we define this macro
// ourselves.
#define LIB_EXPORT

#include <_TPM_Init_fp.h>
#include <Manufacture_fp.h>

#include <assert.h>

void twpm_init() {
	int ret;

	twpm_init_unique();
	ret = twpm_init_nv();
	if (ret == 1) {
		// Call TPM_Manufacture only on the first run and only if NV init
		// was successful to avoid TPM re-manufacturing and application
		// crash in case NV is corrupted.
		int ret = TPM_Manufacture(true);
		if (ret == 0) {
			// FIXME: TPM_Manufacture is intended for simulation,
			// and it assumes running in simulated environment where
			// HW errors are not possible, so we may get a
			// false-positive result.
			LOG_INF("TPM manufacture OK");
		}
		else
			LOG_INF("TPM manufacturing failed");
	}

	_TPM_Init();
}
