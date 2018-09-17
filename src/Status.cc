#include "Status.h"
#include "SourceLocation.hh"

const GLuint Status::STATUS_GLYPH_INDICES[] = {0, 1, 2, 0, 2, 3};

#ifdef HAVE_SOIL2
#include <SOIL2.h>

inline void saveTexture(GLuint image_texture, GLsizei width, GLsizei  height, int channels, const char* name,
                        GLint pack =-1, GLint rowlen =-1)
//------------------------------------------------------------------------------------------------------
{
   glBindTexture(GL_TEXTURE_2D, image_texture);
   if (pack > 0)
      glPixelStorei(GL_PACK_ALIGNMENT, pack);
   if (rowlen >= 0)
      glPixelStorei(GL_PACK_ROW_LENGTH, rowlen);
   GLenum  format;
   switch (channels)
   {
      case 3: format = GL_RGB; break;
      case 4: format = GL_RGBA; break;
      case 1: format = GL_RED; break;
      default: format = GL_RGB; break;
   }
   std::unique_ptr<unsigned char []> data(new unsigned char[width*height*channels]);
   glGetTexImage(GL_TEXTURE_2D, 0, format, GL_UNSIGNED_BYTE, data.get());
   if (pack > 0)
      glPixelStorei(GL_PACK_ALIGNMENT, 4);
   if (rowlen > 0)
      glPixelStorei(GL_PACK_ROW_LENGTH, 0);
   SOIL_save_image(name, SOIL_SAVE_TYPE_PNG, width, height, channels,
                   static_cast<const unsigned char *const>(data.get()));
   glBindTexture(GL_TEXTURE_2D, 0);
}
#endif

bool Status::create_font()
//------------------------
{
   if (is_rgb_font)
      atlas = ftgl::texture_atlas_new(512, 512, 3);
   else
      atlas = ftgl::texture_atlas_new(512, 512, 1);
   if (atlas == nullptr)
   {
      std::cerr << "Error loading Statusbar texture atlas" << std::endl;
      return false;
   }

   font = ftgl::texture_font_new_from_file(atlas, font_size, font_path.c_str());
   if (font == nullptr)
   {
      std::cerr << "Error creating font  from " << font_path << std::endl;
      ftgl::texture_atlas_delete(atlas);
      return false;
   }
//   ftgl::texture_font_load_glyphs(font, "Testing 1234567890Status:");
   return true;
}

bool Status::create_font_texture()
//--------------------------------
{
   GLenum err;
   std::stringstream errs;

   unsigned int& texid = atlas->id;
   if (texid > 0)
      return true;
   oglutil::clearGLErrors();
//      int w = static_cast<int>(atlas->width), h = static_cast<int>(atlas->height);
//      texid = SOIL_create_OGL_texture(atlas->data, &w, &h, atlas->depth, 0, 0);
   glGenTextures(1, &texid);
   glBindTexture( GL_TEXTURE_2D, texid);
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
   glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, atlas->width);
////      glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
////      glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
   if (is_rgb_font)
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, static_cast<GLsizei>(atlas->width),
                   static_cast<GLsizei>(atlas->height),
                   0, GL_RGB, GL_UNSIGNED_BYTE, atlas->data);
   else
      glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<GLsizei>(atlas->width),
                   static_cast<GLsizei>(atlas->height), 0, GL_RED, GL_UNSIGNED_BYTE,
                   atlas->data);
   if (! oglutil::isGLOk(err, &errs))
   {
      std::cerr << "Status::create_font_texture: Error creating texture (" << err << ": " << errs.str() << ")" << std::endl;
      return false;
   }
   glBindTexture(GL_TEXTURE_2D, 0);

//   saveTexture(texid, atlas->width, atlas->height, atlas->depth, "status.png");

   glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
   glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
   glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
   return true;
}

