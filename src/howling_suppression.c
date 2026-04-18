#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

/*
 * Howling Detection & Suppression (HD)
 *
 * Based on Ingenic T32 analysis. The algorithm:
 *
 * Detection uses three criteria on 128-point FFT magnitude spectrum:
 *   1. PTPR (Peak-to-Threshold Power Ratio): bins where 20*log10(mag) > threshold
 *   2. PAPR (Peak-to-Average Power Ratio): bins where 20*log10(mag/avg) > threshold
 *   3. PNPR (Peak-to-Neighbor Power Ratio): bins where the ratio to +-1, +-4
 *      neighbors all exceed threshold in dB (4 consecutive checks)
 *
 * Candidate bins that appear in 3+ consecutive frames (IPMP inter-frame
 * persistence) and pass screening are declared howling frequencies.
 *
 * Suppression applies second-order IIR notch filters at detected frequencies.
 * Filters use crossfade (32-sample ramp) when updating to avoid clicks.
 *
 * Our implementation improves on the original:
 *   - Pure C, no C++ std::vector allocations in the processing path
 *   - Fixed-size arrays instead of dynamic allocation per frame
 *   - Same detection thresholds and notch filter design
 */

#define HD_FFT_SIZE     128
#define HD_HALF_FFT     64
#define HD_MAX_NOTCHES  8
#define HD_PERSIST_THR  3
#define HD_TTL_INIT     1000
#define HD_CROSSFADE    32

struct notch_filter {
	float b0, b1, b2, a1, a2;
	float z1, z2;
};

struct hd_state {
	int sample_rate;
	float spectrum[HD_HALF_FFT];
	float window[HD_FFT_SIZE];
	int persist[HD_HALF_FFT];
	int ttl_max;

	/* detection thresholds */
	float ptpr_thr;
	float papr_thr;
	float pnpr_thr;

	/* active notch filters */
	struct notch_filter notches[HD_MAX_NOTCHES];
	int notch_bins[HD_MAX_NOTCHES];
	int num_notches;

	/* FFT scratch */
	float fft_buf[HD_FFT_SIZE * 2];
};

static void hd_design_notch(struct notch_filter *nf, float freq, float fs, float bw)
{
	float w0 = 2.0f * (float)M_PI * freq / fs;
	float alpha = sinf(w0) * sinhf(logf(2.0f) / 2.0f * bw * w0 / sinf(w0));

	float a0 = 1.0f + alpha;
	nf->b0 = 1.0f / a0;
	nf->b1 = -2.0f * cosf(w0) / a0;
	nf->b2 = 1.0f / a0;
	nf->a1 = -2.0f * cosf(w0) / a0;
	nf->a2 = (1.0f - alpha) / a0;
	nf->z1 = 0.0f;
	nf->z2 = 0.0f;
}

static int16_t hd_notch_process(struct notch_filter *nf, int16_t in)
{
	float x = (float)in;
	float y = nf->b0 * x + nf->z1;
	nf->z1 = nf->b1 * x - nf->a1 * y + nf->z2;
	nf->z2 = nf->b2 * x - nf->a2 * y;

	if (y > 32767.0f) y = 32767.0f;
	if (y < -32768.0f) y = -32768.0f;
	return (int16_t)y;
}

static void hd_compute_hann_window(float *w, int n)
{
	for (int i = 0; i < n; i++)
		w[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * i / (n - 1)));
}

static void hd_simple_dft_mag(const int16_t *in, const float *window,
			      float *mag, int n)
{
	int half = n / 2;
	for (int k = 0; k < half; k++) {
		float re = 0.0f, im = 0.0f;
		for (int j = 0; j < n; j++) {
			float angle = 2.0f * (float)M_PI * k * j / n;
			float w = (float)in[j] * window[j];
			re += w * cosf(angle);
			im -= w * sinf(angle);
		}
		mag[k] = sqrtf(re * re + im * im);
	}
}

