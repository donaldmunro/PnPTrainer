#include <cmath>
#include <vector>
#include <memory>
#include <utility>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>


#include "OpenGLText.h"
#include "util.h"
#include "SourceLocation.hh"

const char* OpenGLText::default_vertex_glsl = R"(
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

const char* OpenGLText::default_fragment_glsl = R"(
#version {{ver}} core
smooth in vec2 texcoord;
uniform sampler2D tex;
uniform vec4 color;
layout(location = 0) out vec4 FragColor;

void main(void)
{
   float a = texture(tex, texcoord).r;
   FragColor = vec4(color.rgb, color.a*a);
//   FragColor = vec4(1.0, 1.0, 1.0, 1.0);
//   FragColor = vec4(a,a,a, 1);
}
)";

const char* OpenGLText::rgb_fragment_glsl = R"(
#version {{ver}} core
in vec2 texcoord;
uniform sampler2D tex;
uniform vec4 color;
layout(location = 0) out vec4 FragColor;

void main(void)
{
   //FragColor = vec4(mix(color.rgb, texture(tex, texcoord).rgb, 0.5), 1);
   FragColor = texture(tex, texcoord) * color;
}
)";

bool OpenGLText::compile_glsl(int glsl_ver)
//-----------------------------------------
{
   std::regex r(R"(\{\{ver\}\})");
   std::stringstream ss;
   ss << glsl_ver;
   std::string vertex = std::regex_replace(default_vertex_glsl, r, ss.str());
   std::string mono_fragment = std::regex_replace(default_fragment_glsl, r, ss.str());
   std::string rgb_fragment = std::regex_replace(rgb_fragment_glsl, r, ss.str());
   if (mono_shader_unit.compile_link(vertex.c_str(), mono_fragment.c_str()) != GL_NO_ERROR)
   {
      std::cerr << "OpenGLText::compile_glsl - Error compiling OpenGLString mono shaders" << std::endl
                << mono_shader_unit.log.str() << std::endl;
      return false;
   }
   if (rgb_shader_unit.compile_link(vertex.c_str(), rgb_fragment.c_str()) != GL_NO_ERROR)
   {
      std::cerr << "OpenGLText::compile_glsl - Error compiling Billboard OpenGLString RGB  shaders" << std::endl
                << rgb_shader_unit.log.str() << std::endl;
      return false;
   }
   return true;
}

bool OpenGLText::add_font(std::string fontname, std::string fontpath, int fontsize, bool isRGB,
                          std::string preload_characters)
//----------------------------------------------------------------------------------------------------------------------
{
   ftgl::texture_atlas_t* atlas;
   if (isRGB)
      atlas = ftgl::texture_atlas_new(512, 512, 3);
   else
      atlas = ftgl::texture_atlas_new(512, 512, 1);
   if (atlas == nullptr)
   {
      std::cerr << "Error creating font atlas for " << fontname << " from " << fontpath << std::endl;
      return false;
   }
   atlas->id = 0;
   texture_font_t* font = ftgl::texture_font_new_from_file(atlas, fontsize, fontpath.c_str());
   if (font == nullptr)
   {
      std::cerr << "Error creating font for " << fontname << " from " << fontpath << std::endl;
      ftgl::texture_atlas_delete(atlas);
      return false;
   }
   if (! preload_characters.empty())
      texture_font_load_glyphs(font, preload_characters.c_str());
   fonts[fontname] = std::make_tuple(atlas, font);
//   fonts.insert_or_assign(fontname, std::make_tuple(atlas, font, 0));
   return true;
}

bool OpenGLText::create_texture(std::string& fontname, std::stringstream* errs)
//------------------------------------------------------
{
   auto it = fonts.find(fontname);
   if (it == fonts.end())
      return false;
   ftgl::texture_atlas_t* atlas;
   ftgl::texture_font_t* font;
   std::tie(atlas, font) = it->second;
   bool is_RGB = (atlas->depth == 3);
   GLenum err;
   unsigned int& texid = atlas->id;
   glGenTextures(1, &texid);
   glBindTexture( GL_TEXTURE_2D, texid);
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
   glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, atlas->width);
   glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
   glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
   if (is_RGB)
   {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, static_cast<GLsizei>(atlas->width),
                   static_cast<GLsizei>(atlas->height),
                   0, GL_RGB, GL_UNSIGNED_BYTE, atlas->data);

