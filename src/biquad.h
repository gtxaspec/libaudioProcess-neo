/* SPDX-License-Identifier: MIT */
#ifndef AUDIO_PROCESS_BIQUAD_H
#define AUDIO_PROCESS_BIQUAD_H

#include <stdint.h>

struct biquad {
	float b0, b1, b2, a1, a2;
	float z1, z2;
};

void biquad_design_peaking(struct biquad *bq, float fs, float freq,
			   float gain_db, float q);
void biquad_design_notch(struct biquad *bq, float fs, float freq, float bw);
void biquad_reset(struct biquad *bq);
float biquad_process(struct biquad *bq, float in);
int16_t biquad_process_i16(struct biquad *bq, int16_t in);

#endif