bool Status::initialize(int glsl_ver)
//-----------------------------------
{
   GLenum err;
   std::stringstream errs;
   if (! create_font())
      return false;
   glGetBooleanv(GL_BLEND, &is_blend);
   glGetIntegerv(GL_BLEND_SRC_ALPHA, reinterpret_cast<GLint *>(&blend_src));
   glGetIntegerv(GL_BLEND_DST_ALPHA, reinterpret_cast<GLint *>(&blend_dst));
   std::regex r(R"(\{\{ver\}\})");
   std::stringstream ss;
   ss << glsl_ver;
   std::string vertex = std::regex_replace(vertex_glsl, r, ss.str());
   std::string fragment = std::regex_replace(fragment_glsl, r, ss.str());
   if (text_program.compile_link(vertex.c_str(), fragment.c_str()) != GL_NO_ERROR)
   {
      std::cerr << "Status::initialize - Error compiling OpenGLString shaders" << std::endl
                << text_program.log.str() << std::endl;
      return false;
   }

   vertex = std::regex_replace(background_vertex_glsl, r, ss.str());
   fragment = std::regex_replace(background_fragment_glsl, r, ss.str());
   if (background_program.compile_link(vertex.c_str(), fragment.c_str()) != GL_NO_ERROR)
   {
      std::cerr << "Status::initialize - Error compiling OpenGLString background shaders" << std::endl
                << text_program.log.str() << std::endl;
      return false;
   }
   return true;
}

bool Status::setup_text_gl()
//--------------------------
{
   if (texts.empty())
   {
      is_renderable = false;
      return false;
   }
   GLenum err;
   std::stringstream errs;
   oglutil::clearGLErrors();
#if !defined(NDEBUG)
   SourceLocation& source_location = SourceLocation::instance();
   source_location.push("Status", "setup_text_gl", __LINE__, "", __FILE__);
#endif
   for(size_t i = 0; i < texts.size(); ++i )
   {
      StatusText& statusText = texts[i];
      if (statusText.vbo != 0)
         glDeleteBuffers(1, &statusText.vbo);
      glGenBuffers(1, &statusText.vbo);
      glBindBuffer(GL_ARRAY_BUFFER, statusText.vbo);
      glBufferData(GL_ARRAY_BUFFER, statusText.text.length()*4*5*sizeof(GLfloat), nullptr, GL_STATIC_DRAW);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      if (statusText.ivbo != 0)
         glDeleteBuffers(1, &statusText.ivbo);
      glGenBuffers(1, &statusText.ivbo);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, statusText.ivbo);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, statusText.text.length()*sizeof(STATUS_GLYPH_INDICES), nullptr, GL_STATIC_DRAW);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

      if (statusText.vao != 0)
         glDeleteVertexArrays(1, &statusText.vao);
      glGenVertexArrays(1, &statusText.vao);
      glBindVertexArray(statusText.vao);
      glBindTexture(GL_TEXTURE_2D, atlas->id);
      glBindBuffer(GL_ARRAY_BUFFER, statusText.vbo);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, statusText.ivbo);
   //   int size; glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &size); size/sizeof(GLuint);
      glEnableVertexAttribArray(0);
      glEnableVertexAttribArray (1);
      GLsizei stride = sizeof(GLfloat) * (3 + 2);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, 0);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void *>(3 * sizeof (GLfloat)));