//      cv::Mat img(atlas->height, atlas->width, CV_8UC3, atlas->data); cv::imwrite("font.png", img);
//      img.create(atlas->height, atlas->width, CV_8UC3);
//      glPixelStorei(GL_PACK_ALIGNMENT, (img.step & 3) ? 1 : 4);
//      glPixelStorei(GL_PACK_ROW_LENGTH, img.step/img.elemSize());
//      glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, img.data);
//      cv::imwrite("texture.png", img);
   }
   else
   {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<GLsizei>(atlas->width),
                   static_cast<GLsizei>(atlas->height), 0, GL_RED, GL_UNSIGNED_BYTE,
                   atlas->data);

//      cv::Mat img(atlas->height, atlas->width, CV_8UC1, atlas->data); cv::imwrite("font.png", img);
//      img.create(atlas->height, atlas->width, CV_8UC1);
//      glPixelStorei(GL_PACK_ALIGNMENT, (img.step & 3) ? 1 : 4);
//      glPixelStorei(GL_PACK_ROW_LENGTH, img.step/img.elemSize());
//      glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, img.data);
//      cv::imwrite("texture.png", img);
   }

   if (! oglutil::isGLOk(err, errs))
   {
      is_good = false;
      std::cerr << "OpenGLText::create_texture: Error creating texture (" << err << ": "
                << ((errs == nullptr) ? "" : errs->str()) << ")" << std::endl;

      return false;
   }
   glBindTexture(GL_TEXTURE_2D, 0);

   glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
   glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
   glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
   return true;
}

bool OpenGLText::add_text(const std::string& fontname, const std::string& text, float x, float y, float z, void* param,
                          float r, float g, float b, float a)
//--------------------------------------------------------------------------------------------------------
{
   if (trim(text).empty())
      return true;
   auto it = fonts.find(fontname);
   if (it == fonts.end())
   {
      std::cerr << "OpenGLText::add_text: Undefined font " << fontname << std::endl;
      return false;
   }
   ftgl::texture_atlas_t* atlas;
   ftgl::texture_font_t* font;
   std::tie(atlas, font) = it->second;
   std::unique_ptr<RenderText> textptr = std::make_unique<RenderText>(fontname, text, x, y, z, param);
   RenderText* render_text = textptr.get();
   texts.push_back(std::move(textptr));
//   std::cout << render_text.text << ": " << render_text.x << "," << render_text.y << "," << render_text.z << std::endl;
   double sx = (1.0 - -1.0) / static_cast<double>(width);
   double sy = (1.0 - -1.0)/ static_cast<double>(height);
   const char* psz = text.c_str();
   float endx = x;
   float maxy = y; //std::numeric_limits<float>::lowest();
   const float startx = x, starty = y;
   for (size_t i = 0; i < text.length(); ++i )
   {
      texture_glyph_t *glyph = texture_font_get_glyph(font, &psz[i]);
      if (glyph != nullptr)
      {
         if ( i > 0)
            x += texture_glyph_get_kerning(glyph, &psz[i-1]);

         float x0  = static_cast<float>(x + static_cast<double>(glyph->offset_x) * sx);
         float x1  = static_cast<float>(x0 + static_cast<double>(glyph->width) * sx);
         float y1  = static_cast<float>(y + static_cast<double>(glyph->height - glyph->offset_y) * sy);
         float y0  = static_cast<float>(y1 - static_cast<double>(glyph->height) * sy);
//         float y0  = static_cast<float>(y + static_cast<double>(glyph->offset_y) * sy);
//         float y1  = static_cast<float>(y0 + static_cast<double>(glyph->height) * sy);

         if (y1 < y0)
            std::swap(y1, y0);
         if (y1 > maxy)
            maxy = y1;

         float s0 = glyph->s0;
         float t0 = glyph->t0;
         float s1 = glyph->s1;
         float t1 = glyph->t1;
         RenderCharacter* render_char = new RenderCharacter(psz[i], x, y, z);
         render_char->add_vertices( new Vertex(x0,y0,z,  s0,t0,  r,g,b,a),
                                    new Vertex(x0,y1,z,  s0,t1,  r,g,b,a),
                                    new Vertex(x1,y0,z,  s1,t0,  r,g,b,a),
                                    new Vertex(x1,y1,z,  s1,t1,  r,g,b,a)
         );
         render_text->add(render_char);

//         for (auto& it : render_char->vertex_data)
//            std::cout << "V: " << it.get()->x << ", " << it.get()->y << ", " << it.get()->z << std::endl;
//         std::cout << "=============" << std::endl;

         x += static_cast<double>(glyph->advance_x)*sx;
//           x = x1 + static_cast<float>(static_cast<double>(glyph->advance_x)*sx);
      }
      else
      {
         std::cerr << "OpenGLText::add_text: Texture atlas out of space." << std::endl;
         return false;
      }
   }
   endx = x;
   render_text->text_width = fabsf(endx - startx);
   render_text->text_height = fabsf(maxy - starty);
   return true;
}

