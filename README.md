# libaudioProcess-neo

Pure C drop-in replacement for Ingenic's proprietary `libaudioProcess.so`.

74KB, single `libc` dependency, covers all Ingenic SoCs (T20, T21, T23, T30, T31, T32, T40, T41, A1, C100).

## What it does

Provides the audio processing pipeline that Ingenic's `libimp.so` loads via `dlopen()` at runtime:

| Module | Description |
|--------|-------------|
| **AEC** | Acoustic Echo Cancellation (WebRTC M47 NLMS adaptive filter) |
| **AGC** | Automatic Gain Control (WebRTC analog + digital) |
| **NS** | Noise Suppression (WebRTC spectral subtraction) |
| **HPF** | High-Pass Filter (Butterworth biquad, 300 Hz cutoff) |
| **LPF** | Low-Pass Filter (first-order IIR) |
| **HS** | Howling Suppression (FFT detection + adaptive notch filters) |
| **DRC** | Dynamic Range Compression (envelope follower) |
| **EQ** | Parametric Equalizer (cascaded biquad filters) |

## Comparison

| | Ingenic original | libaudioProcess-neo |
|---|---|---|
| Size | 510–733 KB | **74 KB** |
| Dependencies | libstdc++ libm libgcc_s libc | **libc** |
| Exported symbols | 1574 | **31** |
| Language | C++ wrapping C | **Pure C11** |
| Platforms | per-SoC builds | **single binary** |
| License | proprietary | **MIT** (WebRTC: BSD-3) |

## Building

Requires a MIPS32r2 cross-compiler (uclibc or glibc):

```sh
make CROSS_COMPILE=mipsel-linux-
```

Produces:
- `libaudioProcess.so` — shared library (drop-in replacement)
- `libaudioProcess.a` — static library (for linking into other projects)

Override the compiler prefix for your toolchain:

```sh
make CROSS_COMPILE=/path/to/mipsel-thingino-linux-uclibc_sdk-buildroot/bin/mipsel-linux-
```

## Installation

Copy `libaudioProcess.so` to `/usr/lib/` on the device, replacing the Ingenic original:

```sh
cp libaudioProcess.so /usr/lib/libaudioProcess.so
```

Or test without replacing by using `LD_LIBRARY_PATH`:

```sh
LD_LIBRARY_PATH=/path/to/dir/containing/lib your_application
```

## Configuration

Reads `webrtc_profile.ini` when AEC is enabled (path passed by `libimp`). Supported settings:

- `[Set_Far_Frame]` / `[Set_Near_Frame]` — gain scaling (`Frame_V`), delay
- `[AEC]` — NLP mode, drift compensation, metrics, delay logging
- Standard Ingenic INI format used across all SDK versions

## Project structure

```
src/
  audio_process.h          Public API (31 exported symbols)
  aec.c                    AEC wrapper
  agc.c                    AGC wrapper
  ns.c                     NS wrapper
  hpf.c                    High-pass filter
  lpf.c                    Low-pass filter
  howling.c                Howling detection/suppression
  drc.c                    Dynamic range compression
  eq.c                     Parametric equalizer
  drc_eq.c                 Combined DRC+EQ API (T32)
  biquad.{c,h}             Shared biquad filter
  config.{c,h}             INI config parser
  util.h                   Shared utilities
  libaudioProcess.map      Linker version script
  webrtc/                  Vendored WebRTC M47 sources (BSD-3)
```

## License

- Our code: [MIT](LICENSE)
- Vendored WebRTC sources: [BSD-3-Clause](src/webrtc/LICENSE) with [patent grant](src/webrtc/PATENTS)
