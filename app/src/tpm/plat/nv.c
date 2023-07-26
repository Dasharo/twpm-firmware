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

//**Introduction
/*
	This file contains the NV read and write access methods.  This implementation
	uses RAM/file and does not manage the RAM/file as NV blocks.
	The implementation may become more sophisticated over time.
*/

#include <wolfssl/wolfcrypt/sha512.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>

LOG_MODULE_REGISTER(nv);

#define FLASH_NODE DT_NODELABEL(flash)

#define NVINTEGRITYMAGIC (0x44494E54) // TNID - TPM NV Integrity Data
typedef struct {
	struct {
		uint32_t magic;
		uint8_t nv_digest[WC_SHA512_DIGEST_SIZE];
	} sig;
	uint8_t nv_signature[WC_SHA512_DIGEST_SIZE];
	// Nucleo-TPM stored here hash of the entire NV among other data like
	// write count and last write time. nv_signature field was generated by
	// using NV hash and TPM unique ID to form sort of cryptographic
	// signature.
	//
	// This was ok in emulated TPM but in real TPM either we assume that NV
	// is safe and cannot be extracted (such as when flash is burried inside
	// SoC) and we don't have to use signature (hash is enough for detecting
	// NV corruption), or when we use an external flash signature is enough,
	// we have to use encryption.
} nv_integrity_t;

static const struct flash_area *flash_nv_integrity = NULL;
static const struct flash_area *flash_nv_storage = NULL;
static bool nv_ok = false;
static uint8_t *nv_shadow = NULL;
static uint8_t *nv_integrity = NULL;
static size_t nv_erase_block_size;
static uint8_t nv_erase_polarity;
static size_t nv_write_block_size;
static size_t nv_integrity_size;

/**
 * @brief Erase flash device
 * 
 * @param flash     Pointer to flash device or to flash area.
 * @retval          0 on success, negative on failure
 */
static int nv_erase_area(const struct flash_area *area) {
	LOG_INF("flash: erase 0x%04lx-0x%04lx", area->fa_off,
		area->fa_off + area->fa_size - 1);
	return flash_area_erase(area, 0, area->fa_size);
}

/**
 * @brief Initialize NV - erase NV.
 *
 * This function erases NV to prepare for first usage. NV is written after
 * commit.
 */
static int nv_format(void) {
	if (nv_erase_area(flash_nv_integrity) < 0) {
		LOG_ERR("Failed to erase nv_integrity");
		return -1;
	}

	if (nv_erase_area(flash_nv_storage) < 0) {
		LOG_ERR("Failed to erase nv_storage");
		return -1;
	}

	return 0;
}

/**
 * @brief Open flash partition.
 * 
 * @param[in] id Partition ID obtained using FLASH_AREA_ID macro.
 * @param[in] label Partition label obtained using FLASH_AREA_LABEL_STR macro.
 * 
 * @retval  Pointer to device object, or NULL on error.
 */
static const struct flash_area *nv_partition_open(int id, const char *label) {
	const struct flash_area *area;
	int ret = flash_area_open(id, &area);
	if (ret < 0) {
		LOG_ERR("Failed to open flash area %s: %d", label, ret);
		return NULL;
	}

	LOG_INF("flash: %s: 0x%04lx - 0x%04lx", label, area->fa_off,
		area->fa_off + area->fa_size - 1);

	const struct device *device = flash_area_get_device(area);
	if (!device) {
		LOG_ERR("flash_area_get_device returned NULL");
		return NULL;
	}

	if (!device_is_ready(device)) {
		LOG_ERR("Underlying flash device is not ready");
		return NULL;
	}

	return area;
}

/**
 * @brief Initializes TPM NV subsystem.
 *
 * This function on the first call formats NV storage, on subsequent calls NV is
 * read and verified.
 *
 * @retval 0 if was loaded succesfully, 1 if a fresh NV was initialized,
 *         negative on error.
 */
