#include <metal_stdlib>
using namespace metal;

inline int blockSum_item(int i, device const float* src, int n, threadgroup float* tile, device float* partial) {
    const int lane = (i % 256);
    if ((i < n)) {
        tile[lane] = src[i];
    } else {
        tile[lane] = 0.0;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    {
        int s = (256 / 2);
        while ((s > 0)) {
            if ((lane < s)) {
                tile[lane] = (tile[lane] + tile[(lane + s)]);
            }
            threadgroup_barrier(mem_flags::mem_threadgroup);
            int s_next = (s / 2);
            s = s_next;
        }
    }
    if ((lane == 0)) {
        partial[(i / 256)] = tile[0];
    }
    return 0;
}

kernel void blockSum(device const float* src [[buffer(0)]],
    constant int& n [[buffer(1)]],
    device float* partial [[buffer(2)]],
    device int* __out [[buffer(3)]],
    uint __gid [[thread_position_in_grid]]) {
    threadgroup float tile[256];
    __out[__gid] = blockSum_item((int)__gid, src, n, tile, partial);
}

