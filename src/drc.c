#include <stdlib.h>
#include <math.h>
#include <stdint.h>

#include "audio_process.h"
#include "drc_eq_internal.h"
#include "util.h"

float drc_compute_gain(struct drc_eq_state *s, float level)
{
	if (level <= s->threshold)
		return 1.0f;

	float target;
	if (level <= s->knee) {
		float range = s->knee - s->threshold;
		float x = (level - s->threshold) / range;
		target = s->threshold + range * x * s->ratio;
	} else {
		target = s->threshold + (level - s->threshold) * s->ratio;
	}

	return target / level;
}

void drc_process(struct drc_eq_state *s, float *buf, int n)
{
	for (int i = 0; i < n; i++) {
		float level = fabsf(buf[i]) / 32768.0f;
		float coeff = level > s->envelope ?
			      s->attack_coeff : s->release_coeff;
		s->envelope += coeff * (level - s->envelope);
		buf[i] *= drc_compute_gain(s, s->envelope);
	}
}

static struct drc_eq_state *drc_eq_alloc(int sample_rate)
{
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

/* T41 ZRT API */

void *audio_process_drc_create(int sample_rate, int channels)
{
	(void)channels;
	struct drc_eq_state *s = drc_eq_alloc(sample_rate);
	if (s)
		s->enabled_drc = 1;
	return s;
}

int audio_process_drc_set_config(void *handle, int threshold, int knee,
				 int ratio, int makeup)
{
	struct drc_eq_state *s = handle;
	if (!s)
		return -1;

	(void)makeup;
	s->threshold = powf(10.0f, (float)threshold / -20.0f);
	s->knee = powf(10.0f, (float)knee / -20.0f);
	if (ratio > 0)
		s->ratio = 1.0f / (float)ratio;
	return 0;
}

void audio_process_drc_process(void *handle, int16_t *data, int num_samples)
{
	struct drc_eq_state *s = handle;
	if (!s || !data || num_samples <= 0)
		return;

	for (int i = 0; i < num_samples; i++) {
		float sample = (float)data[i];
		float level = fabsf(sample) / 32768.0f;
		float coeff = level > s->envelope ?
			      s->attack_coeff : s->release_coeff;
		s->envelope += coeff * (level - s->envelope);
		sample *= drc_compute_gain(s, s->envelope);
		data[i] = clamp_i16(sample);
	}
}

void audio_process_drc_free(void *handle)
{
	free(handle);
}
