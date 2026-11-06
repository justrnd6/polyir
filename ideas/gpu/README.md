# ideas/gpu — GPU host runtimes and GPU programs

> **Under review — not part of the polyir standard library.** (The language's kernels, `kernel func`, and their CPU side,
> `../../core/stdlib/Gpu.poly`, are part of polyir; what is here is the host side that runs them on a real GPU.)

| File | What it is |
|---|---|
| `Metal.poly` | the Metal host runtime, through the Objective-C runtime (`objc_msgSend`): find the device, compile the `--emit-msl` source, make buffers, launch, wait |
| `Vulkan.poly` | the Vulkan host runtime, over Vulkan's C API: the same for the `--emit-glsl` / `--emit-spirv` shaders |
| `FftGpu.poly` | runs the FFT stages of `../audio/Fft.poly` on the GPU (Metal) |
| `SoundGpu.poly` | sound on the GPU (Metal): convolution reverb and FFT convolution of `../audio` Sounds |

`examples/` holds the GPU programs (each runs its kernels on the GPU and on the CPU and compares them; `*-vulkan.poly`
are the Vulkan versions) and `examples/shared/` the kernels and helpers they share.

**Needs:** a native build (`polyir-check file.poly --out name`; the VM refuses a program that uses a host runtime).
Metal: macOS on Apple Silicon (the CLI adds `-framework Metal`). Vulkan: a Vulkan driver and the loader (on a Mac:
`brew install vulkan-loader molten-vk`) and `glslangValidator` for the GLSL route (`brew install glslang`). A program
exits with 10 when the machine has no such GPU.
