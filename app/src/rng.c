#include "zephyr/sys/printk.h"
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/random/random.h>
#include <assert.h>

#ifdef CONFIG_TWPM_RNG_TEST
static uint8_t buffer[64];
char buffer_ascii[sizeof buffer * 2 + 1];

static int rng_test(const struct shell *sh) {
	int ret = sys_csrand_get(buffer, sizeof buffer);
	if (ret == 0) {
		for (int i = 0; i < sizeof buffer; i++) {
			uint8_t t;

			t = buffer[i] >> 4;
			buffer_ascii[i * 2] = t < 10 ? t + '0' : t + 'a' - 10;

			t = buffer[i] & 0x0f;
			buffer_ascii[i * 2 + 1] = t < 10 ? t + '0' : t + 'a' - 10;
		}

		buffer_ascii[sizeof buffer_ascii - 1] = '\0';

		if (sh)
			shell_print(sh, "%s", buffer_ascii);
		else
			printk("%s\n", buffer_ascii);
	} else {
		if (sh)
			shell_error(sh, "RNG error %d", ret);
		else
			printk("RNG error %d\n", ret);
	}

	return ret;
}

void twpm_test_rng() {
	while (true) {
		int ret = rng_test(NULL);
		if (ret < 0) {
			printk("RNG test fail");
			k_panic();
		}

		k_yield();
	}
}

#ifdef CONFIG_SHELL
static const struct shell *shell;
static struct k_work_delayable work;

static void rng_work(struct k_work *_work) {
	ARG_UNUSED(_work);

	assert(shell != NULL);

	int ret = rng_test(shell);

	if (ret < 0) {
		k_work_cancel_delayable(&work);
		shell_set_bypass(shell, NULL);
		shell = NULL;
	} else
		k_work_reschedule(&work, K_NO_WAIT);
}

static void rng_bypass(const struct shell *sh, uint8_t *data, size_t len) {
	assert(shell != NULL);
	assert(shell == sh);

	for (size_t i = 0; i < len; i++) {
		if (data[i] == 0x03) {
			k_work_cancel_delayable(&work);
			shell_set_bypass(shell, NULL);
			shell = NULL;
			break;
		}
	}

}

static int cmd_rng_test(const struct shell *sh, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell = sh;
	k_work_init_delayable(&work, rng_work);
	shell_set_bypass(sh, rng_bypass);
	k_work_reschedule(&work, K_NO_WAIT);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(rng_commands,
	SHELL_CMD(test, NULL, "Run RNG test", cmd_rng_test),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(rng, &rng_commands, "RNG (Random Number Generator) commands", NULL);

#endif
#endif
