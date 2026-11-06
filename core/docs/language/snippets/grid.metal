#include <metal_stdlib>
using namespace metal;

inline int cell_item(int x, int y, int width) {
    return ((y * width) + x);
}

kernel void cell(constant int& width [[buffer(0)]],
    device int* __out [[buffer(1)]],
    uint2 __gid [[thread_position_in_grid]],
    uint2 __grid [[threads_per_grid]]) {
    __out[(__gid.y * __grid.x + __gid.x)] = cell_item((int)__gid.x, (int)__gid.y, width);
}

