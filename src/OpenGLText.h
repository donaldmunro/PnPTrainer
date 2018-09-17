#ifndef _FONTPROVIDER_H_
#define _FONTPROVIDER_H_

#include <cassert>

#include <iostream>
#include <memory>
#include <utility>
#include <vector>
#include <tuple>
#include <functional>

#include <ft2build.h>
#include FT_FREETYPE_H

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

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "OGLUtils.h"
#include "OGLFiberWin.hh"
//#include "Axes.hh"

//namespace std
//{
//   template<> struct default_delete<ftgl::texture_atlas_t>
//   {
//      void operator()(ftgl::texture_atlas_t* t) { if (t != nullptr) ftgl::texture_atlas_delete(t); }
//   };
//
//   template<> struct default_delete<ftgl::texture_font_t>
//   {
//      void operator()(ftgl::texture_font_t* f) { if (f != nullptr) ftgl::texture_font_delete(f); }
//   };
//}

struct Vertex
//===========
{
   Vertex(GLfloat x_, GLfloat y_, GLfloat z_, GLfloat s_, GLfloat t_, GLfloat r_, GLfloat g_, GLfloat b_, GLfloat a_=1.0f) :
      x(x_), y(y_), z(z_), s(s_), t(t_), r(r_), g(g_), b(b_), a(a_) { has_color = (r >= 0 & g >=0 & b >= 0 && a >= 0); }
   Vertex() = default;
   Vertex(Vertex& other) = default;
   Vertex(Vertex&& other) = default;

   GLfloat x, y, z;
   GLfloat s, t;
   GLfloat r, g, b, a;
   bool has_color = false;
   GLsizeiptr bytesize() { return (has_color) ? 9 * sizeof(GLfloat) : 5 * sizeof(GLfloat); }
   GLsizeiptr size() { return (has_color) ? 9 : 5; }
};

struct RenderCharacter
//====================
{
   RenderCharacter(char ch_, float x_, float y_, float z_) : ch(ch_), x(x_), y(y_), z(z_) {}
   RenderCharacter(RenderCharacter& other) = delete;

   // call with pure pointer (new Vertex)
   template <typename T> void add_vertices(T&& vert) { no_bytes += vert->bytesize(); vertex_data.emplace_back(vert); }
   template <typename T, typename... Ts> void add_vertices(T&& vert, Ts... rest)
   {
      no_bytes += vert->bytesize();
      vertex_data.emplace_back(vert);
      add_vertices(rest...);
   }

   void add_indices(std::initializer_list<GLuint> indexes)
   {
      std::copy(indexes.begin(), indexes.end(), std::back_inserter(index_data));
   };
   GLsizei vertex_count() { return static_cast<GLsizei>(vertex_data.size()); }
   GLsizei index_count() { return static_cast<GLsizei>(index_data.size()); }

   char ch;
   float x, y, z;
   GLsizeiptr no_bytes = 0;
   std::vector<std::unique_ptr<Vertex>> vertex_data;
   std::vector<GLuint> index_data;
};

struct RenderText
//===============
{
   RenderText(const std::string& fontName, const std::string& s, float x_, float y_, float z_, void *param_) :
      font(fontName), text(s), x(x_), y(y_), z(z_), param(param_), text_width(0), text_height(0),
      render_characters(new std::vector<std::unique_ptr<RenderCharacter>>),
      no_vertices(0), no_bytes(0), vbo(0), vao(0), vboi(0) {}
   ~RenderText() { clear(); }

   void clear()
   {
      if (render_characters != nullptr)
      {
         render_characters->clear();
         delete render_characters;
         render_characters = nullptr;
      }
      if (vbo != 0)
         glDeleteBuffers(1, &vbo);
      if (vboi != 0)
         glDeleteBuffers(1, &vboi);
      if (vao != 0)
         glDeleteVertexArrays(1, &vao);
      vbo = vao = vboi = 0;
      no_vertices = 0;
      no_bytes = 0;
      font = text = "";
      x = y = z = std::numeric_limits<float>::quiet_NaN();
      text_width = text_height = 0;
      if (has_color != nullptr)
         delete has_color;
      if (has_indices != nullptr)
         delete has_indices;
      has_color = has_indices = nullptr;
      param = nullptr;
   }

   RenderText(RenderText&) = delete;
   RenderText(RenderText&& other) : font(std::move(other.font)), text(std::move(other.text)),
                                    x(other.x), y(other.y), z(other.z), param(other.param),
                                    render_characters(other.render_characters),
                                    text_width(other.text_width), text_height(other.text_height),
                                    vbo(other.vbo), vao(other.vao), vboi(other.vboi),
                                    has_color(other.has_color), has_indices(other.has_indices)
   {
      other.x = other.y = other.z = other.text_width = other.text_height = std::numeric_limits<float>::quiet_NaN();
      other.vbo = other.vao = other.vboi = 0;
      other.render_characters = nullptr;
      other.has_indices = other.has_color = nullptr;
      other.param = nullptr;
   }
   size_t add(RenderCharacter *prch)
   {
      render_characters->emplace_back(prch);
      no_vertices += prch->vertex_data.size();
      no_bytes += prch->no_bytes;
      if (has_color == nullptr)
         has_color = new bool(prch->vertex_data[0]->has_color);
      else
      {
         if (*has_color != prch->vertex_data[0]->has_color)
         {
            std::cerr << "RenderText::add: Can't mix color and non-color vertices in one text item" << std::endl;
            throw std::logic_error("RenderText::add: Can't mix color and non-color vertices in one text item");
         }
      }
      bool is_indices = (prch->index_data.size() > 0);
      if (is_indices)
         no_indices += prch->index_data.size();
      if (has_indices == nullptr)
         has_indices = new bool(is_indices);
      else
      {
         if (*has_indices != is_indices)
         {
            std::cerr << "RenderText::add: Can't mix indexed and non-indexed vertices in one text item" << std::endl;
            throw std::logic_error("RenderText::add: Can't mix indexed and non-indexed vertices in one text item");
         }
      }
      return render_characters->size();
   }

   size_t size() { return render_characters->size(); }
   bool is_color() { return (has_color != nullptr) ? *has_color : false; }
   bool is_indexed() { return (has_indices != nullptr) ? *has_indices : false; }

   std::string font, text;
   float x, y, z;
   void *param = nullptr;
   std::vector<std::unique_ptr<RenderCharacter>>* render_characters;
   float text_width, text_height;
   size_t no_vertices = 0, no_indices = 0;
   GLsizeiptr no_bytes = 0;
   GLuint vbo, vao, vboi;
   bool* has_color = nullptr, *has_indices = nullptr, uploaded = false;
};

