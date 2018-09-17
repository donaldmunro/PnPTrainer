#version {{ver}} core
uniform sampler2D tex;
in vec2 texCoord;
in vec4 vColor;
layout(location = 0) out vec4 FragColor;
const vec4 colour = vec4(1,0,1,1);
void main()
{
    float a = texture(tex, texCoord).r;
//    float a = texture(tex, texCoord).b;
//    FragColor = vec4(a, a, a, 1);
//    FragColor = vec4(1, 1, 1, 1);
//    FragColor = vec4(colour.rgb, colour.a*a);
    FragColor = vec4(vColor.rgb, vColor.a*a);

//   FragColor = vec4(texture(tex, texCoord));
}