bool OpenGLText::render(std::function<glm::mat4(float, float, float, void*)> MVPCallback, float r, float g, float b, float a,
                        std::stringstream* errs)
//-------------------------------------------------------------------------------------------------------------
{
   std::unique_ptr <std::stringstream> perrs;
   if (errs == nullptr)
   {
      perrs = std::make_unique<std::stringstream>();
      errs = perrs.get();
   }
#if !defined(NDEBUG)
   SourceLocation& source_location = SourceLocation::instance();
   source_location.push("OpenGLText", "render()", __LINE__);
#endif
   GLenum err;
   int cerrors = 0;
   for (std::unique_ptr<RenderText>& tp : texts)
   {
      RenderText* render_text = tp.get();
      oglutil::clearGLErrors();
      auto it = fonts.find(render_text->font);
      if (it == fonts.end())
      {
         *errs << "Font " << render_text->font << " for text " << render_text->text << " not found." << std::endl;;
         cerrors++;
         continue;
      }
      GLsizei no = static_cast<GLsizei>(render_text->no_vertices);
      if (no == 0)
      {
//         std::cerr << "OpenGLText::render(): WARNING - Vertex count == 0 for font " << render_text->font << " text "
//                   << render_text->text << std::endl;
         continue;
      }
      std::vector<std::unique_ptr<RenderCharacter>>* render_characters = render_text->render_characters;
      ftgl::texture_atlas_t* atlas;
      ftgl::texture_font_t* font;
      std::tie(atlas, font) = it->second;
      if (atlas->id == 0)
      {
         std::stringstream ss;
#if !defined(NDEBUG)
         source_location.update(__LINE__, "Creating font texture");
#endif
         if (! create_texture(render_text->font, &ss))
         {
            *errs << "Error creating texture for font " << render_text->font << " for text " << render_text->text
                  << " (" << ss.str() << ")" << std::endl;;
            cerrors++;
            continue;
         }
      }
      bool is_RGB = (atlas->depth == 3), has_colour = render_text->is_color();
      if (has_colour)
         r = g = b = a = -1;
      if ( (render_text->vbo == 0) || (render_text->vao == 0) )
      {
#if !defined(NDEBUG)
         source_location.update(__LINE__, "Setting up VBO/VAO");
#endif
         std::stringstream ss;
         if (!setup_buffers(render_text, &ss))
         {
            *errs << "Error creating buffers for text " << render_text->text << " (" << ss.str() << ")" << std::endl;;
            cerrors++;
            continue;
         }
      }
      if (!on_pre_render())
      {
         *errs << "OpenGLText::render: on_pre_render msg" << std::endl;
         cerrors++;
         continue;
      }
      if ( (! render_text->uploaded) && (! text_upload(render_text)) )
      {
         *errs << "Error uploading vertex buffers for text " << render_text->text << std::endl;;
         cerrors++;
         continue;
      }

#if !defined(NDEBUG)
      source_location.update(__LINE__, "Setting up render");
#endif
      if (is_blend == GL_FALSE)
         glEnable(GL_BLEND);
      if (!oglutil::isGLOk(err, errs)) std::cerr << err << ": " << errs->str() << std::endl;
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      if (!oglutil::isGLOk(err, errs)) std::cerr << err << ": " << errs->str() << std::endl;
      glBlendColor(1, 1, 1, 1);
      if (!oglutil::isGLOk(err, errs)) std::cerr << err << ": " << errs->str() << std::endl;
      oglutil::OGLProgramUnit& program = (is_RGB) ? rgb_shader_unit : mono_shader_unit;
      program.activate();
      glBindTexture(GL_TEXTURE_2D, atlas->id);
      if (!oglutil::isGLOk(err, errs))
      {
         std::cerr << "OpenGLText::render_text " << render_text->text << ": " << err << ": " << errs->str()
                   << " binding texture " << atlas->id << std::endl;
         cerrors++;
         continue;
      }
      glActiveTexture(texture_unit);
      if (!oglutil::isGLOk(err, errs)) std::cerr << "OpenGLText::render_text: texture unit " << err << ": " << errs->str().c_str() << std::endl;

      glm::mat4 MVP = MVPCallback(render_characters->at(0)->x, render_characters->at(0)->y, render_characters->at(0)->z,
                                  render_text->param);
      glUniformMatrix4fv(program.uniform("MVP"), 1, GL_FALSE, &MVP[0][0]);

      if (r >= 0 && g >= 0 && b >= 0 && a >= 0)
         glUniform4f(program.uniform("color"), r, g, b, a);
      glUniform1i(program.uniform("tex"), texture_unit - GL_TEXTURE0);
      if (!oglutil::isGLOk(err, errs))
      {
         std::cerr << "OpenGLText::render - Error binding uniforms (" << err << ": " << errs->str() << ")" << std::endl;
#if !defined(NDEBUG)
         source_location.pop();
#endif
         return false;
      }
      glBindVertexArray(render_text->vao);
      if (!oglutil::isGLOk(err, errs)) std::cerr << "OpenGLText::render_text: VAO " << err << ": " << errs->str().c_str() << std::endl;
//      GLsizei no_indices = static_cast<GLsizei>(render_text->no_indices);
//      if ((no_indices > 0) && (render_text->vboi > 0))
//         glDrawElements(GL_TRIANGLE_STRIP, no_indices, GL_UNSIGNED_INT, 0);
//      else
//         glDrawArrays(GL_TRIANGLE_STRIP, 0, no);
#if !defined(NDEBUG)
      source_location.update(__LINE__, "Rendering.");
#endif
      GLint offset =0;
      for (size_t i=0; i<render_characters->size(); i++)
      {
         const std::unique_ptr<RenderCharacter>& character = render_characters->at(i);
         GLsizei cnt = character->vertex_count();
         if (cnt == 0) continue;
//         glm::mat4 MVP = MVPCallback(character->x, character->y, character->z);
//         glUniformMatrix4fv(program.uniform("MVP"), 1, GL_FALSE, &MVP[0][0]);
         glBindTexture(GL_TEXTURE_2D, atlas->id); // Why is this necessary ??????
         GLsizei icnt = character->index_count();
         if ((icnt> 0) && (render_text->vboi > 0))
         {
            glDrawElements(GL_TRIANGLE_STRIP, icnt, GL_UNSIGNED_INT, reinterpret_cast<const void *>(offset));
            offset += icnt;
         }
         else
         {
            glDrawArrays(GL_TRIANGLE_STRIP, offset, cnt);
            offset += cnt;
         }
      }
      bool isOK = oglutil::isGLOk(err, errs);
      if (! isOK)
      {
         std::cerr << "OpenGLText::render_text: Draw msg" << err << ": " << errs->str().c_str()
                   << std::endl;
      }
      if (is_blend == GL_FALSE)
         glDisable(GL_BLEND);
      glBlendFunc(blend_src, blend_dst);
   }
#if !defined(NDEBUG)
   source_location.pop();
#endif
   return (cerrors == 0);
}

