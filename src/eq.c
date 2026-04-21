/* SPDX-License-Identifier: MIT */
#include "drc_eq_internal.h"
#include "biquad.h"

void eq_process(struct drc_eq_state *s, float *buf, int n)
{
	for (int i = 0; i < n; i++) {
		float sample = buf[i];
		for (int b = 0; b < s->num_eq_bands; b++)
			sample = biquad_process(&s->eq_bands[b], sample);
		buf[i] = sample;
	}
}
