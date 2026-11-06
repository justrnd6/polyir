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

inline float smooth_item(int i, device const float* src, int n) {
    if ((i == 0)) {
        return src[i];
    }
    if ((i == (n - 1))) {
        return src[i];
    }
    return (((src[(i - 1)] + src[i]) + src[(i + 1)]) / 3.0);
}

kernel void smooth(device const float* src [[buffer(0)]],
    constant int& n [[buffer(1)]],
    device float* __out [[buffer(2)]],
    uint __gid [[thread_position_in_grid]]) {
    __out[__gid] = smooth_item((int)__gid, src, n);
}