bool OpenGLText::setup_buffers(RenderText* renderText, std::stringstream *errs)
//---------------------------------------------------------------------------
{
   std::vector<std::unique_ptr<RenderCharacter>>* render_characters = renderText->render_characters;
   if ( (render_characters->empty()) || ((*render_characters)[0]->vertex_data.empty()) ) return true;
   if (renderText->vbo > 0) glDeleteBuffers(1, &renderText->vbo);
   if (renderText->vboi > 0)
      glDeleteBuffers(1, &renderText->vboi);
   if (renderText->vao > 0)
      glDeleteVertexArrays(1, &renderText->vao);
   GLsizeiptr bytesize = renderText->no_bytes;
   size_t no = renderText->no_vertices;
   bool has_colour = renderText->is_color(), has_indices = renderText->is_indexed();
   oglutil::clearGLErrors();
   glGenBuffers(1, &renderText->vbo);
   if (has_indices)
      glGenBuffers(1, &renderText->vboi);
   glGenVertexArrays(1, &renderText->vao);
   GLenum err;
   std::stringstream ss;
   if (!oglutil::isGLOk(err, &ss))
   {
      if (errs != nullptr)
         *errs << "OpenGLString::setup_buffer: Error creating VBO/VAO (" << ss.str() << ")" << std::endl;
      if (renderText->vbo > 0) glDeleteBuffers(1, &renderText->vbo);
      if ( (has_indices) && (renderText->vboi > 0) )
         glDeleteBuffers(1, &renderText->vboi);
      if (renderText->vao > 0)
         glDeleteVertexArrays(1, &renderText->vao);
      renderText->vbo = renderText->vboi = renderText->vao = 0;
      return false;
   }
   glBindBuffer(GL_ARRAY_BUFFER, renderText->vbo);
   glBufferData(GL_ARRAY_BUFFER, bytesize, nullptr, GL_DYNAMIC_DRAW);
   if (has_indices)
   {
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderText->vboi);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, renderText->no_indices*sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
   }
   glBindBuffer(GL_ARRAY_BUFFER, 0);

   glBindVertexArray(renderText->vao);
   glBindBuffer(GL_ARRAY_BUFFER, renderText->vbo);
   if (has_indices)
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderText->vboi);
   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);
   GLsizei stride = static_cast<GLsizei>(bytesize / no); // = sizeof(GLfloat) * (3 + 2);
   glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, 0);
   glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void *>(3 * sizeof(GLfloat)));
   if (has_colour)
   {
      glEnableVertexAttribArray(2);
      glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void *>(5 * sizeof(GLfloat)));
   }
   glBindVertexArray(0);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   if (has_indices)
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
   if (!oglutil::isGLOk(err, errs))
   {
      std::cerr << "OpenGLText::setup_buffers " << renderText->text << ": " << err << ": " << errs->str() << std::endl;
      return false;
   }
   return true;
}