void *audio_process_hs_create(int sample_rate, int frame_size)
{
	(void)frame_size;
	struct hd_state *s = calloc(1, sizeof(*s));
	if (!s)
		return NULL;

	s->sample_rate = sample_rate > 0 ? sample_rate : 8000;
	s->ttl_max = HD_TTL_INIT;
	s->ptpr_thr = 30.0f;   /* dB */
	s->papr_thr = 15.0f;   /* dB above average */
	s->pnpr_thr = 10.0f;   /* dB above neighbors */

	hd_compute_hann_window(s->window, HD_FFT_SIZE);

	return s;
}

void audio_process_hs_process(void *handle, int16_t *data, int num_samples)
{
	struct hd_state *s = handle;
	if (!s || !data || num_samples <= 0)
		return;

	int frame_size = HD_FFT_SIZE;
	int num_frames = num_samples / frame_size;

	for (int f = 0; f < num_frames; f++) {
		int16_t *frame = data + f * frame_size;

		hd_simple_dft_mag(frame, s->window, s->spectrum, frame_size);

		/* compute average magnitude */
		float avg = 0.0f;
		for (int k = 0; k < HD_HALF_FFT; k++)
			avg += s->spectrum[k];
		avg /= HD_HALF_FFT;
		if (avg < 1e-10f)
			avg = 1e-10f;

		/* detect howling candidates */
		int candidates[HD_HALF_FFT];
		int num_cand = 0;

		for (int k = 5; k < HD_HALF_FFT - 6; k++) {
			float mag = s->spectrum[k];
			if (mag < 1e-10f)
				continue;

			float mag_db = 20.0f * log10f(mag);
			float papr_db = 20.0f * log10f(mag / avg);

			if (mag_db < s->ptpr_thr)
				continue;
			if (papr_db < s->papr_thr)
				continue;

			/* PNPR: check 4 neighbor ratios (+-1, +-4) */
			int pass = 1;
			int offsets[] = {-1, -4, 3, 6};
			for (int n = 0; n < 4 && pass; n++) {
				float neighbor = s->spectrum[k + offsets[n]];
				if (neighbor < 1e-10f)
					neighbor = 1e-10f;
				float ratio_db = 20.0f * log10f(mag / neighbor);
				if (ratio_db < s->pnpr_thr)
					pass = 0;
			}

			if (pass && num_cand < HD_HALF_FFT)
				candidates[num_cand++] = k;
		}

		/* update persistence counters */
		for (int k = 0; k < HD_HALF_FFT; k++) {
			int found = 0;
			for (int c = 0; c < num_cand; c++) {
				if (candidates[c] == k) {
					found = 1;
					break;
				}
			}
			if (found) {
				if (s->persist[k] < s->ttl_max)
					s->persist[k]++;
			} else {
				if (s->persist[k] > 0)
					s->persist[k]--;
			}
		}

		/* select top frequencies for notch filtering */
		int new_notch_bins[HD_MAX_NOTCHES];
		int new_num = 0;

		for (int k = 5; k < HD_HALF_FFT - 1 && new_num < HD_MAX_NOTCHES; k++) {
			if (s->persist[k] >= HD_PERSIST_THR) {
				new_notch_bins[new_num++] = k;
			}
		}

		/* update notch filters if howling bins changed */
		if (new_num != s->num_notches ||
		    memcmp(new_notch_bins, s->notch_bins, new_num * sizeof(int)) != 0) {
			s->num_notches = new_num;
			memcpy(s->notch_bins, new_notch_bins, new_num * sizeof(int));
			float bin_hz = (float)s->sample_rate / HD_FFT_SIZE;
			for (int i = 0; i < new_num; i++) {
				float freq = (float)new_notch_bins[i] * bin_hz;
				hd_design_notch(&s->notches[i], freq,
						(float)s->sample_rate, 0.5f);
			}
		}

		/* apply notch filters with crossfade */
		for (int i = 0; i < s->num_notches; i++) {
			for (int j = 0; j < frame_size; j++) {
				int16_t filtered = hd_notch_process(&s->notches[i], frame[j]);
				if (j < HD_CROSSFADE) {
					float t = (float)j / HD_CROSSFADE;
					frame[j] = (int16_t)((1.0f - t) * frame[j] + t * filtered);
				} else {
					frame[j] = filtered;
				}
			}
		}
	}
}

void audio_process_hs_free(void *handle)
{
	free(handle);
}