int twpm_init_nv(void)
{
	bool integrity_verify = false;
	bool first_run = true;
#if 0

	flash_nv_integrity = nv_partition_open(FLASH_AREA_ID(nv_integrity),
					       FLASH_AREA_LABEL_STR(nv_integrity));
	flash_nv_storage = nv_partition_open(FLASH_AREA_ID(nv_storage),
					     FLASH_AREA_LABEL_STR(nv_storage));

	uint32_t magic;
	if (flash_area_read(flash_nv_integrity, 0, &magic, sizeof magic) < 0) {
		LOG_ERR("Flash read failed");
		return -1;
	}

	if (magic != NVINTEGRITYMAGIC) {
		LOG_INF("Invalid integrity magic, formatting NV ...");
		if (nv_format() < 0) {
			LOG_ERR("NV format failed");
			return -1;
		}
	} else {
		integrity_verify = true;
		first_run = false;
	}

	// Assuming NV integrity and storage are located on the same device.
	const struct device *device = flash_area_get_device(flash_nv_storage);
	nv_erase_polarity = flash_area_erased_val(flash_nv_storage);
	struct flash_pages_info info;
	// Assume every block has the same size.
	flash_get_page_info_by_idx(device, 0, &info);
	nv_erase_block_size = info.size;
	nv_write_block_size = flash_get_write_block_size(device);

	LOG_INF("NV: erase block size = %d", nv_erase_block_size);
	LOG_INF("NV: write block size = %d", nv_write_block_size);

	// Not an ideal solution, contains shadow copy of entire NV (16 KiB).
	nv_shadow = k_malloc(flash_nv_storage->fa_size);
	if (!nv_shadow) {
		LOG_ERR("NV: cannot allocate enough memory");
		return -1;
	}

	// Need to allocate multiple of write block size for integrity_t
	nv_integrity_size = sizeof(nv_integrity_t);
	if (nv_integrity_size % nv_write_block_size != 0)
		nv_integrity_size += nv_write_block_size - (nv_integrity_size % nv_write_block_size);
	nv_integrity = k_malloc(nv_integrity_size);
	if (!nv_integrity) {
		LOG_ERR("NV: cannot allocate memory");
		return -1;
	}
	memset(nv_integrity, nv_erase_polarity, nv_integrity_size);

	int ret = flash_area_read(flash_nv_storage, 0, nv_shadow, flash_nv_storage->fa_size);
	if (ret < 0) {
		LOG_ERR("NV: read failed");
		return -1;
	}

	if (integrity_verify) {
		ret = flash_area_read(flash_nv_integrity, 0, nv_integrity, nv_integrity_size);
		if (ret < 0) {
			LOG_ERR("NV: integrity read failed");
			return -1;
		}

		nv_integrity_t *integrity = (nv_integrity_t*)nv_integrity;
		uint8_t digest[WC_SHA512_DIGEST_SIZE];
		wc_Sha512 hasher;
		wc_InitSha512(&hasher);
		wc_Sha512Update(&hasher, nv_shadow, flash_nv_storage->fa_size);
		wc_Sha512Final(&hasher, digest);

		if (memcmp(integrity->sig.nv_digest, digest, WC_SHA512_DIGEST_SIZE) == 0) {
			LOG_INF("NV: verify OK");
		} else {
			LOG_ERR("NV: verify failed, NV is corrupted");
			return -1;
		}
	}
#endif
	LOG_INF("NV init done");
	nv_ok = true;
	return first_run ? 1 : 0;
}

/**
 *
 * @param[in] param Platform specific data, not used currently
 * 
 * @retval  0 Success
 * @retval >0 Recoverable error
 * @retval <0 Unrecoverable error
 */
int _plat__NVEnable(void *param) {
	return nv_ok ? 0 : -1;
}

/**
 * @brief Check if NV is properly initialized and available
 * 
 * @retval 0 NV is available
 * @retval 1 NV not available due to write failure
 * @retval 2 NV not available due to rate limit (not used)
 */
int _plat__IsNvAvailable(void)
{
	return nv_ok ? 0 : 1;
}

/**
 * @brief Read a chunk of NV memory
 * 
 * @param[in] offset Offset in bytes.
 * @param[in] size Size in bytes.
 * @param[out] data Output buffer.
 */
void _plat__NvMemoryRead(unsigned int offset, unsigned int size, void *data)
{
	LOG_DBG("NV: read 0x%04x - 0x%04x", offset, offset + size - 1);

	if (!nv_ok) {
		LOG_ERR("NV read attempted, but NV is not initialized");
		return;
	}

	if (offset + size > flash_nv_storage->fa_size) {
		LOG_ERR("Attempt to read NV beyond its bounds");
		return;
	}

	memcpy(data, &nv_shadow[offset], size);
}

