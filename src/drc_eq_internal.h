/* SPDX-License-Identifier: MIT */
#ifndef AUDIO_PROCESS_DRC_EQ_INTERNAL_H
#define AUDIO_PROCESS_DRC_EQ_INTERNAL_H

#include "biquad.h"

#define DRC_MAX_EQ_BANDS 5

struct drc_eq_state {
	int sample_rate;
	int frame_size;
	int enabled_drc;
	int enabled_eq;

	/* DRC */
	float threshold;
	float knee;
	float ratio;
	float attack_coeff;
	float release_coeff;
	float envelope;

	/* EQ */
	struct biquad eq_bands[DRC_MAX_EQ_BANDS];
	int num_eq_bands;
};

struct drc_eq_state *drc_eq_alloc(int sample_rate);
float drc_compute_gain(struct drc_eq_state *s, float level);
void drc_process(struct drc_eq_state *s, float *buf, int n);
void eq_process(struct drc_eq_state *s, float *buf, int n);

#endif
