/*
Copyright (c) 2017 Donald Munro

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

#ifndef TRAINER_OGLSHADERUTILS_H
#define TRAINER_OGLSHADERUTILS_H

#include <string>
#include <unordered_map>
#include <algorithm>
#include <regex>
#ifdef STD_FILESYSTEM
#include <filesystem>
namespace filesystem = std::filesystem;
#endif
#ifdef FILESYSTEM_EXPERIMENTAL
#include <experimental/filesystem>
namespace filesystem = std::experimental::filesystem;
#endif
#ifdef FILESYSTEM_BOOST
#include <boost/filesystem.hpp>
namespace filesystem = boost::filesystem;
#endif

#include <GL/gl.h>

namespace oglutil
{
   inline void clearGLErrors() { while (glGetError() != GL_NO_ERROR); }

   bool isGLOk(GLenum& err, std::stringstream *strst =nullptr);

   GLuint compile_shader(std::string source, GLenum type, GLenum& err, std::stringstream *errbuf);

   bool link_shader(const GLuint program, GLenum& err, std::stringstream *errbuf = nullptr);

   GLuint compile_link_shader(const std::string& vertex_source, const std::string& fragment_source,
                              GLuint& vertexShader, GLuint& fragmentShader,
                              GLenum& err, std::stringstream *errbuf = nullptr);

   GLuint compile_link_shader(const std::string& vertex_source, GLuint& vertexShader,
                              const std::string& tess_control_source, GLuint& TessControlShader,
                              const std::string& tess_eval_source, GLuint& tessEvalShader,
                              const std::string& geometry_source, GLuint& geometryShader,
                              const std::string& fragment_source, GLuint& fragmentShader,
                              GLenum& err, std::stringstream *errbuf = nullptr);

   inline std::string emptys(const char *pch) { return ( (pch == nullptr) ? "" : std::string(pch) ); }

   bool load_shaders(const std::string& directory, std::string& vertexShader, std::string& fragmentShader,
                  std::string* geometryShader = nullptr, std::string* tessControlShader = nullptr,
                  std::string* tessEvalShader = nullptr);

   struct OGLProgramUnit
   //===================
   {
      GLuint program =GL_FALSE, vertex_shader =GL_FALSE, tess_control_shader =GL_FALSE,
            tess_eval_shader =GL_FALSE, geometry_shader =GL_FALSE, fragment_shader =GL_FALSE;
      std::stringstream log;
      // For automatic deletion use VAO_, VBO_ name for VAOs and VBOs and TEX_ name for textures
      std::unordered_map<std::string, GLuint> uints;
      std::unordered_map<std::string, GLint> ints;
      std::regex VBOr{R"(.*VBO.*)"}, VBAr{R"(.*VAO.*)"}, TEXr{R"(.*TEX.*)"};

      ~OGLProgramUnit() { delall(); }
      void delall();

      void activate() { glUseProgram(program); }

      GLenum compile_link(const char* vertex = nullptr, const char* fragment = nullptr, const char* geometry = nullptr,
                        const char* tess_control = nullptr, const char* tess_eval = nullptr)
      {
         GLenum err;
         program = oglutil::compile_link_shader(emptys(vertex), vertex_shader, emptys(tess_control), tess_control_shader,
                                       emptys(tess_eval), tess_eval_shader, emptys(geometry), geometry_shader,
                                       emptys(fragment), fragment_shader, err, &log);
         return err;
      }

      GLuint& GLuint_ref(const char* name) { return uints[name]; }

      GLuint GLuint_get(const char* name) const
      //----------------------------------------------
      {
         auto it = uints.find(name);
         if (it != uints.end())
            return it->second;
         return GL_FALSE;
      }

      GLint& GLint_ref(const char*  name) { return ints[name]; }
      GLint GLint_get(const char* name) const
      //--------------------------------------
      {
         auto it = ints.find(name);
         if (it != ints.end())
            return it->second;
         return -1;
      }

      operator bool() const { return program != GL_FALSE; }

      bool del(const std::string& name);

      GLint uniform(std::string name) { return glGetUniformLocation(program, name.c_str()); }

      bool delu(const std::string& k, GLuint* v);
   };
};

#endif //TRAINER_OGLSHADERUTILS_H
