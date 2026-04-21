/* SPDX-License-Identifier: MIT */
#include <stdint.h>
#include <math.h>
#include <string.h>

#include "audio_process.h"
#include "biquad.h"
#include "util.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"

/*
 * HPF: second-order Butterworth high-pass filter.
 *
 * libimp manages the state buffer (int16_t[16]) and passes it to us.
 * The original Ingenic implementation used Q13 fixed-point arithmetic
 * with a coefficient pointer embedded at state[6]. Instead, we store
 * a float biquad directly in the state buffer (28 bytes fits in the
 * 32-byte allocation libimp makes).
 *
 * The biquad is designed on first call based on a 300 Hz cutoff
 * (standard for speech HPF, removes DC offset and low-frequency rumble).
 */

#define HPF_MAGIC 0x4850  /* "HP" in the first int16_t */
#define HPF_CUTOFF_HZ 300

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

	struct biquad *bq = (struct biquad *)state;

	/* detect uninitialized state and design the filter */
	if (bq->b0 == 0.0f) {
		float fs = 16000.0f;
		float fc = (float)HPF_CUTOFF_HZ;
		float w0 = 2.0f * (float)M_PI * fc / fs;
		float cw = cosf(w0);
		float alpha = sinf(w0) / (2.0f * 0.7071f); /* Q = 1/sqrt(2) Butterworth */

		float a0_inv = 1.0f / (1.0f + alpha);
		bq->b0 = ((1.0f + cw) / 2.0f) * a0_inv;
		bq->b1 = -(1.0f + cw) * a0_inv;
		bq->b2 = bq->b0;
		bq->a1 = (-2.0f * cw) * a0_inv;
		bq->a2 = (1.0f - alpha) * a0_inv;
		bq->z1 = 0.0f;
		bq->z2 = 0.0f;
	}

	for (int i = 0; i < num_samples; i++)
		data[i] = biquad_process_i16(bq, data[i]);

	return 0;
}

void audio_process_hpf_free(void)
{
}
