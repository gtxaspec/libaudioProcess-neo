#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "webrtc/modules/audio_processing/aec/include/echo_cancellation.h"
#include "webrtc/modules/audio_processing/aec/aec_core.h"
#include "webrtc/modules/audio_processing/agc/legacy/gain_control.h"
#include "webrtc/modules/audio_processing/ns/include/noise_suppression.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"

#define AEC_MAGIC 0x61656330  /* "aec0" */
#define MAX_SAMPLES_10MS 480

struct aec_handle {
	uint32_t magic;
	int32_t sample_rate;
	void *aec;
};

struct aec_frame {
	const int16_t *far;
	int16_t *near;
	int reserved;
	int num_bytes;
};

/* ---- AEC ---- */

void *audio_process_aec_create(int sample_rate, const char *profile_path)
{
	(void)profile_path;

	struct aec_handle *h = calloc(1, sizeof(*h));
	if (!h)
		return NULL;

	h->magic = AEC_MAGIC;
	h->sample_rate = sample_rate;
	h->aec = WebRtcAec_Create();
	if (!h->aec)
		goto fail;

	if (WebRtcAec_Init(h->aec, sample_rate, sample_rate) != 0)
		goto fail;

	AecConfig cfg = {
		.nlpMode = kAecNlpModerate,
		.skewMode = kAecFalse,
		.metricsMode = kAecFalse,
		.delay_logging = kAecFalse
	};
	WebRtcAec_set_config(h->aec, cfg);
	WebRtcAec_enable_delay_agnostic(WebRtcAec_aec_core(h->aec), 1);
	WebRtcAec_enable_extended_filter(WebRtcAec_aec_core(h->aec), 1);

	return h;
fail:
	if (h->aec)
		WebRtcAec_Free(h->aec);
	free(h);
	return NULL;
}

int audio_process_aec_process(struct aec_handle *h, struct aec_frame *f)
{
	if (!h || h->magic != AEC_MAGIC)
		return -1;

	int bytes_per_10ms = (h->sample_rate * 2) / 100;
	if (bytes_per_10ms == 0)
		return -1;

	int num_frames = f->num_bytes / bytes_per_10ms;
	int samples_10ms = h->sample_rate / 100;
	float far_f[MAX_SAMPLES_10MS];
	float near_f[MAX_SAMPLES_10MS];
	float out_f[MAX_SAMPLES_10MS];

	for (int i = 0; i < num_frames; i++) {
		int offset = i * (bytes_per_10ms / 2);
		const int16_t *far_ptr = f->far + offset;
		int16_t *near_ptr = f->near + offset;

		for (int j = 0; j < samples_10ms; j++)
			far_f[j] = (float)far_ptr[j];

		WebRtcAec_BufferFarend(h->aec, far_f, (size_t)samples_10ms);

		for (int j = 0; j < samples_10ms; j++)
			near_f[j] = (float)near_ptr[j];

		const float *near_pp = near_f;
		float *out_pp = out_f;
		WebRtcAec_Process(h->aec, &near_pp, 1, &out_pp,
				  (size_t)samples_10ms, 0, 0);

		for (int j = 0; j < samples_10ms; j++) {
			float v = out_f[j];
			if (v > 32767.0f) v = 32767.0f;
			if (v < -32768.0f) v = -32768.0f;
			near_ptr[j] = (int16_t)v;
		}
	}

	return 0;
}

int audio_process_aec_free(struct aec_handle *h)
{
	if (!h || h->magic != AEC_MAGIC)
		return -1;

	WebRtcAec_Free(h->aec);
	free(h);
	return 0;
}

/* ---- AGC ---- */

void *audio_process_agc_create(void)
{
	return WebRtcAgc_Create();
}

int audio_process_agc_set_config(void *agc, int min_level, int max_level,
				 int mode, int sample_rate,
				 int16_t target_level, int16_t comp_gain,
				 uint8_t limiter_enable)
{
	if (!agc)
		return -1;

	if (WebRtcAgc_Init(agc, min_level, max_level, mode,
			   (uint32_t)sample_rate) != 0)
		return -1;

	WebRtcAgcConfig config = {
		.targetLevelDbfs = target_level,
		.compressionGaindB = comp_gain,
		.limiterEnable = limiter_enable
	};
	if (WebRtcAgc_set_config(agc, config) != 0)
		return -1;

	return 0;
}

void audio_process_agc_get_config(void *agc, WebRtcAgcConfig *out)
{
	if (agc && out)
		WebRtcAgc_get_config(agc, out);
}

int audio_process_agc_process(void *agc, const int16_t *const *in_near,
			      size_t num_bands, size_t samples,
			      int16_t *const *out, int32_t in_mic_level,
			      int32_t *out_mic_level, int16_t echo,
			      uint8_t *saturation_warning)
{
	if (WebRtcAgc_Process(agc, in_near, num_bands, samples, out,
			      in_mic_level, out_mic_level,
			      echo, saturation_warning) != 0)
		return -1;
	return 0;
}

int audio_process_agc_free(void *agc)
{
	if (!agc)
		return -1;
	WebRtcAgc_Free(agc);
	return 0;
}

/* ---- NS ---- */

void *audio_process_ns_create(void)
{
	return WebRtcNs_Create();
}

int audio_process_ns_set_config(void *ns, int sample_rate, int policy)
{
	if (!ns)
		return -1;
	if (WebRtcNs_Init(ns, (uint32_t)sample_rate) != 0)
		return -1;
	if (WebRtcNs_set_policy(ns, policy) != 0)
		return -1;
	return 0;
}

void audio_process_ns_get_config(void)
{
}

