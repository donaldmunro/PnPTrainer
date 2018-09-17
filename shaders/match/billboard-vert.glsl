#version {{ver}} core
layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 tex;
uniform vec3 eye, up;
uniform mat4 P;
smooth out vec2 texcoord;

void main()
{
   texcoord = tex;
   vec4 position = vec4(pos, 1.0);
   vec3 n = normalize(eye - pos);
   vec3 r = normalize(cross(up, n));
   vec3 u = normalize(cross(n, r));
   mat4 MVP = P*mat4(vec4(r, 0), vec4(u, 0), vec4(n, 0), vec4(0, 0, 0, 1));
   gl_Position = MVP * position;
}