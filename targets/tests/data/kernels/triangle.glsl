// ==== mainVertex.vert ====
#version 330 core
in vec3 position;
uniform mat4 mvp;
out vec3 vColor;
void main() {
    gl_Position = (mvp * vec4(position, 1.0));
    vColor = position;
}

// ==== mainFragment.frag ====
#version 330 core
in vec3 vColor;
out vec4 fragColor;
void main() {
    fragColor = vec4(vColor, 1.0);
}

