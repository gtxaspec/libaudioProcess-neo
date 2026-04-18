#ifndef AUDIO_PROCESS_CONFIG_H
#define AUDIO_PROCESS_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

struct aec_config {
	bool enabled;
	float far_gain;
	float near_gain;
	int delay_ms;
	int16_t nlp_mode;        /* 0=conservative, 1=moderate, 2=aggressive */
	bool metrics;
	bool delay_logging;
	bool delay_agnostic;
	bool extended_filter;
	bool drift_compensation;
};

int config_load(const char *path, struct aec_config *cfg);
void config_defaults(struct aec_config *cfg);

#endif
