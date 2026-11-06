// ==== cell.comp ====
#version 450
#extension GL_EXT_shader_explicit_arithmetic_types : require
layout(local_size_x = 8, local_size_y = 8) in;

layout(push_constant) uniform Params {
    uint gpu_width;
    uint gpu_height;
    int width;
};
layout(std430, set = 0, binding = 0) writeonly buffer gpu_out_block { int gpu_out[]; };

int cell_item(int x, int y, int width) {
    return ((y * width) + x);
}

void main() {
    uint gpu_x = gl_GlobalInvocationID.x;
    uint gpu_y = gl_GlobalInvocationID.y;
    if (gpu_x >= gpu_width || gpu_y >= gpu_height) { return; }
    uint gpu_gid = gpu_y * gpu_width + gpu_x;
    gpu_out[gpu_gid] = cell_item(int(gpu_x), int(gpu_y), width);
}

