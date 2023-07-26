#pragma once

void twpm_init();
int twpm_init_nv(void);
void twpm_init_unique(void);
void twpm_run_command(unsigned int requestSize, unsigned char *request,
		        unsigned int *responseSize, unsigned char **response);