void audio_process_ns_process(void *ns, const float *const *in_bands,
			      int num_bands, float *const *out_bands)
{
	if (!ns || !in_bands || !*in_bands || !out_bands || !*out_bands || !num_bands)
		return;

	WebRtcNs_Analyze(ns, in_bands[0]);
	WebRtcNs_Process(ns, in_bands, (size_t)num_bands, out_bands);
}

int audio_process_ns_free(void *ns)
{
	if (!ns)
		return -1;
	WebRtcNs_Free(ns);
	return 0;
}

/* ---- HPF (biquad IIR, matching Ingenic implementation) ---- */

void audio_process_hpf_create(int16_t *state_x, int16_t *state_y,
			      int16_t init_x, int16_t init_y,
			      int count_x, int count_y)
{
	WebRtcSpl_MemSetW16(state_x, init_x, count_x);
	WebRtcSpl_MemSetW16(state_y, init_y, count_y);
}

int audio_process_hpf_process(int16_t *state, int16_t *data, int num_samples)
{
	if (!state)
		return -1;

	int16_t *coeffs = *(int16_t **)(state + 6);

	for (int i = 0; i < num_samples; i++) {
		int16_t x0 = data[i];
		int16_t x1 = state[2];
		int16_t x2 = state[3];
		int16_t y1 = state[0];
		int16_t y2 = state[1];

		state[3] = state[1];
		state[2] = state[0];
		state[5] = state[4];
		state[4] = x0;

		int32_t acc = (int32_t)x0 * coeffs[0] +
			      (int32_t)coeffs[1] * state[4] +
			      (int32_t)coeffs[2] * state[5] +
			      ((int32_t)x2 * coeffs[4] +
			       (int32_t)x1 * coeffs[3] +
			       (((int32_t)y2 * coeffs[4] +
				 (int32_t)y1 * coeffs[3]) >> 15)) * 2;

		int32_t rounded = acc + 0x800;
		int16_t y0 = (int16_t)(acc >> 13);

		if (rounded < -0x8000000)
			rounded = -0x8000000;
		if (rounded > 0x7ffffff)
			rounded = 0x7ffffff;

		state[0] = y0;
		state[1] = ((int16_t)acc + y0 * -0x2000) * 4;
		data[i] = (int16_t)(rounded >> 12);
	}

	return 0;
}

void audio_process_hpf_free(void)
{
}

/* ---- LPF (first-order IIR, from T32 analysis) ---- */

void audio_process_lpf_create(int16_t *state, int sample_rate, int cutoff_freq)
{
	float ratio = (float)cutoff_freq / (float)sample_rate;

	state[0] = 0;
	if (ratio > 0.0f && ratio < 0.5f) {
		double alpha = 1.0 - exp((double)ratio * -6.283185307);
		state[1] = (int16_t)(alpha * 256.0);
	} else {
		state[1] = 256;
	}
}

void audio_process_lpf_process(int16_t *state, int16_t *data, int num_samples)
{
	int16_t y = state[0];
	int16_t alpha = state[1];

	for (int i = 0; i < num_samples; i++) {
		y = (int16_t)((uint32_t)(((int32_t)data[i] - (int32_t)y) * (int32_t)alpha) >> 8) + y;
		data[i] = y;
	}

	state[0] = y;
}

void audio_process_lpf_free(void)
{
}

/* ---- HS (howling suppression stubs) ---- */

struct hs_handle {
	int enabled;
};

void *audio_process_hs_create(int sample_rate, int frame_size)
{
	(void)sample_rate;
	(void)frame_size;
	struct hs_handle *h = calloc(1, sizeof(*h));
	if (h)
		h->enabled = 1;
	return h;
}

void audio_process_hs_process(void *handle, int16_t *data, int num_samples)
{
	(void)handle;
	(void)data;
	(void)num_samples;
}

void audio_process_hs_free(void *handle)
{
	free(handle);
}

/* ---- DRC/EQ stubs (T32) ---- */

struct drc_eq_handle {
	int enabled;
};

void *audio_process_drc_eq_create(int sample_rate, int channels)
{
	(void)sample_rate;
	(void)channels;
	struct drc_eq_handle *h = calloc(1, sizeof(*h));
	return h;
}

int audio_process_drc_eq_set_config(void *handle, int drc_attack, int drc_release,
				    int drc_threshold, int drc_ratio,
				    int eq_bands, int eq_gains, int eq_freqs)
{
	(void)handle;
	(void)drc_attack;
	(void)drc_release;
	(void)drc_threshold;
	(void)drc_ratio;
	(void)eq_bands;
	(void)eq_gains;
	(void)eq_freqs;
	return 0;
}

int audio_process_drc_eq_enable(void *handle, int enable)
{
	struct drc_eq_handle *h = handle;
	if (h)
		h->enabled = enable;
	return 0;
}

void audio_process_drc_eq_process(void *handle, const int16_t *const *in,
				  int16_t *const *out)
{
	(void)handle;
	(void)in;
	(void)out;
}

void audio_process_drc_eq_free(void *handle)
{
	free(handle);
}

/* ---- DRC stubs (T41 ZRT variant, different API from drc_eq) ---- */

void *audio_process_drc_create(int sample_rate, int channels)
{
	(void)sample_rate;
	(void)channels;
	return calloc(1, sizeof(int));
}

int audio_process_drc_set_config(void *handle, int attack, int release,
				 int threshold, int ratio)
{
	(void)handle;
	(void)attack;
	(void)release;
	(void)threshold;
	(void)ratio;
	return 0;
}

void audio_process_drc_process(void *handle, int16_t *data, int num_samples)
{
	(void)handle;
	(void)data;
	(void)num_samples;
}

void audio_process_drc_free(void *handle)
{
	free(handle);
}
