#include "audio_process.h"
#include "webrtc/modules/audio_processing/agc/legacy/gain_control.h"

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
