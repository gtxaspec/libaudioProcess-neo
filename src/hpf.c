#include <stdint.h>

#include "audio_process.h"
#include "util.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"

/*
 * State layout (int16_t[8], managed by libimp):
 *   [0]   y[n-1]      previous output
 *   [1]   residual     fractional feedback accumulator
 *   [2]   y[n-2]       feedback delay
 *   [3]   y[n-3]       feedback delay
 *   [4]   x[n-1]       previous input
 *   [5]   x[n-2]       previous input
 *   [6-7] coeffs_ptr   pointer to int16_t[5] Q13 coefficients
 *
 * Coefficients (Q13): b0, b1, b2, a1, a2
 * All arithmetic is Q13 fixed-point with Q12 output rounding.
 */

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

		int32_t rounded = acc + Q13_ROUND;
		int16_t y0 = (int16_t)(acc >> 13);

		if (rounded < Q12_SAT_MIN)
			rounded = Q12_SAT_MIN;
		if (rounded > Q12_SAT_MAX)
			rounded = Q12_SAT_MAX;

		state[0] = y0;
		state[1] = ((int16_t)acc + y0 * -Q13_ONE) * 4;
		data[i] = (int16_t)(rounded >> 12);
	}

	return 0;
}

void audio_process_hpf_free(void)
{
}
