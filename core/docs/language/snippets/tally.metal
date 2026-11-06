#include <metal_stdlib>
using namespace metal;

inline float __atomic_add_float(device float* array, int slot, float amount) {
    device atomic_uint* word = reinterpret_cast<device atomic_uint*>(&array[slot]);
    uint seen = atomic_load_explicit(word, memory_order_relaxed);
    while (true) {
        uint wanted = as_type<uint>(as_type<float>(seen) + amount);
        if (atomic_compare_exchange_weak_explicit(word, &seen, wanted, memory_order_relaxed, memory_order_relaxed)) {
            return as_type<float>(seen);
        }
    }
}

inline int tally_item(int i, device int* counts, device float* sums, device int* lows) {
    atomic_fetch_add_explicit(reinterpret_cast<device atomic_int*>(&counts[(i % 4)]), (int)(1), memory_order_relaxed);
    __atomic_add_float(sums, (i % 4), (float)(0.5));
    atomic_fetch_min_explicit(reinterpret_cast<device atomic_int*>(&lows[(int)(0)]), i, memory_order_relaxed);
    return 0;
}

kernel void tally(device int* counts [[buffer(0)]],
    device float* sums [[buffer(1)]],
    device int* lows [[buffer(2)]],
    device int* __out [[buffer(3)]],
    uint __gid [[thread_position_in_grid]]) {
    __out[__gid] = tally_item((int)__gid, counts, sums, lows);
}

