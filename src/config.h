/* SPDX-License-Identifier: MIT */
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

	/* Ingenic AEC extensions */
	bool user_mode;
	float mu_min;
	float mu_decay;
	float cor_thd1;
	float cor_thd2;
	float cor_thd3;
	float cor_thd4;
	float far_pow_thd;
	float safe_suppression;
	int restrain_band_center;
	int restrain_band_wide;
	float restrain_factor;
};

int config_load(const char *path, struct aec_config *cfg);
void config_defaults(struct aec_config *cfg);

#endif
