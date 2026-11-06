// ==== scale.comp ====
#version 450
#extension GL_EXT_shader_explicit_arithmetic_types : require
layout(local_size_x = 64) in;

layout(push_constant) uniform Params {
    uint gpu_count;
    float k;
};
layout(std430, set = 0, binding = 0) readonly buffer x_in_block { float x_in[]; };
layout(std430, set = 0, binding = 1) writeonly buffer gpu_out_block { float gpu_out[]; };

float scale_item(float x, float k) {
    return ((x * k) + 1.0);
}

void main() {
    uint gpu_gid = gl_GlobalInvocationID.x;
    if (gpu_gid >= gpu_count) { return; }
    gpu_out[gpu_gid] = scale_item(x_in[gpu_gid], k);
}

// ==== smooth.comp ====
#version 450
#extension GL_EXT_shader_explicit_arithmetic_types : require
layout(local_size_x = 64) in;

layout(push_constant) uniform Params {
    uint gpu_count;
    int n;
};
layout(std430, set = 0, binding = 0) readonly buffer src_block { float src[]; };
layout(std430, set = 0, binding = 1) writeonly buffer gpu_out_block { float gpu_out[]; };

float smooth_item(int i, int n) {
    if ((i == int(0))) {
        return src[int(i)];
    }
    if ((i == (n - int(1)))) {
        return src[int(i)];
    }
    return (((src[int((i - int(1)))] + src[int(i)]) + src[int((i + int(1)))]) / 3.0);
}

void main() {
    uint gpu_gid = gl_GlobalInvocationID.x;
    if (gpu_gid >= gpu_count) { return; }
    gpu_out[gpu_gid] = smooth_item(int(gpu_gid), n);
}