void OpenGLText::clear(bool has_gl_context)
//-----------------------------------------
{
   if ( (! has_gl_context) && (opengl_context != nullptr) )
      opengl_context->request_context();
   if (! fonts.empty())
   {
      for (auto pp : fonts)
      {
         ftgl::texture_atlas_t* atlas;
         ftgl::texture_font_t* font;
         std::tie(atlas, font) = pp.second;
         if (atlas->id != 0)
            glDeleteTextures(1, &atlas->id);
         ftgl::texture_atlas_delete(atlas);
         ftgl::texture_font_delete(font);
      }
   }
   if ( (! has_gl_context) && (opengl_context != nullptr) )
      opengl_context->release_context();
}

bool OpenGLText::text_upload(RenderText* renderText)
//---------------------------------------------------
{
   std::vector<std::unique_ptr<RenderCharacter>>* render_characters = renderText->render_characters;
   if ( (render_characters->empty()) || ((*render_characters)[0]->vertex_data.empty()) ) return true;
   size_t no = renderText->no_vertices;
   if (no == 0) return true;
   GLsizeiptr bytesize = renderText->no_bytes;
   bool color = renderText->is_color();

   glBindBuffer(GL_ARRAY_BUFFER, renderText->vbo);
   GLfloat *vertex_ptr;
   vertex_ptr = static_cast<GLfloat *>(glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
   bool useMap = (vertex_ptr != nullptr), useIndexMap = false;
   if (vertex_ptr == nullptr)
      vertex_ptr = new GLfloat[no];
   GLfloat *ptr = vertex_ptr;
   GLuint *index_ptr = nullptr, *iptr = nullptr;
   if (renderText->is_indexed())
   {
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderText->vboi);
      index_ptr = static_cast<GLuint *>(glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY));
      useIndexMap = (index_ptr != nullptr);
      if (index_ptr == nullptr)
         index_ptr = new GLuint[renderText->no_indices];
      iptr = index_ptr;
   }
   for (size_t i=0; i<render_characters->size(); i++)
   {
      RenderCharacter* character = render_characters->at(i).get();
      std::vector<std::unique_ptr<Vertex>> &vertices = character->vertex_data;
      size_t vno = vertices.size();
      for (size_t j = 0; j < vno; j++)
      {
         Vertex *v = vertices[j].get();
         *ptr++ = v->x;
         *ptr++ = v->y;
         *ptr++ = v->z;
         *ptr++ = v->s;
         *ptr++ = v->t;
         if (color)
         {
            *ptr++ = v->r;
            *ptr++ = v->g;
            *ptr++ = v->b;
            *ptr++ = v->a;
         }
      }
      if (renderText->is_indexed())
      {
         std::vector<GLuint> &indices = character->index_data;
         size_t ino = indices.size();
         if (ino > 0)
         {
            for (size_t j = 0; j < ino; j++)
               *iptr++ = indices[j];
         }
      }
   }
   if (useMap)
      glUnmapBuffer(GL_ARRAY_BUFFER);
   else
   {
      glBufferData(GL_ARRAY_BUFFER, bytesize, vertex_ptr, GL_STATIC_DRAW);
      delete[] vertex_ptr;
   }
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   if (renderText->is_indexed())
   {
      if (useIndexMap)
         glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
      else if (index_ptr != nullptr)
      {
         glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * renderText->no_indices, index_ptr, GL_STATIC_DRAW);
         delete[] index_ptr;
      }
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
   }

   GLenum err;
   std::stringstream errs;
   if (! oglutil::isGLOk(err, &errs))
   {
      std::cerr << "OpenGLText::upload " << err << ": " << errs.str() << ")" << std::endl;
      return false;
   }
   renderText->uploaded = true;
   return true;
}

