#version {{ver}} core
//#version 440 core
uniform mat4 MVP;
layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vColor;

smooth out vec3 color;
void main()
{
   color = vColor;
   gl_Position = MVP * vec4(vPosition, 1.0);
}
