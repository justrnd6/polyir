# core/stdlib — the polyir standard library

The standard library is four small libraries written in polyir. All four are **built into the compiler**: a program
calls their functions without an import, and the compiler adds what it uses (and only that) when the check reports a call
to one of them as unknown (`../src/front/Prelude.cpp`). A program that imports the file,
or defines a function of the same name, is left alone.

| File | What it holds |
|---|---|
| `Math.poly` | `sign`, `fract`, `clamp`, `mix`: small `kernel func`s on `float`s (they run on the CPU and inside GPU kernels) |
| `Gpu.poly` | the CPU side of the functions a kernel uses to cooperate: `atomicAdd`, `atomicAddFloat`, `atomicMin`, `atomicMax`, `groupSize`, `localIndex`, `groupIndex`, `barrier` |
| `Text.poly` | working with strings: `contains`, `startsWith`, `endsWith`, `split`, `lines`, `join`, `trim`, `replace`, `repeat`, `indent`, `indentRest`, `fill`, and `StringBuilder` |
| `Lists.poly` | working with arrays and mutarrays: `first`, `last`, `drop` (generic, and each reads as a method: `xs.drop(1)`); a `match` list pattern's `...rest` is `drop` |
| `sync-prelude.py` | copies the four files into the compiler (`../src/front/PreludeText.cpp`); run it after editing any of them (`python3 core/stdlib/sync-prelude.py`) |

Their tests are `../tests/programs/PreludeTests.poly` (Math), `GpuTests.poly`, `StringTests.poly` and `ListTests.poly`.

What is not here: the domain libraries that showcase polyir (waveforms, sound buffers, filters, the FFT, the Metal and
Vulkan host runtimes, audio output) are under review in [`../../ideas`](../../ideas/README.md) and are not part of the
standard library; meta-build's host library is `../../tools/metabuild/src/Host.poly`.
