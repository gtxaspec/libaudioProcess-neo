#ifndef AUDIO_PROCESS_UTIL_H
#define AUDIO_PROCESS_UTIL_H

#include <stdint.h>

/* Q13 fixed-point constants (used by HPF) */
#define Q13_ONE       0x2000
#define Q13_ROUND     0x800
#define Q12_SAT_MAX   0x7FFFFFF
#define Q12_SAT_MIN   (-0x8000000)

static inline int16_t clamp_i16(float v)
{
	if (v > 32767.0f)
		return 32767;
	if (v < -32768.0f)
		return -32768;
	return (int16_t)v;
}

static inline float clamp_f(float v, float lo, float hi)
{
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

#endif
