#include <metal_stdlib>
using namespace metal;

inline float scale_item(float x, float k) {
    return ((x * k) + 1.0);
}

kernel void scale(device const float* x_in [[buffer(0)]],
    constant float& k [[buffer(1)]],
    device float* __out [[buffer(2)]],
    uint __gid [[thread_position_in_grid]]) {
    __out[__gid] = scale_item(x_in[__gid], k);
}

