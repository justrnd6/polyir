// ==== tally.comp ====
#version 450
#extension GL_EXT_shader_explicit_arithmetic_types : require
layout(local_size_x = 64) in;

layout(push_constant) uniform Params {
    uint gpu_count;
};
layout(std430, set = 0, binding = 0)  buffer counts_block { int counts[]; };
layout(std430, set = 0, binding = 1)  buffer sums_block { float sums[]; };
layout(std430, set = 0, binding = 1) buffer sums_bits_block { uint sums_bits[]; };
float gpu_add_float_sums(int slot, float amount) {
    uint seen = sums_bits[slot];
    for (int tries = 0; tries < 1000000; tries = tries + 1) {
        uint wanted = floatBitsToUint(uintBitsToFloat(seen) + amount);
        uint previous = atomicCompSwap(sums_bits[slot], seen, wanted);
        if (previous == seen) { return uintBitsToFloat(previous); }
        seen = previous;
    }
    return 0.0;
}
layout(std430, set = 0, binding = 2)  buffer lows_block { int lows[]; };
layout(std430, set = 0, binding = 3) writeonly buffer gpu_out_block { int gpu_out[]; };

int tally_item(int i) {
    atomicAdd(counts[int((i % int(4)))], int(1));
    gpu_add_float_sums(int((i % int(4))), float(0.5));
    atomicMin(lows[int(int(0))], i);
    return 0;
}

void main() {
    uint gpu_gid = gl_GlobalInvocationID.x;
    if (gpu_gid >= gpu_count) { return; }
    gpu_out[gpu_gid] = tally_item(int(gpu_gid));
}

