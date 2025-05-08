#version 410 core

layout (location = 0) in vec3 aPos;

uniform float u_Time;

uniform vec2 u_Pos;
uniform float u_Width;
uniform float u_Height;

void main() {
    gl_Position=vec4(u_Pos + vec2(u_Width, u_Height) * aPos.xy, 0.0f,1.0f);
}