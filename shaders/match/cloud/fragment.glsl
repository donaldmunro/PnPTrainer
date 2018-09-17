#version {{ver}} core
smooth in vec4 colour;
in float selected;
uniform vec3 light;
layout(location = 0) out vec4 FragColor;

float Ns = 250;
vec3 mat_specular=vec3(1);
vec3 light_specular=vec3(1);
vec3 light_ambient=vec3(0.1, 0.1, 0.1);

void main(void)
{
   vec3 N;
   N.xy = gl_PointCoord * 2.0 - vec2(1.0);
   float mag = dot(N.xy, N.xy);
// if (mag > 1.0) discard;   // kill pixels outside circle
   if (selected == 0)
      FragColor = vec4(colour.rgb, 1);
   else if (mag <= 1.0)
   {
      N.z = sqrt(1.0-mag);
      float diffuse = max(0.0, dot(light, N));
      vec3 eye = vec3 (0.0, 0.0, 1.0);
      vec3 halfVector = normalize( eye + light);
      float spec = max( pow(dot(N,halfVector), Ns), 0.);
      vec3 specular = light_specular*mat_specular* spec;
      vec3 modulated = min((light_ambient + colour.rgb * diffuse) + specular, vec3(1.0));
      FragColor = vec4(modulated, 1);
    }
    else if (mag <= 2.0)
      FragColor = vec4(0.1, 0.1, 0.1, 0.2);
    else discard;
}