struct OpenGLContext
//=================
{
   virtual void request_context() =0;
   virtual void release_context() =0;
};

class OpenGLText
//=================
{
public:
   OpenGLText(GLuint textureUnit, OpenGLContext* openGLContext =nullptr)  :
      texture_unit(textureUnit), opengl_context(openGLContext) {}
   virtual ~OpenGLText() { clear(); }

   virtual bool initialize(int glsl_ver =440)
   {
      glGetBooleanv(GL_BLEND, &is_blend);
      glGetIntegerv(GL_BLEND_SRC_ALPHA, reinterpret_cast<GLint *>(&blend_src));
      glGetIntegerv(GL_BLEND_DST_ALPHA, reinterpret_cast<GLint *>(&blend_dst));
      return compile_glsl(glsl_ver);
   }

   void setViewport(int w, int h) { width = w, height = h; }
   bool good() { return is_good;};
   bool add_font(std::string fontname, std::string fontpath, int fontsize, bool isRGB =false, std::string preload_characters ="");
   void clear_text() { texts.clear(); }
   bool add_text(const std::string& fontname, const std::string& text, float x, float y, float z, void* param =nullptr,
                 float r=-1,float g =-1, float b=-1, float a=1);
   bool render(std::function<glm::mat4(float, float, float, void*)> MVPCallback, float r=-1,float g =-1, float b=-1, float a=1,
               std::stringstream* errs =nullptr);

   static const char* default_vertex_glsl;
   static const char* default_fragment_glsl;
   static const char* rgb_fragment_glsl;

protected:
   virtual bool compile_glsl(int glsl_ver);
   virtual bool on_pre_render() { return true; }
   virtual bool create_texture(std::string& fontname, std::stringstream* err =nullptr);
   virtual bool text_upload(RenderText* renderText);
//   virtual bool text_render(oglutil::OGLProgramUnit &shader, RenderText &render_text, GLuint texid,
//                            GLenum renderMode = GL_TRIANGLES);

private:
   GLuint texture_unit;
   int width =-1, height =-1;
   OpenGLContext* opengl_context;
   std::unordered_map<std::string, std::tuple<ftgl::texture_atlas_t*, ftgl::texture_font_t*>> fonts;
   oglutil::OGLProgramUnit mono_shader_unit, rgb_shader_unit;
//   std::vector<std::unique_ptr<RenderCharacter>> render_characters;
   std::vector<std::unique_ptr<RenderText>> texts;
   GLboolean is_blend;
   GLenum blend_src, blend_dst;
   bool is_good = false;

   void clear(bool has_gl_context =false);

   bool setup_buffers(RenderText* renderText, std::stringstream *err);
};

inline std::string trim(const std::string& str, std::string chars =" \t")
//------------------------------------------------------------------------
{
   if (str.length() == 0)
      return str;
   unsigned long b = str.find_first_not_of(chars);
   unsigned long e = str.find_last_not_of(chars);
   if (b == std::string::npos) return "";
   return std::string(str, b, e - b + 1);
}

#endif //PNPTRAINER_FONTPROVIDER_H
