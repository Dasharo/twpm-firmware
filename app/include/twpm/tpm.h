#pragma once

#define TWPM_IRQ                11

#define TWPM_REG_STATUS         0x00
#define TWPM_REG_STATUS_EXEC     (1 << 0)
#define TWPM_REG_STATUS_ABORT    (1 << 1)
#define TWPM_REG_STATUS_COMPLETE (1 << 2)
#define TWPM_REG_OP_TYPE        0x04
#define TWPM_REG_OP_TYPE_NOP     0x0
#define TWPM_REG_OP_TYPE_CMD     0x1
#define TWPM_REG_OP_TYPE_ILLEGAL 0xC
#define TWPM_REG_LOCALITY       0x08
#define TWPM_REG_BUF_SIZE       0x0C
#define TWPM_REG_COMPLETE       0x40

void twpm_init();
int twpm_init_nv(void);
void twpm_init_unique(void);
void twpm_run_command(unsigned int requestSize, unsigned char *request,
		        unsigned int *responseSize, unsigned char **response);
