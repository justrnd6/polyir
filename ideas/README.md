# ideas — domain libraries that showcase polyir

> **Under review — not part of the polyir standard library.** These libraries, their examples and their tests were
> moved here from `core/` so the standard library (`../core/stdlib`: Math, Gpu, Text) stays small. Whether each one
> becomes standard, moves elsewhere or goes away is still to be decided.

Everything here is plain polyir. A program uses a library by importing it (nothing here is built into the compilers);
the libraries use the standard library's built-in `sign`, `fract`, `clamp` and `mix` (`../core/stdlib/Math.poly`).

| Folder | What it is | Needs |
|---|---|---|
| [`audio/`](audio/README.md) | waveforms (`Wave`), the `Sound` buffer with WAV files, biquad filters, resampling, convolution and the FFT kernels | nothing beyond polyir: runs on every target that has the C math functions |
| [`gpu/`](gpu/README.md) | the Metal and Vulkan host runtimes, GPU sound (convolution, FFT) and the GPU example programs | Metal: macOS on Apple Silicon; Vulkan: a Vulkan driver and loader (MoltenVK on a Mac) and `glslangValidator` |
| [`audio-output/`](audio-output/README.md) | playing sound on the speakers, finished or as it is made (a fill function) | the operating system's audio API: macOS AudioQueue (tested); Linux ALSA and Windows waveOut (written, untested) |

Each folder holds the library files, `examples/` and `tests/`. Who checks them:

- `../runtime/tests/vm_vs_native.sh` runs `audio/examples` and every `*/tests` on the VM and as a native build;
- the example programs of every `*/examples` are in the build's example list (`front_examples.txt`: FrontParseTest, PbcTest,
  HirJsonWriteTest), and every file here is translated to each target by TargetGoldenTest (`../targets/<lang>/tests/ideas`).
