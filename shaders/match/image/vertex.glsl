#version {{ver}} core
layout(location = 0) in vec2 vPosition;
//layout(location = 1) in vec2 tCoord;
uniform float z;
out vec2 texCoord;
//void main()
//{
//   gl_Position = vec4(vPosition, 0.1, 1);
//   texCoord = tCoord;
//}

//const vec2 quadVertices[4] = { vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0)};
//const vec2 texVertices[4] = { vec2(0, 0), vec2(1, 0), vec2(0, 1), vec2(1, 1) };

//const vec2 quadVertices[4] = { vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0) };
//const vec2 texVertices[4] = { vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1) };

//const vec2 quadVertices[4] = { vec2(-1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(1.0, 1.0)};
//const vec2 texVertices[4] = { vec2(0, 0), vec2(0, 1), vec2(1, 0), vec2(1, 1) };

uniform vec2 uImageSize;

const vec2 quadVertices[4] = { vec2(-1, -1),  vec2(-1,  1),   vec2(1, -1),   vec2(1,  1) };
//const vec2 texVertices[4] = { vec2(0,  1),  vec2(0,  0),  vec2(1,  1),   vec2(1,  0) };

void main()
{
   vec2 texVertices[4] = { vec2(0,  uImageSize.y),  vec2(0,  0),
                           vec2(uImageSize.x, uImageSize.y),   vec2(uImageSize.x,  0) };
   texCoord = texVertices[gl_VertexID];
   gl_Position = vec4(quadVertices[gl_VertexID], z, 1.0);
}