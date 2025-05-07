#version 410 core

layout (location = 0) in vec3 aPos;

uniform float time;

uniform vec2 u_Pos;

void main() {
    gl_Position = vec4(aPos.x/16.0/4.0+u_Pos.x,aPos.y/9.0/4.0+u_Pos.y,aPos.z, 1.0);
}