#define DT_DRV_COMPAT twpm_lpc_fpga

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

// TOOD: make configurable
LOG_MODULE_REGISTER(LPC, LOG_LEVEL_DBG);

DT_INST_FOREACH_STATUS_OKAY(LPC_INIT)