//bool OpenGLText::text_render(oglutil::OGLProgramUnit &shader, RenderText &render_text, GLuint texid, GLenum renderMode)
////---------------------------------------------------------------------------------------------------------------------
//{
//   GLsizei no = static_cast<GLsizei>(render_text.no_vertices);
//   if (no == 0) return true;
//   GLenum err;
//   std::stringstream errs;
//   glBindTexture(GL_TEXTURE_2D, texid);
//   if (!oglutil::isGLOk(err, &errs))
//   {
//      std::cerr << "OpenGLText::render_text " << render_text.text << ": " << err << ": " << errs.str()
//                << " binding texture " << texid << std::endl;
//      return false;
//   }
//   glActiveTexture(texture_unit);
//   if (!oglutil::isGLOk(err, &errs)) std::cerr << "OpenGLText::render_text: texture unit " << err << ": " << errs.str().c_str() << std::endl;
//   glUniform1i(shader.uniform("tex"), texture_unit - GL_TEXTURE0);
//   if (!oglutil::isGLOk(err, &errs)) std::cerr << "OpenGLText::render_text: texture uniform " << err << ": " << errs.str().c_str() << std::endl;
//   glBindVertexArray(render_text.vao);
//   if (!oglutil::isGLOk(err, &errs)) std::cerr << "OpenGLText::render_text: VAO " << err << ": " << errs.str().c_str() << std::endl;
//   GLsizei no_indices = static_cast<GLsizei>(render_text.no_indices);
//   if ((no_indices > 0) && (render_text.vboi > 0))
//      glDrawElements(renderMode, no_indices, GL_UNSIGNED_INT, 0);
//   else if (render_text.vbo > 0)
//   {
////      glDrawArrays(renderMode, 0, no);
//
//      std::vector<std::unique_ptr<RenderCharacter>>* render_characters = render_text.render_characters;
//      GLint offset =0;
//      glBindTexture(GL_TEXTURE_2D, 0);
//      for (size_t i=0; i<render_characters->size(); i++)
//      {
//         glBindTexture(GL_TEXTURE_2D, texid);
//         glActiveTexture(texture_unit);
//         glBindBuffer(GL_ARRAY_BUFFER, render_text.vbo);
//         glBindVertexArray(render_text.vao);
//         glDrawArrays(renderMode, offset, 4);
//         offset += 4;
//         glBindVertexArray(0);
//         glBindBuffer(GL_ARRAY_BUFFER, 0);
//         glActiveTexture(GL_TEXTURE0);
//      }
//   }
//   else
//   {
//      std::cerr << "OpenGLText::render_text: OpenGL state not initialised for " << render_text.text << std::endl;
//      return false;
//   }
//   bool isOK = oglutil::isGLOk(err, &errs);
//   glBindVertexArray(0);
//   glActiveTexture(GL_TEXTURE0);
//   glBindTexture(GL_TEXTURE_2D, 0);
//   if (! isOK)
//   {
//      std::cerr << "OpenGLText::render_text: Draw msg" << err << ": " << errs.str().c_str()
//                << std::endl;
//   }
//   return isOK;
//}