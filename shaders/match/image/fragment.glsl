#version {{ver}} core
//#extension GL_ARB_texture_rectangle: enable
in vec2 texCoord;

// uniform sampler2D tex;
//uniform usampler2DRect tex; //GL_TEXTURE_RECTANGLE: The documentation says this should work
uniform sampler2DRect tex; //however this works, not the above.

layout(location = 0) out vec4 FragColor;

void main()
{
//   FragColor = vec4(texture(tex, texCoord));
   FragColor = vec4(texture2DRect(tex, texCoord));
}
