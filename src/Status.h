#ifndef __STATUS_H__
#define __STATUS_H__

#include <cstring>
#include <iostream>
#include <string>
#include <regex>
#include <chrono>

#ifdef USE_GLAD
#if !defined(GLAD_GLAPI_EXPORT)
#define GLAD_GLAPI_EXPORT
#endif
#include <glad/glad.h>
#endif
#ifdef USE_GLEW
#include <GL/glew.h>
#endif
#include <GL/gl.h>
#include <GL/glext.h>

#include <freetype-gl/freetype-gl.h>

#include "glm/glm.hpp"

#include "OGLUtils.h"
#include "types.h"

struct StatusText
{
   std::string text;
   glm::vec3 color;
   float x, y;
   StatusText(std::string txt, glm::vec3 colour, float xpos, float ypos) : text(txt), color(colour), x(xpos), y(ypos) {}
private:
   GLuint vbo =0, ivbo = 0, vao =0;
   float xend = 0;
   friend class Status;
};

class Status
{
public:
   Status(GLuint textureUnit, const char* fontPath, int fontSize = 20, bool isRGBFont =false,
          float z =0, float backgroundZ =0.1, glm::vec3 backgroundColour =glm::vec3(0.1f, 0.1f, 0.1f)) :
      texture_unit(textureUnit), font_path(fontPath), font_size(fontSize),
      is_rgb_font(isRGBFont), z(z), background_z(backgroundZ), background_colour(backgroundColour) {}

   virtual ~Status();

   void size(float w, float h) { width = w; height = h; }

   glm::vec2 size() { return glm::vec2(width, height); };

   //Perhaps confusingly, the xpos is absolute for the first parameter and relative for the rest.
   template <typename S, typename V, typename F>
   bool set(S txt, V color, F xpos, F ypos)
   {
      if (strlen(txt) > 0)
         new_texts.emplace_back(txt, color, xpos, ypos);
      if (! texts.empty())
         clear();
      texts = std::move(new_texts);
      is_renderable = (! texts.empty());
      requires_setup = true;
      if ( (atlas != nullptr) && (atlas->id > 0) )
      {
         glDeleteTextures(1, &atlas->id);
         atlas->id = 0;
      }
      start = std::chrono::high_resolution_clock::now();
      return true;
   }
   template <typename S, typename V, typename F, typename... Ts>
   bool set(S txt, V color, F xpos, F ypos, Ts... rest)
   {
      if (strlen(txt) > 0)
         new_texts.emplace_back(txt, color, xpos, ypos);
      return set(rest...);
   }

   void clear()
   //----------
   {
      for (StatusText& stxt : texts)
      {
         glDeleteBuffers(1, &stxt.vbo);
         stxt.vbo = 0;
         glDeleteVertexArrays(1, &stxt.vao);
         stxt.vao = 0;
      }
      texts.clear();
   }

   void set_color(glm::vec3 color) { colour = color; }

   void set_timeout(long timeoutMs) { timeout = timeoutMs; }

   bool create_font();

   bool create_font_texture();

   bool initialize(int glsl_ver =440);

   bool setup_text_gl();

   bool render(const glm::mat4& MVP);

   bool must_render() { return (! texts.empty() && is_renderable); }

private:
   GLuint texture_unit;
   GLboolean is_blend;
   GLenum blend_src, blend_dst;
   long timeout = 5000; //ms
   TimeType start;
   float width =-1, height =-1;
   oglutil::OGLProgramUnit text_program, background_program;
   glm::vec3 colour, background_colour;
   std::string font_path;
   std::vector<StatusText> texts, new_texts;
   int font_size;
   bool is_rgb_font;
   GLfloat z =0, background_z;
   ftgl::texture_atlas_t* atlas = nullptr;
   texture_font_t* font = nullptr;
   GLfloat transparency = 1.0;
   bool is_renderable = true, requires_setup = false;

   inline static std::string vertex_glsl = R"(
#version {{ver}} core
layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 tex;
uniform mat4 MVP;
smooth out vec2 texcoord;

void main()
{
   texcoord = tex;
   gl_Position = MVP * vec4(pos, 1.0);
}
)";

   inline static std::string fragment_glsl = R"(
#version {{ver}} core
smooth in vec2 texcoord;
uniform sampler2D tex;
uniform vec4 color;
uniform bool isRGB;
layout(location = 0) out vec4 FragColor;

void main(void)
{
   if (isRGB)
      FragColor = texture(tex, texcoord) * color;
//      FragColor = vec4(texture(tex, texcoord).r, color.g, color.b, 1);
   else
   {
      float a = texture(tex, texcoord).r;
      FragColor = vec4(color.rgb, color.a*a);
   }
//   FragColor = vec4(1.0, 1.0, 1.0, 1.0);
//   FragColor = vec4(a,a,a, 1);
}
)";

  inline static std::string background_vertex_glsl = R"(
#version {{ver}} core
layout(location = 0) in vec2 vPosition;
uniform mat4 MVP;
uniform vec2 size;
uniform float z;
void main()
{
   vec2 quadVertices[4] = { vec2(0, 0),  vec2(0,  size.y),   vec2(size.x, 0),   vec2(size.x,  size.y) };
   gl_Position = MVP * vec4(quadVertices[gl_VertexID], z, 1.0);
}
)";

   inline static std::string background_fragment_glsl = R"(
#version {{ver}} core
uniform vec4 color;
layout(location = 0) out vec4 FragColor;
void main() { FragColor = color; }
)";

   static const GLuint STATUS_GLYPH_INDICES[];
};
#endif