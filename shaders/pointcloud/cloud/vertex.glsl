#version {{ver}} core
//#version 440 core
uniform mat4 MV;
uniform mat4 P;
uniform float pointSize;

//uniform vec3 location;
uniform float maxDistance;
layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec4 vColor;
smooth out vec4 colour;

void main()
{
   vec3 pos = vPosition.xyz;
   if (vPosition.w == 0)
   {
      colour = vColor;
      gl_PointSize = pointSize;
   }
   else if (vPosition.w == 1)
   {
      colour = vec4(1, 1, 1, 1);
      gl_PointSize = pointSize;
   }
   else if (vPosition.w == 2)
   {
      colour = vec4(1, 1, 0, 1);
      gl_PointSize = pointSize + 3;
   }

   gl_Position = (P * MV) * vec4(pos, 1.0);
}
