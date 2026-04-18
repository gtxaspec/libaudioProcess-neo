#include <stdlib.h>
#include <stdint.h>

#include "audio_process.h"
#include "audio_process_internal.h"
#include "util.h"
#include "webrtc/modules/audio_processing/aec/include/echo_cancellation.h"
#include "webrtc/modules/audio_processing/aec/aec_core.h"

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

		for (int j = 0; j < samples_10ms; j++)
			near_ptr[j] = clamp_i16(out_f[j]);
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
