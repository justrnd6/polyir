// ==== blockSum.comp ====
#version 450
#extension GL_EXT_shader_explicit_arithmetic_types : require
layout(local_size_x = 256) in;

layout(push_constant) uniform Params {
    uint gpu_count;
    int n;
};
layout(std430, set = 0, binding = 0) readonly buffer src_block { float src[]; };
layout(std430, set = 0, binding = 1)  buffer partial_block { float partial[]; };
layout(std430, set = 0, binding = 2) writeonly buffer gpu_out_block { int gpu_out[]; };
shared float tile[256];
void gpu_barrier() {
    memoryBarrierShared();
    barrier();
}

int blockSum_item(int i, int n) {
    const int lane = int((i % 256));
    if ((i < n)) {
        tile[int(lane)] = float(src[int(i)]);
    } else {
        tile[int(lane)] = float(0.0);
    }
    gpu_barrier();
    {
        int s = (256 / int(2));
        while ((s > int(0))) {
            if ((lane < s)) {
                tile[int(lane)] = float((tile[int(lane)] + tile[int((lane + s))]));
            }
            gpu_barrier();
            int s_next = (s / int(2));
            s = s_next;
        }
    }
    if ((lane == int(0))) {
        partial[int((i / 256))] = float(tile[int(0)]);
    }
    return 0;
}

void main() {
    uint gpu_gid = gl_GlobalInvocationID.x;
    int gpu_result = blockSum_item(int(gpu_gid), n);
    if (gpu_gid < gpu_count) { gpu_out[gpu_gid] = gpu_result; }
}

