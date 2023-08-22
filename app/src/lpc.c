#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>

// TOOD: make configurable
LOG_MODULE_REGISTER(LPC, LOG_LEVEL_DBG);

static void irq_handler(void *param) {
	(void)param;

	LOG_INF("IRQ happened");
}

void twpm_lpc_init() {
	struct {
		uint32_t base;
		uint32_t size;
	} info = DT_PROP(DT_ALIAS(lpcfpga), reg);

	uint32_t *reg = (uint32_t *)info.base;

	LOG_INF("Found controller at 0x%08x-0x%08x", info.base, info.base + info.size - 1);
	LOG_INF("reg[0] = 0x%08x", reg[0]);
	LOG_INF("reg[1] = 0x%08x", reg[1]);
	LOG_INF("reg[2] = 0x%08x", reg[2]);
	LOG_INF("reg[3] = 0x%08x", reg[3]);
	LOG_INF("reg[4] = 0x%08x", reg[4]);
	LOG_INF("reg[5] = 0x%08x", reg[5]);

	IRQ_CONNECT(4, 1, irq_handler, NULL, 0);
	irq_enable(4);

	LOG_INF("IRQ enabled");
}
