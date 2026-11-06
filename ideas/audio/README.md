# ideas/audio — waveforms, sound buffers and filters

> **Under review — not part of the polyir standard library.**

Signal processing written in polyir, on the CPU (most functions are `kernel func`s, so they also run inside GPU kernels):

| File | What it is |
|---|---|
| `Wave.poly` | oscillators with a phase in whole cycles (`sine`, `square`, `sawtooth`, `triangle`), `noise`, the envelope `fade` and `noteFrequency` (MIDI key to hertz); it imports `../../core/stdlib/Math.poly` for `sign`, `fract`, `clamp`, `mix` |
| `Sound.poly` | the `Sound` buffer, mono or with any number of channels: `tone`, `add`, `pan`, `scale`, `normalize`, `noise`, `strength`, filters, echo and reverb, WAV writing and reading, `resample` |
| `Biquad.poly` | biquad filters from the Audio EQ Cookbook (low/high-pass, band-pass, notch, bell, shelves) with their exact response curve (`gain`) |
| `Resample.poly` | the windowed-sinc resampler `Sound.resample` uses |
| `Convolve.poly` | convolution as a kernel, one thread per output sample |
| `Fft.poly` | the fast Fourier transform as kernels (Stockham stages, batches, windowed framing) |

`examples/waves.poly` uses the Wave functions. `tests/` holds the checks written in polyir (`WaveTests`, `SoundTests`,
`StereoTests`, `WavTests`, `BiquadTests`, `ResampleTests`) and `tests/fixtures/`: WAV files written byte by byte by
`make-fixtures.py`, which `WavTests` reads from the folder it runs in.

**Needs:** nothing beyond polyir and the C math functions (`sinf`, `floorf`, `powf`, ...): every target that builds the
programs runs them. Playing a sound is `../audio-output`; running the kernels on a GPU is `../gpu`.
