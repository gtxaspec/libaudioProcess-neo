CROSS_COMPILE ?= mipsel-linux-

CC      = $(CROSS_COMPILE)gcc
STRIP   = $(CROSS_COMPILE)strip

CFLAGS  = -O2 -Wall -fPIC -DMIPS_FPU_LE -DMIPS32_LE -DWEBRTC_POSIX -Isrc
LDFLAGS = -shared -lm -lpthread

TARGET  = libaudioProcess.so

SRCDIR  = src

# WebRTC AEC
AEC_SRC = $(addprefix $(SRCDIR)/webrtc/modules/audio_processing/aec/, \
	aec_core.c aec_core_mips.c echo_cancellation.c \
	aec_rdft.c aec_rdft_mips.c aec_resampler.c)

# WebRTC AGC
AGC_SRC = $(addprefix $(SRCDIR)/webrtc/modules/audio_processing/agc/legacy/, \
	analog_agc.c digital_agc.c)

# WebRTC NS
NS_SRC = $(addprefix $(SRCDIR)/webrtc/modules/audio_processing/ns/, \
	ns_core.c noise_suppression.c)

# WebRTC utility
UTIL_SRC = $(addprefix $(SRCDIR)/webrtc/modules/audio_processing/utility/, \
	delay_estimator.c delay_estimator_wrapper.c)

# WebRTC common audio
COMMON_SRC = $(addprefix $(SRCDIR)/webrtc/common_audio/, \
	ring_buffer.c fft4g.c)

# WebRTC SPL
SPL_SRC = $(addprefix $(SRCDIR)/webrtc/common_audio/signal_processing/, \
	auto_correlation.c auto_corr_to_refl_coef.c \
	complex_bit_reverse.c complex_fft.c \
	copy_set_operations.c cross_correlation.c cross_correlation_mips.c \
	division_operations.c dot_product_with_scale.c \
	downsample_fast.c \
	energy.c filter_ar.c filter_ar_fast_q12.c \
	filter_ma_fast_q12.c get_hanning_window.c get_scaling_square.c \
	levinson_durbin.c \
	lpc_to_refl_coef.c min_max_operations.c min_max_operations_mips.c \
	randomization_functions.c real_fft.c refl_coef_to_lpc.c \
	resample.c resample_48khz.c resample_by_2.c resample_by_2_internal.c \
	resample_fractional.c \
	spl_init.c spl_sqrt.c spl_sqrt_floor_mips.c \
	splitting_filter.c sqrt_of_one_minus_x_squared.c \
	vector_scaling_operations.c)

# WebRTC VAD
VAD_SRC = $(addprefix $(SRCDIR)/webrtc/common_audio/vad/, \
	webrtc_vad.c vad_core.c vad_filterbank.c vad_gmm.c vad_sp.c)

# Our wrapper
WRAPPER_SRC = $(SRCDIR)/audio_process.c

ALL_SRC = $(WRAPPER_SRC) $(AEC_SRC) $(AGC_SRC) $(NS_SRC) $(UTIL_SRC) \
	  $(COMMON_SRC) $(SPL_SRC) $(VAD_SRC)

OBJS = $(ALL_SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^
	$(STRIP) $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

size: $(TARGET)
	ls -lh $(TARGET)
	readelf -d $(TARGET) | grep NEEDED

.PHONY: all clean size
