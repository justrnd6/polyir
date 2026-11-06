#!/bin/bash
# Rebuilds the pictures and the program output that the guide shows, from the REAL programs
# (so nothing in the guide is typed in by hand). Run from anywhere; needs the built CLI (build/polyir-check, or
# $POLYIR_CHECK) and, for the sound examples, nothing else (they run silently).
#   docs/snippets/regenerate.sh && python3 docs/build.py
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"   # core/; the examples it runs are in ../ideas, the polyir-check command in ../build
CLI="${POLYIR_CHECK:-$ROOT/../build/polyir-check}"
work="$(mktemp -d)"

# The studio program: stereo, WAV files, resampling and equalizers. It prints a report and draws three response curves.
(cd "$ROOT/../ideas/audio-output/examples" && "$CLI" studio.poly --out "$work/studio" > /dev/null)
(cd "$work" && ./studio > "$HERE/studio.txt")
cp "$work"/studio-eq.pgm "$work"/studio-hum.pgm "$work"/studio-phone.pgm "$HERE/pics/"

# The live program: a stream whose sound is made while it plays (the speakers run at volume 0).
(cd "$ROOT/../ideas/audio-output/examples" && "$CLI" live.poly --out "$work/live" > /dev/null)
(cd "$work" && ./live > "$HERE/live.txt")
# The kernel-shape examples: pictures and timings (they need a Metal GPU; skip these lines on a machine without one).
for example in shapes zbuffer reduce1; do
  (cd "$ROOT/../ideas/gpu/examples" && "$CLI" --emit-msl "$example.poly" > "$work/$example.metal" && "$CLI" "$example.poly" --out "$work/$example" > /dev/null)
  (cd "$work" && ./"$example" > "$HERE/$example.txt" 2>/dev/null || true)
done
cp "$work"/shapes-plasma.pgm "$work"/shapes-vignette.pgm "$work"/zbuffer.pgm "$HERE/pics/" 2>/dev/null || true

# The NTSC television example (needs a Metal GPU): five pictures of the encode/decode round trip.
(cd "$ROOT/../ideas/gpu/examples" && "$CLI" --emit-msl ntsc.poly > "$work/ntsc.metal" && "$CLI" ntsc.poly --out "$work/ntsc" > /dev/null)
(cd "$work" && ./ntsc && cp ntsc-*.ppm ntsc-*.pgm "$HERE/pics/") 2>/dev/null || true

# The colour-as-a-signal example (needs a Metal GPU): nine pictures and the vectorscopes.
(cd "$ROOT/../ideas/gpu/examples" && "$CLI" --emit-msl colour.poly > "$work/colour.metal" && "$CLI" colour.poly --out "$work/colour" > /dev/null)
(cd "$work" && ./colour && cp colour-*.ppm colour-*.pgm "$HERE/pics/") 2>/dev/null || true

# The images-as-signals example (needs a Metal GPU): the table of pixel numbers and the pictures.
(cd "$ROOT/../ideas/gpu/examples" && "$CLI" --emit-msl pixels.poly > "$work/pixels.metal" && "$CLI" pixels.poly --out "$work/pixels" > /dev/null)
(cd "$work" && ./pixels > "$HERE/pixels-table.txt" && cp pixels-*.ppm pixels-scan.pgm "$HERE/pics/") 2>/dev/null || true

# The workbook of classic problems (needs a Metal GPU): the timing table.
(cd "$ROOT/../ideas/gpu/examples" && "$CLI" --emit-msl classics.poly > "$work/classics.metal" && "$CLI" classics.poly --out "$work/classics" > /dev/null)
(cd "$work" && ./classics > "$HERE/classics-metal.txt") 2>/dev/null || true

# The multi-device Vulkan example (needs glslangValidator and a Vulkan driver): the report it prints.
if command -v glslangValidator > /dev/null; then
  mkdir -p "$work/mgpu"
  (cd "$ROOT/../ideas/gpu/examples" && "$CLI" --emit-glsl multigpu-vulkan.poly | awk -v d="$work/mgpu" '/^\/\/ ==== /{f=d"/"$3} f{print > f}')
  for shader in "$work"/mgpu/*.comp; do glslangValidator -V "$shader" -o "${shader%.comp}.spv" > /dev/null; done
  (cd "$ROOT/../ideas/gpu/examples" && "$CLI" multigpu-vulkan.poly --out "$work/mgpu/run" > /dev/null)
  (cd "$work/mgpu" && ./run > "$HERE/multigpu-vulkan.txt") 2>/dev/null || true
fi

# The left- versus right-handed z example (needs glslangValidator and a Vulkan driver): the report and the F pictures it prints.
if command -v glslangValidator > /dev/null; then
  mkdir -p "$work/handed"
  (cd "$ROOT/../ideas/gpu/examples" && "$CLI" --emit-glsl handed-vulkan.poly | awk -v d="$work/handed" '/^\/\/ ==== /{f=d"/"$3} f{print > f}')
  for shader in "$work"/handed/*.comp; do glslangValidator -V "$shader" -o "${shader%.comp}.spv" > /dev/null; done
  (cd "$ROOT/../ideas/gpu/examples" && "$CLI" handed-vulkan.poly --out "$work/handed/run" > /dev/null)
  (cd "$work/handed" && ./run > "$HERE/handed-vulkan.txt") 2>/dev/null || true
fi

# The camera-from-scratch example (needs a Metal GPU): three pictures and the report it prints.
(cd "$ROOT/../ideas/gpu/examples" && "$CLI" --emit-msl camera.poly > "$work/camera.metal" && "$CLI" camera.poly --out "$work/camera" > /dev/null)
(cd "$work" && ./camera > "$HERE/camera-metal.txt" && cp camera-*.pgm "$HERE/pics/") 2>/dev/null || true
sed -n '/^a point straight ahead/,/ceiling/p' "$HERE/camera-metal.txt" > "$HERE/camera-turning.txt"

# What the compiler writes for the three small kernels the guide shows (Metal and GLSL).
for kernel in grid tally blocksum scale; do
  "$CLI" --emit-msl "$HERE/$kernel.poly" > "$HERE/$kernel.metal"
  "$CLI" --emit-glsl "$HERE/$kernel.poly" > "$HERE/$kernel.glsl"
done

# The direct SPIR-V writer: the module for the smallest kernel, as the disassembler shows it (needs xxd and spirv-dis).
if command -v xxd > /dev/null && command -v spirv-dis > /dev/null; then
  (cd "$work" && "$CLI" --emit-spirv "$HERE/scale.poly" | awk '/^\/\/ ==== /{next} {print}' | xxd -r -p > scale.spv && spirv-dis scale.spv > "$HERE/scale.spvasm")
fi

rm -rf "$work"

# What the compiler says about mistakes: in one file, and across files (each error names its file).
(cd "$HERE" && "$CLI" --check errors.poly > errors.txt || true)
(cd "$ROOT/tests/programs" && "$CLI" --check fixtures/diagnostics/main.poly > "$HERE/errors-files.txt" || true)

# The diagrams that are plots of exact formulas (the panning law, the windowed sinc, ...).
python3 "$HERE/make-figures.py"
echo "regenerated studio.txt, live.txt and three pictures in $HERE"
