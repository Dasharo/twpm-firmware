#include <twpm/platform.h>

/**
 * @brief Check if physical presence is signaled
 * 
 * @retval TRUE(1)          if physical presence is signaled
 * @retval FALSE(0)         if physical presence is not signaled
 */
int _plat__PhysicalPresenceAsserted(void)
{
    // Currently we don't implement any hardware method for physical presence.
    // TPM specification doesn't define a specific hardware method, so it can be
    // anything, e.g. a button.
    //
    // Please see Physical Presence Interface section 7
    return false;
}
