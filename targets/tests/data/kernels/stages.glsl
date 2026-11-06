// ==== extras.vert ====
#version 330 core
in vec4 p;
uniform float k;
out float shade;
void main() {
    gl_Position = (mat4(1.0) * p);
    shade = (k - 3.0 * trunc(k / 3.0));
}

