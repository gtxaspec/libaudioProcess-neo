#include <stdlib.h>
#include <math.h>
#include <stdint.h>

#include "audio_process.h"
#include "drc_eq_internal.h"
#include "biquad.h"
#include "util.h"

/* T32 combined DRC+EQ API -- thin glue over drc.c and eq.c */

void *audio_process_drc_eq_create(int sample_rate, int channels)
{
	(void)channels;

	struct drc_eq_state *s = calloc(1, sizeof(*s));
	if (!s)
		return NULL;

	s->sample_rate = sample_rate > 0 ? sample_rate : 16000;
	s->frame_size = s->sample_rate / 100;
	s->threshold = 0.5f;
	s->knee = 0.7f;
	s->ratio = 0.5f;

	float attack_ms = 5.0f;
	float release_ms = 50.0f;
	s->attack_coeff = 1.0f - expf(-1000.0f /
				      ((float)s->sample_rate * attack_ms));
	s->release_coeff = 1.0f - expf(-1000.0f /
				       ((float)s->sample_rate * release_ms));
	return s;
}

int audio_process_drc_eq_set_config(void *handle, int drc_threshold,
				    int drc_knee, int drc_ratio,
				    int eq_num_bands, int *eq_freqs,
				    float *eq_gains, int *eq_qs)
{
	struct drc_eq_state *s = handle;
	if (!s)
		return -1;

	if (drc_threshold >= 0) {
		s->threshold = powf(10.0f, (float)drc_threshold / -20.0f);
		s->knee = powf(10.0f, (float)drc_knee / -20.0f);
		if (drc_ratio > 0)
			s->ratio = 1.0f / (float)drc_ratio;
	}

	if (eq_num_bands > 0 && eq_freqs && eq_gains) {
		if (eq_num_bands > DRC_MAX_EQ_BANDS)
			eq_num_bands = DRC_MAX_EQ_BANDS;
		s->num_eq_bands = eq_num_bands;
		for (int i = 0; i < eq_num_bands; i++) {
			float q = (eq_qs && eq_qs[i] > 0) ?
				  (float)eq_qs[i] / 10.0f : 1.0f;
			biquad_design_peaking(&s->eq_bands[i],
					      (float)s->sample_rate,
					      (float)eq_freqs[i],
					      eq_gains[i], q);
		}
	}

	return 0;
}

int audio_process_drc_eq_enable(void *handle, int drc_enable, int eq_enable)
{
	struct drc_eq_state *s = handle;
	if (!s)
		return -1;
	s->enabled_drc = drc_enable;
	s->enabled_eq = eq_enable;
	return 0;
}

void audio_process_drc_eq_process(void *handle, const float *const *in,
				  float *const *out)
{
	struct drc_eq_state *s = handle;
	if (!s || !in || !*in || !out || !*out)
		return;

	const float *src = in[0];
	float *dst = out[0];
	int n = s->frame_size;

	for (int i = 0; i < n; i++)
		dst[i] = src[i];

	if (s->enabled_drc)
		drc_process(s, dst, n);
	if (s->enabled_eq)
		eq_process(s, dst, n);
}

void audio_process_drc_eq_free(void *handle)
{
	free(handle);
}
