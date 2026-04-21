/* SPDX-License-Identifier: MIT */
#ifndef AUDIO_PROCESS_INTERNAL_H
#define AUDIO_PROCESS_INTERNAL_H

#include <stdint.h>

#define AEC_MAGIC        0x61656330  /* "aec0" */
#define MAX_SAMPLES_10MS 480         /* 48kHz * 10ms */

struct aec_handle {
	uint32_t magic;
	int32_t sample_rate;
	void *aec;
	float far_gain;
	float near_gain;
};

struct aec_frame {
	const int16_t *far;
	int16_t *near;
	int reserved;
	int num_bytes;
};

#endif
