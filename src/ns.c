/* SPDX-License-Identifier: MIT */
#include "audio_process.h"
#include "webrtc/modules/audio_processing/ns/include/noise_suppression.h"

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
