#version 410 core

out vec4 FragColor;

uniform float time;
uniform vec3 u_Color;
uniform vec2 u_Pos;

void main() {
    FragColor = vec4(u_Color,1.0f);
}