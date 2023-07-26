#pragma once

#include <zephyr/kernel.h>

int _plat__IsCanceled(void);
uint64_t _plat__TimerRead(void);
bool _plat__TimerWasReset(void);
bool _plat__TimerWasStopped(void);
void _plat__ClockAdjustRate(int adjust);
int32_t _plat__GetEntropy(unsigned char *entropy, uint32_t amount);
unsigned char _plat__LocalityGet(void);
int _plat__NVEnable(void *platParameter);
int _plat__IsNvAvailable(void);
void _plat__NvMemoryRead(unsigned int offset, unsigned int size, void *data);
int _plat__NvIsDifferent(unsigned int offset, unsigned int size, void *data);
void _plat__NvMemoryWrite(unsigned int offset, unsigned int size, void *data);
void _plat__NvMemoryClear(unsigned int start, unsigned int size);
void _plat__NvMemoryMove(unsigned int src, unsigned int dst, unsigned int size);
int _plat__NvCommit(void);
int _plat__WasPowerLost(void);
int _plat__PhysicalPresenceAsserted(void);
FUNC_NORETURN void _plat__FailDetailed(char *file, int line, const char * func);
