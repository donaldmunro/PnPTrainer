#version {{ver}} core
uniform mat4 MVP;

attribute vec3 vertex;
attribute vec2 tex_coord;
attribute vec4 color;
out vec2 texCoord;
smooth out vec4 vColor;

void main()
{
    texCoord = tex_coord;
    vColor = color;
    gl_Position = MVP*vec4(vertex,1.0);
}
