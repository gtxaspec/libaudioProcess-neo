#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

/*
 * DRC (Dynamic Range Compression) + EQ (Parametric Equalizer)
 *
 * Based on Ingenic T32/T41 analysis.
 *
 * DRC operates in the frequency domain:
 *   - 256-point FFT on each frame
 *   - Per-bin gain computation with threshold/knee/ratio
 *   - Inverse FFT + overlap-add
 *
 * EQ operates as a frequency-domain magnitude shaper:
 *   - User specifies control points (freq, gain_dB)
 *   - Linear interpolation between points
 *   - Per-bin gain applied in frequency domain
 *
 * Our implementation uses time-domain processing which is more efficient
 * for the small frame sizes used in audio (160-480 samples):
 *   - DRC: envelope follower + gain curve (attack/release/threshold/ratio)
 *   - EQ: cascaded biquad filters (up to 5 bands)
 * This avoids the FFT overhead and latency of the original.
 *
 * Two API variants are supported:
 *   - audio_process_drc_eq_* (T32): combined DRC+EQ
 *   - audio_process_drc_* (T41 ZRT): DRC only
 */

#define DRC_MAX_EQ_BANDS 5

struct biquad {
	float b0, b1, b2, a1, a2;
	float z1, z2;
};

struct drc_eq_state {
	int sample_rate;
	int enabled_drc;
	int enabled_eq;

	/* DRC parameters */
	float threshold;    /* linear amplitude */
	float knee;         /* linear amplitude */
	float ratio;        /* compression ratio (0..1, 0=inf compression) */
	float attack_coeff;
	float release_coeff;
	float envelope;

	/* EQ */
	struct biquad eq_bands[DRC_MAX_EQ_BANDS];
	int num_eq_bands;
};

static void biquad_peaking(struct biquad *bq, float fs, float freq,
			   float gain_db, float q)
{
	float A = powf(10.0f, gain_db / 40.0f);
	float w0 = 2.0f * (float)M_PI * freq / fs;
	float alpha = sinf(w0) / (2.0f * q);

	float a0 = 1.0f + alpha / A;
	bq->b0 = (1.0f + alpha * A) / a0;
	bq->b1 = (-2.0f * cosf(w0)) / a0;
	bq->b2 = (1.0f - alpha * A) / a0;
	bq->a1 = (-2.0f * cosf(w0)) / a0;
	bq->a2 = (1.0f - alpha / A) / a0;
	bq->z1 = 0.0f;
	bq->z2 = 0.0f;
}

static float biquad_process_sample(struct biquad *bq, float in)
{
	float out = bq->b0 * in + bq->z1;
	bq->z1 = bq->b1 * in - bq->a1 * out + bq->z2;
	bq->z2 = bq->b2 * in - bq->a2 * out;
	return out;
}

static float drc_compute_gain(struct drc_eq_state *s, float level)
{
	if (level <= s->threshold)
		return 1.0f;

	if (level <= s->knee) {
		float knee_range = s->knee - s->threshold;
		float x = (level - s->threshold) / knee_range;
		float target = s->threshold + knee_range * x * s->ratio;
		return target / level;
	}

	float over = level - s->threshold;
	float target = s->threshold + over * s->ratio;
	return target / level;
}

/* ---- DRC/EQ combined API (T32) ---- */

void *audio_process_drc_eq_create(int sample_rate, int channels)
{
	(void)channels;
	struct drc_eq_state *s = calloc(1, sizeof(*s));
	if (!s)
		return NULL;

	s->sample_rate = sample_rate > 0 ? sample_rate : 16000;
	s->threshold = 0.5f;
	s->knee = 0.7f;
	s->ratio = 0.5f;
	s->envelope = 0.0f;

	float attack_ms = 5.0f;
	float release_ms = 50.0f;
	s->attack_coeff = 1.0f - expf(-1.0f / (s->sample_rate * attack_ms / 1000.0f));
	s->release_coeff = 1.0f - expf(-1.0f / (s->sample_rate * release_ms / 1000.0f));

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
			float q = (eq_qs && eq_qs[i] > 0) ? (float)eq_qs[i] / 10.0f : 1.0f;
			biquad_peaking(&s->eq_bands[i], (float)s->sample_rate,
				       (float)eq_freqs[i], eq_gains[i], q);
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
	int frame_size = 160;

	for (int i = 0; i < frame_size; i++) {
		float sample = src[i];

		if (s->enabled_drc) {
			float level = fabsf(sample) / 32768.0f;
			float coeff = level > s->envelope ?
				      s->attack_coeff : s->release_coeff;
			s->envelope += coeff * (level - s->envelope);

			float gain = drc_compute_gain(s, s->envelope);
			sample *= gain;
		}

		if (s->enabled_eq) {
			for (int b = 0; b < s->num_eq_bands; b++)
				sample = biquad_process_sample(&s->eq_bands[b], sample);
		}

		dst[i] = sample;
	}
}

void audio_process_drc_eq_free(void *handle)
{
	free(handle);
}

/* ---- DRC only API (T41 ZRT variant) ---- */

void *audio_process_drc_create(int sample_rate, int channels)
{
	struct drc_eq_state *s = audio_process_drc_eq_create(sample_rate, channels);
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

	s->threshold = powf(10.0f, (float)threshold / -20.0f);
	s->knee = powf(10.0f, (float)knee / -20.0f);
	if (ratio > 0)
		s->ratio = 1.0f / (float)ratio;
	(void)makeup;
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

		float gain = drc_compute_gain(s, s->envelope);
		sample *= gain;

		if (sample > 32767.0f) sample = 32767.0f;
		if (sample < -32768.0f) sample = -32768.0f;
		data[i] = (int16_t)sample;
	}
}

void audio_process_drc_free(void *handle)
{
	free(handle);
}