//      glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &size); size/sizeof(GLuint);
      oglutil::clearGLErrors();
      const char *txt = statusText.text.c_str();
      size_t total = 0;
      GLint offset = 0, ioffset = 0;
      float x = ((i == 0) ? 0 : texts[i-1].xend) + statusText.x;
      ftgl::texture_font_load_glyphs(font, statusText.text.c_str());
      for(size_t j = 0; j < statusText.text.length(); ++j )
      {
         texture_glyph_t* glyph = ftgl::texture_font_get_glyph(font, &txt[j]);
//         texture_glyph_t* glyph = ftgl::texture_font_find_glyph(font, &txt[j]);
         if( glyph != nullptr )
         {
            float kerning = 0.0f;
            if( j > 0)
               kerning = ftgl::texture_glyph_get_kerning( glyph, txt + j - 1);
            x += kerning;
            GLfloat x0  = ( x + glyph->offset_x );
            GLfloat x1  = ( x0 + glyph->width );
            GLfloat y0  = ( statusText.y + glyph->offset_y );
            GLfloat y1  = ( y0 - glyph->height );
            float s0 = glyph->s0;
            float t0 = glyph->t0;
            float s1 = glyph->s1;
            float t1 = glyph->t1;

            std::cout << x0 << "," << y0 << " " << x0 << "," << y1 << " " << x1 << "," << y1 << " " << x1 << "," << y0 << std::endl;

   //            GLfloat verts[] = { x0,y0,z,  s0,t0,
   //                                x0,y1,z,  s0,t1,
   //                                x1,y1,z,  s1,t1,
   //                                x1,y0,z,  s1,t0 };
            GLfloat verts[] = { x0,y0,z,  s0,t0,
                                x1,y0,z,  s1,t0,
                                x0,y1,z,  s0,t1,
                                x1,y1,z,  s1,t1,
            };

            glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(verts), &verts[0]);
            offset += sizeof(verts);
            x += glyph->advance_x;
            total += sizeof(verts);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, ioffset, sizeof(STATUS_GLYPH_INDICES), &STATUS_GLYPH_INDICES[0]);
            ioffset += sizeof(STATUS_GLYPH_INDICES);
            if (! oglutil::isGLOk(err, &errs)) std::cerr << err << ": " << errs.str().c_str() << std::endl;
         }
      }
      statusText.xend = x;
      if (! oglutil::isGLOk(err, &errs)) std::cerr << err << ": " << errs.str().c_str() << std::endl;
   //      std::cout << static_cast<void *>(ptr) << " " << static_cast<void *>(&vertices[text.length()*4*5])
   //                << " " << ((uchar *)ptr) - ((uchar *)&vertices[text.length()*4*5]) << std::endl;
      assert(offset == statusText.text.length()*4*5* sizeof(GLfloat));
      glBindVertexArray(0);
      if (! oglutil::isGLOk(err, &errs)) std::cerr << err << ": " << errs.str().c_str() << std::endl;
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
   }
   GLfloat quad[] = { 0,0,   0, height,   width,0,    width,height  };
   glGenBuffers(1, &background_program.GLuint_ref("VBO_BACK"));
   glBindBuffer(GL_ARRAY_BUFFER, background_program.GLuint_get("VBO_BACK"));
   glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glGenVertexArrays(1, &background_program.GLuint_ref("VAO_BACK"));
   if (! oglutil::isGLOk(err, &errs)) std::cerr << err << ": " << errs.str().c_str() << std::endl;
   glBindVertexArray(background_program.GLuint_get("VAO_BACK"));
   if (! oglutil::isGLOk(err, &errs)) std::cerr << err << ": " << errs.str().c_str() << std::endl;
   glBindBuffer(GL_ARRAY_BUFFER, background_program.GLuint_get("VBO_BACK"));

   if (! oglutil::isGLOk(err, &errs))
   {
      std::cerr << err << ": " << errs.str().c_str() << std::endl;
      is_renderable = false;
#if !defined(NDEBUG)
      source_location.pop();
#endif
      return false;
   }
#if !defined(NDEBUG)
   source_location.pop();
#endif
   return true;
}

