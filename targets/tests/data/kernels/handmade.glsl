// ==== wrap.comp ====
#version 450
#extension GL_EXT_shader_explicit_arithmetic_types : require
layout(local_size_x = 64) in;

layout(push_constant) uniform Params {
    uint gpu_count;
    float k;
};
layout(std430, set = 0, binding = 0) readonly buffer x_in_block { float x_in[]; };
layout(std430, set = 0, binding = 1) writeonly buffer gpu_out_block { float gpu_out[]; };

float gpu_fmod(float x, float y) {
    float ax = abs(x);
    float ay = abs(y);
    float r = ax - floor(ax / ay) * ay;
    r = (r < 0.0) ? r + ay : r;
    r = (r >= ay) ? r - ay : r;
    return (x < 0.0) ? -r : r;
}
vec2 gpu_fmod(vec2 x, vec2 y) { return vec2(gpu_fmod(x.x, y.x), gpu_fmod(x.y, y.y)); }
vec3 gpu_fmod(vec3 x, vec3 y) { return vec3(gpu_fmod(x.x, y.x), gpu_fmod(x.y, y.y), gpu_fmod(x.z, y.z)); }
vec4 gpu_fmod(vec4 x, vec4 y) { return vec4(gpu_fmod(x.x, y.x), gpu_fmod(x.y, y.y), gpu_fmod(x.z, y.z), gpu_fmod(x.w, y.w)); }

float wrap_item(float x, float k) {
    return gpu_fmod(x, k);
}

void main() {
    uint gpu_gid = gl_GlobalInvocationID.x;
    if (gpu_gid >= gpu_count) { return; }
    gpu_out[gpu_gid] = wrap_item(x_in[gpu_gid], k);
}

// ==== twice.comp ====
#version 450
#extension GL_EXT_shader_explicit_arithmetic_types : require
layout(local_size_x = 64) in;

layout(push_constant) uniform Params {
    uint gpu_count;
};
layout(std430, set = 0, binding = 0) readonly buffer x_in_block { float x_in[]; };
layout(std430, set = 0, binding = 1) writeonly buffer gpu_out_block { float gpu_out[]; };

float gpu_fmod(float x, float y) {
    float ax = abs(x);
    float ay = abs(y);
    float r = ax - floor(ax / ay) * ay;
    r = (r < 0.0) ? r + ay : r;
    r = (r >= ay) ? r - ay : r;
    return (x < 0.0) ? -r : r;
}
vec2 gpu_fmod(vec2 x, vec2 y) { return vec2(gpu_fmod(x.x, y.x), gpu_fmod(x.y, y.y)); }
vec3 gpu_fmod(vec3 x, vec3 y) { return vec3(gpu_fmod(x.x, y.x), gpu_fmod(x.y, y.y), gpu_fmod(x.z, y.z)); }
vec4 gpu_fmod(vec4 x, vec4 y) { return vec4(gpu_fmod(x.x, y.x), gpu_fmod(x.y, y.y), gpu_fmod(x.z, y.z), gpu_fmod(x.w, y.w)); }

float wrap_item(float x, float k) {
    return gpu_fmod(x, k);
}
float twice_item(float x, int i) {
    return (wrap_item(x, 2.0) * float(i));
}

void main() {
    uint gpu_gid = gl_GlobalInvocationID.x;
    if (gpu_gid >= gpu_count) { return; }
    gpu_out[gpu_gid] = twice_item(x_in[gpu_gid], int(gpu_gid));
}