/**
 * This function checks to see if the NV is different from the test value. This is
 * so that NV will not be written if it has not changed.
 *
 * @param[in] offset offset from start of NV
 * @param[in] size how many bytes to compare
 * @param[in] data buffer to compare against
 *
 * @retval TRUE(1)    the NV location is different from the test value
 * @retval FALSE(0)   the NV location is the same as the test value
 */
int _plat__NvIsDifferent(unsigned int offset, unsigned int size, void *data)
{
	return (memcmp(&nv_shadow[offset], data, size) != 0);
}

//***_plat__NvMemoryWrite()
// This function is used to update NV memory. The "write" is to a memory copy of
// NV. At the end of the current command, any changes are written to
// the actual NV memory.
// NOTE: A useful optimization would be for this code to compare the current 
// contents of NV with the local copy and note the blocks that have changed. Then
// only write those blocks when _plat__NvCommit() is called.
void _plat__NvMemoryWrite(unsigned int offset, unsigned int size, void *data)
{
	LOG_DBG("NV: write 0x%04x - 0x%04x", offset, offset + size - 1);
	
	if (!nv_ok) {
		LOG_ERR("NV write attempted, but NV is not initialized");
		return;
	}

	if (offset + size > flash_nv_storage->fa_size) {
		LOG_ERR("Attempt to write NV beyond its bounds");
		return;
	}

	memcpy(&nv_shadow[offset], data, size);
}

//***_plat__NvMemoryClear()
// Function is used to set a range of NV memory bytes to an implementation-dependent
// value. The value represents the erase state of the memory.
void _plat__NvMemoryClear(unsigned int offset, unsigned int size)
{
	if (offset + size > flash_nv_storage->fa_size) {
		LOG_ERR("Attempt to erase NV beyond its bounds");
		return;
	}

	memset(&nv_shadow[offset], nv_erase_polarity, size);
}

//***_plat__NvMemoryMove()
// Function: Move a chunk of NV memory from source to destination
//      This function should ensure that if there overlap, the original data is
//      copied before it is written
void _plat__NvMemoryMove(unsigned int src, unsigned int dst, unsigned int size)
{
	if (src + size > flash_nv_storage->fa_size
	    || dst + size > flash_nv_storage->fa_size) {
		memmove(&nv_shadow[dst], &nv_shadow[src], size);
	}
}

//***_plat__NvCommit()
// Update NV chip
// return type: int
//  0       NV write success
//  non-0   NV write fail
int _plat__NvCommit(void)
{
	int ret;

	if (!nv_ok) {
		LOG_ERR("NV commit attempted, but NV is not initialized");
		return -1;
	}

	LOG_INF("NV commit");

	// TODO: should write only blocks that had changed to reduce hardware
	// wear out. Also, write must be atomic and fault tolerant to avoid NV
	// corruption due to power loss; in the simplest scenario we could use 2
	// copies of NV for that, more complex solution would involve using
	// per-block copies. To further reduce flash damage we could use wear
	// levelling.

	// nv_integrity was malloc()ed which ensures proper alignment.
	#if 0
	nv_integrity_t *integrity = (nv_integrity_t*)nv_integrity;
	integrity->sig.magic = NVINTEGRITYMAGIC;

	wc_Sha512 hasher;
	wc_InitSha512(&hasher);
	wc_Sha512Update(&hasher, nv_shadow, flash_nv_storage->fa_size);
	wc_Sha512Final(&hasher, integrity->sig.nv_digest);

	ret = nv_erase_area(flash_nv_storage);
	if (ret < 0) {
		LOG_ERR("NV: storage erase failed: %d", ret);
		return -1;
	}
	ret = flash_area_write(flash_nv_storage, 0, nv_shadow, flash_nv_storage->fa_size);
	if (ret < 0) {
		LOG_ERR("NV: commit failed: %d", ret);
		return -1;
	}

	ret = nv_erase_area(flash_nv_integrity);
	if (ret < 0) {
		LOG_ERR("NV: integrity erase failed: %d", ret);
		return -1;
	}
	ret = flash_area_write(flash_nv_integrity, 0, integrity, nv_integrity_size);
	if (ret < 0) {
		LOG_ERR("NV: commit failed (integrity): %d", ret);
		return -1;
	}
	#endif

	return 0;
}