bool Status::render(const glm::mat4& MVP)
//---------------------------------------
{
   if ( (texts.empty()) || (! is_renderable) ) return false;
   if (requires_setup)
      requires_setup = (! setup_text_gl());
   if ( (atlas->id == 0) && (! create_font_texture()) )
   {
      std::cerr << "Status::initialize - Error setting up font texture" << std::endl;
      return false;
   }
   TimeType now = std::chrono::high_resolution_clock::now();
   long total_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
   if (total_elapsed > timeout)
   {
      clear();
      return true;
   }
#if !defined(NDEBUG)
   SourceLocation& source_location = SourceLocation::instance();
   source_location.push("Status", "render", __LINE__);
#endif
   transparency =  1.0f - (static_cast<float>(total_elapsed) / static_cast<float>(timeout));
//   std::cout << "Timeout " << total_elapsed << " / " << timeout << " = " << transparency << std::endl;
   bool ret = true;
   GLenum err;
   std::stringstream errs;
   oglutil::clearGLErrors();
   glBlendColor( 1, 1, 1, 1 );
   glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
   glEnable(GL_BLEND);
//         glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );

   oglutil::clearGLErrors();
   background_program.activate();
   if (! oglutil::isGLOk(err, &errs)) std::cerr << err << ": " << errs.str().c_str() << std::endl;
   glUniformMatrix4fv(background_program.uniform("MVP"), 1, GL_FALSE, &MVP[0][0]);
   glUniform4f(background_program.uniform("color"), background_colour.r, background_colour.g, background_colour.b,
               transparency);
   if (! oglutil::isGLOk(err, &errs)) std::cerr << err << ": " << errs.str().c_str() << std::endl;
   glUniform2f(background_program.uniform("size"), width, height);
   if (! oglutil::isGLOk(err, &errs)) std::cerr << err << ": " << errs.str().c_str() << std::endl;
   glUniform1f(background_program.uniform("z"), background_z);
   if (! oglutil::isGLOk(err, &errs)) std::cerr << err << ": " << errs.str().c_str() << std::endl;
   glBindVertexArray(background_program.GLuint_get("VAO_BACK"));
   if (! oglutil::isGLOk(err, &errs)) std::cerr << err << ": " << errs.str().c_str() << std::endl;
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   if (! oglutil::isGLOk(err, &errs)) std::cerr << err << ": " << errs.str().c_str() << std::endl;

   text_program.activate();
   glActiveTexture(texture_unit);
   glUniformMatrix4fv(text_program.uniform("MVP"), 1, GL_FALSE, &MVP[0][0]);
   glUniform1i(text_program.uniform("tex"), texture_unit - GL_TEXTURE0);
   glUniform1i(text_program.uniform("isRGB"), is_rgb_font ? 1 : 0);
   for (StatusText& statusText : texts)
   {
      glUniform4f(text_program.uniform("color"), statusText.color.r, statusText.color.g, statusText.color.b, transparency);
      glBindVertexArray(0);
      glBindTexture(GL_TEXTURE_2D, atlas->id);
      glBindVertexArray(statusText.vao);
   //      glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(statusText.text.length() * sizeof(STATUS_GLYPH_INDICES) / sizeof(GLuint)),
   //                     GL_UNSIGNED_INT, 0);
      for (int i=0, offset=0; i<statusText.text.length(); i++)
      {
//         glBindTexture(GL_TEXTURE_2D, atlas->id);
         if (! oglutil::isGLOk(err, &errs))
            std::cout << err << ": " << errs.str() << std::endl;
         glDrawArrays(GL_TRIANGLE_STRIP, offset, 4);
         offset += 4;
//         glBindTexture(GL_TEXTURE_2D, 0);
      }
      if (! oglutil::isGLOk(err, &errs))
      {
         std::cerr << "Status::render: " << err << ": " << errs.str().c_str() << std::endl;
         ret = false;
      }
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glBindTexture(GL_TEXTURE_2D, 0);
   }

   glBlendFunc(GL_ONE, GL_ZERO);
   glBlendColor(0, 0, 0, 0);
   glDisable(GL_BLEND);
#if !defined(NDEBUG)
   source_location.pop();
#endif
   return ret;
}

Status::~Status()
//---------------
{
   if (atlas->id > 0)
   {
      glDeleteTextures(1, &atlas->id);
      atlas->id = 0;
   }
   if (font != nullptr)
      ftgl::texture_font_delete(font);
   if (atlas != nullptr)
      ftgl::texture_atlas_delete(atlas);
   clear();
}


