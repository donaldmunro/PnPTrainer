#version {{ver}} core
//#version 440 core
uniform mat4 MV;
uniform mat4 P;
uniform vec3 eye;
uniform float pointSize;

//uniform vec3 location;
uniform float maxDistance;
layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec4 vColor;
smooth out vec4 colour;
out float selected;

const float a = 0.7, b = 0.5, c = 0;
void main()
{
   vec3 pos = vPosition.xyz;
   float d = vPosition.w;
   if (d == 0)
   {
      colour = vec4(1, 1, 0, 1);
      selected = 0;
   }
   else
   {
      d = distance(eye, pos);
      colour = vColor;
      selected = 1;
   }
   gl_PointSize = (1/(a + b*d + c*d*d)) * pointSize;
   gl_Position = (P * MV) * vec4(pos, 1.0);
}
