#include <twpm/platform.h>
#include <zephyr/random/rand32.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(rng);

/**
 * @brief Fills destination buffer with random data.
 * 
 * @param entropy destination buffer
 * @param amount destination buffer size
 *
 * @retval amount of bytes filled on success
 * @retval -1 on error. Note: according from doc from Platform_fp.h header,
 *         error is sticky which suggest platform should keep track of error.
 *         ms-tpm already handles this internally so we don't have to.
 */
int32_t _plat__GetEntropy(unsigned char *entropy, uint32_t amount)
{
#if 0
	int ret = sys_csrand_get(entropy, amount);
	if (ret < 0) {
		LOG_ERR("RNG failed\n");
		return -1;
	}

	return amount;
#else
	return 0;
#endif
}
