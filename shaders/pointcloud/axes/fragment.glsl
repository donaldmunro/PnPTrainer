#version {{ver}} core
//#version 440 core
// precision mediump float;
smooth in vec3 color;
//uniform vec3 color;
layout(location = 0) out vec4 FragColor;
void main() { FragColor = vec4(color, 1); }
