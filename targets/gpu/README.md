# libpolyir-lang-targets: the GPU back ends

Metal, GLSL and SPIR-V from the same HIR (`gpu/`, `include/polyir/Kernels.hpp`).

## GPU output

`polyir-check f.poly --emit-msl | --emit-glsl | --emit-spirv` (and `polyir-hir f.json --msl | --glsl | --spirv`) write Metal, GLSL
(the graphics stages, or one Vulkan compute shader per kernel under `// ==== name.comp ====`), and SPIR-V modules written straight
from the HIR (no GLSL, no glslang) as hex words under `// ==== name.spv ====` (`xxd -r -p` makes the `.spv`). The writers began
as ports of the Nina ones, and `tests/data/kernels/` keeps the sizes and hashes of what those wrote for every kernel example (and
their text for the kernels `KernelTest` makes by hand): `KernelTest` checks that the C++ writes the same.
