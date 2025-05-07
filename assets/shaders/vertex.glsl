#version 410 core

layout (location = 0) in vec3 aPos;

uniform float time;

uniform vec2 u_Pos;
uniform vec2 u_Scale;

void main() {
    float x = aPos.x/16.0/4.0*u_Scale.x;
    float y = aPos.y/9.0/4.0*u_Scale.y;
    gl_Position = vec4(x+u_Pos.x,y+u_Pos.y,0.0f, 1.0f);
}