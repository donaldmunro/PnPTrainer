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

#include <iosfwd>
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <cstdio>

#ifdef USE_GLEW
#include <GL/glew.h>
#endif
#ifdef USE_GLAD
#include <glad/glad.h>
#endif

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/freeglut.h>

#include "OGLUtils.h"

namespace oglutil
{
   bool isGLOk(GLenum& err, std::stringstream *strst)
   //-------------------------------------------------
   {
      err = glGetError();
      bool ret = (err == GL_NO_ERROR);
      GLenum err2 = err;
      while (err2 != GL_NO_ERROR)
      {
         char *errmsg = (char *) gluErrorString(err);
         if ( (strst != nullptr) && (errmsg != nullptr) )
            *strst << (char *) gluErrorString(err);
         err2 = glGetError();
         if ( (strst != nullptr) && (err2 != GL_NO_ERROR) )
            *strst << std::endl;
      }
      return ret;
   }

   GLuint compile_shader(std::string source, GLenum type, GLenum& err, std::stringstream *errbuf)
   //--------------------------------------------------------------------------------------------
   {
      if (source.size() < FILENAME_MAX)
      {
         try
         {
            filesystem::path file(source);
            if (filesystem::exists(file))
            {
               std::ifstream ifs(source);
               if (! ifs.good())
               {
                  std::cerr << "Error opening (presumed) shader file " << source << std::endl;
                  return GL_FALSE;
               }
//               std::cout << "Compiling shader file " << filesystem::canonical(file).string() << std::endl;
               source = std::string( (std::istreambuf_iterator<char>(ifs) ),
                                     (std::istreambuf_iterator<char>()    ) );
            }
         }
         catch (...)
         {
         }
      }

      char *p = const_cast<char *>(source.c_str());
      GLuint handle = glCreateShader(type);
      glShaderSource(handle, 1, (const GLchar **) &p, nullptr);
      if (! isGLOk(err, errbuf))
         return -1;
      glCompileShader(handle);
      // if (! isGLOk(errbuf))
      //    return -1;
      bool compiled = isGLOk(err, errbuf);
      GLint status[1];
      glGetShaderiv(handle, GL_COMPILE_STATUS, &status[0]);
      if ( (status[0] == GL_FALSE) || (! compiled) )
      {
         if (errbuf != nullptr)
            *errbuf << "Compile msg: " << std::endl << source;
         status[0] = 0;
         glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &status[0]);
         std::unique_ptr<GLchar> logoutput(new GLchar[status[0]]);
         GLsizei loglen[1];
         glGetShaderInfoLog(handle, status[0], &loglen[0], logoutput.get());
         if (errbuf != nullptr)
            *errbuf << std::string(logoutput.get());
         return -1;
      }
      else
      {
         status[0] = 0;
         glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &status[0]);
         std::unique_ptr<GLchar> logoutput(new GLchar[status[0] + 1]);
         GLsizei loglen[1];
         glGetShaderInfoLog(handle, status[0], &loglen[0], logoutput.get());
         std::string logout = std::string(logoutput.get());
         if ( (logout.length() > 0) && (errbuf != nullptr) )
            *errbuf << logout;
      }
      return handle;
   }

   bool link_shader(const GLuint program, GLenum& err, std::stringstream *errbuf)
   //-----------------------------------------------------------------------------
   {
      glLinkProgram(program);
      if (! isGLOk(err, errbuf))
      {
         if (errbuf != nullptr)
            *errbuf << " (Error linking shaders to shader program)";
      }
      GLint status[1];
      glGetProgramiv(program, GL_LINK_STATUS, &status[0]);
      if (status[0] == GL_FALSE)
      {
         if (errbuf != nullptr)
            *errbuf << "Link msg: ";
         status[0] = 0;
         glGetProgramiv(program, GL_INFO_LOG_LENGTH, &status[0]);
         std::unique_ptr<GLchar> logoutput(new GLchar[status[0]]);
         GLsizei loglen[1];
         glGetProgramInfoLog(program, status[0], &loglen[0], logoutput.get());
         if (errbuf != nullptr)
            *errbuf << std::string(logoutput.get());
         return false;
      }
      else
      {
         status[0] = 0;
         glGetProgramiv(program, GL_INFO_LOG_LENGTH, &status[0]);
         std::unique_ptr<GLchar> logoutput(new GLchar[status[0] + 1]);
         GLsizei loglen[1];
         glGetProgramInfoLog(program, status[0], &loglen[0], logoutput.get());
         std::string logout = logoutput.get();
         if ( (logout.length() > 0) && (errbuf != nullptr) )
            *errbuf << "Shader Link Status: " << logout;
      }
      return true;
   }

   GLuint compile_link_shader(const std::string& vertex_source, const std::string& fragment_source,
                              GLuint& vertexShader, GLuint& fragmentShader,
                              GLenum& err, std::stringstream *errbuf)
   //---------------------------------------------------------------------------------------
   {
      GLuint unused;
      return compile_link_shader(vertex_source, vertexShader, "", unused, "", unused, "", unused,
                                 fragment_source, fragmentShader, err, errbuf);
   }

   GLuint compile_link_shader(const std::string& vertex_source, GLuint& vertexShader,
                              const std::string& tess_control_source, GLuint& tessControlShader,
                              const std::string& tess_eval_source, GLuint& tessEvalShader,
                              const std::string& geometry_source, GLuint& geometryShader,
                              const std::string& fragment_source, GLuint& fragmentShader,
                              GLenum& err, std::stringstream *errbuf)
   //---------------------------------------------------------------------------------------
   {
      vertexShader = tessControlShader = tessEvalShader = geometryShader = fragmentShader = 0;
      if (! vertex_source.empty())
      {
         vertexShader = compile_shader(vertex_source, GL_VERTEX_SHADER, err, errbuf);
         if (vertexShader == 0)
            return 0;
      }
      if (! tess_control_source.empty())
      {
         tessControlShader = compile_shader(tess_control_source, GL_TESS_CONTROL_SHADER, err, errbuf);
         if (tessControlShader == 0)
         {
            if (vertexShader) glDeleteShader(vertexShader);
            return 0;
         }
      }
      if (! tess_eval_source.empty())
      {
         tessEvalShader = compile_shader(tess_eval_source, GL_TESS_EVALUATION_SHADER, err, errbuf);
         if (tessEvalShader == 0)
         {
            if (vertexShader) glDeleteShader(vertexShader);
            if (tessControlShader) glDeleteShader(tessControlShader);
            return 0;
         }
      }
      if (! geometry_source.empty())
      {
         geometryShader = compile_shader(geometry_source, GL_GEOMETRY_SHADER, err, errbuf);
         if (geometryShader == 0)
         {
            if (vertexShader) glDeleteShader(vertexShader);
            if (tessControlShader) glDeleteShader(tessControlShader);
            if (tessEvalShader) glDeleteShader(tessEvalShader);
            return 0;
         }
      }
      if (! fragment_source.empty())
      {
         fragmentShader = compile_shader(fragment_source, GL_FRAGMENT_SHADER, err, errbuf);
         if (fragmentShader == 0)
         {
            if (vertexShader) glDeleteShader(vertexShader);
            if (tessControlShader) glDeleteShader(tessControlShader);
            if (tessEvalShader) glDeleteShader(tessEvalShader);
            if (geometryShader) glDeleteShader(geometryShader);
            return 0;
         }
      }

      clearGLErrors();
      GLuint program = glCreateProgram();
      if (program == 0)
      {
         if (errbuf != nullptr)
            *errbuf << "Error creating shader program.";
         if (vertexShader) glDeleteShader(vertexShader);
         if (tessControlShader) glDeleteShader(tessControlShader);
         if (tessEvalShader) glDeleteShader(tessEvalShader);
         if (geometryShader) glDeleteShader(geometryShader);
         if (fragmentShader) glDeleteShader(fragmentShader);
         return 0;
      }
      if (vertexShader)
         glAttachShader(program, vertexShader);
      if (tessControlShader)
         glAttachShader(program, tessControlShader);
      if (tessEvalShader)
         glAttachShader(program, tessEvalShader);
      if (geometryShader)
         glAttachShader(program, geometryShader);
      if (fragmentShader)
         glAttachShader(program, fragmentShader);
      if (! isGLOk(err, errbuf))
      {
         if (errbuf != nullptr)
            *errbuf << "Error attaching shaders to shader program.";
         glDeleteProgram(program);
         if (vertexShader) glDeleteShader(vertexShader);
         if (tessControlShader) glDeleteShader(tessControlShader);
         if (tessEvalShader) glDeleteShader(tessEvalShader);
         if (geometryShader) glDeleteShader(geometryShader);
         if (fragmentShader) glDeleteShader(fragmentShader);
         return 0;
      }
      if (! link_shader(program, err, errbuf))
      {
         glDeleteProgram(program);
         if (vertexShader) glDeleteShader(vertexShader);
         if (tessControlShader) glDeleteShader(tessControlShader);
         if (tessEvalShader) glDeleteShader(tessEvalShader);
         if (geometryShader) glDeleteShader(geometryShader);
         if (fragmentShader) glDeleteShader(fragmentShader);
         return 0;
      }
      return program;
   }

   bool load_shaders(const std::string &directory, std::string &vertexShader, std::string &fragmentShader,
                     std::string *geometryShader, std::string *tessControlShader,
                     std::string *tessEvalShader)
   //----------------------------------------------------------------------------------------------------
   {
      if (!filesystem::is_directory(directory))
      {
         std::cerr << directory << " not found or not a directory." << std::endl;
         return false;
      }
      vertexShader = fragmentShader = "";
      if (geometryShader != nullptr) *geometryShader = "";
      if (tessControlShader != nullptr) *tessControlShader = "";
      if (tessEvalShader != nullptr) *tessEvalShader = "";
      const std::string shader_pattern = R"(.*\.glsl$|.*\.vert$|.*\.frag$|.*\.tess|.*\.geom)";
      std::regex shader_regex(shader_pattern, std::regex_constants::icase);
      std::string vertex_shader, fragment_shader, geometry_shader, tess_eval_shader, tess_control_shader;
      for (const auto &entry : filesystem::directory_iterator(directory))
      {
         std::string filename = filesystem::absolute(entry.path());
         std::string basename = entry.path().filename().string();
         if (std::regex_match(basename, shader_regex))
         {
            std::transform(basename.begin(), basename.end(), basename.begin(), ::tolower);
            std::string ext = entry.path().extension();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if ((ext == ".vert") || (basename.find("vert") != std::string::npos))
               vertex_shader = filename;
            else if ((ext == ".frag") || (basename.find("frag") != std::string::npos))
               fragment_shader = filename;
            else if ((ext == ".geom") || (basename.find("geom") != std::string::npos))
               geometry_shader = filename;
            else if ((ext == ".tess") || (basename.find("tess") != std::string::npos))
            {
               if (basename.find("eval") != std::string::npos)
                  tess_eval_shader = filename;
               else if (basename.find("cont") != std::string::npos)
                  tess_control_shader = filename;
               else
                  std::cerr << "Tesselation match for eval/cont not found.(" << filename << ")" << std::endl;
            }
         }
      }
      if (!vertex_shader.empty())
      {
         std::ifstream ifs(vertex_shader);
         if (!ifs.good()) return false;
         vertexShader = std::string((std::istreambuf_iterator<char>(ifs)),
                                    (std::istreambuf_iterator<char>()));
      }
      if (!fragment_shader.empty())
      {
         std::ifstream ifs(fragment_shader);
         if (!ifs.good()) return false;
         fragmentShader = std::string((std::istreambuf_iterator<char>(ifs)),
                                      (std::istreambuf_iterator<char>()));
      }
      if ((tessControlShader != nullptr) && (!tess_control_shader.empty()))
      {
         std::ifstream ifs(tess_control_shader);
         if (!ifs.good()) return false;
         *tessControlShader = std::string((std::istreambuf_iterator<char>(ifs)),
                                          (std::istreambuf_iterator<char>()));
      }
      if ((tessEvalShader != nullptr) && (!tess_eval_shader.empty()))
      {
         std::ifstream ifs(tess_eval_shader);
         if (!ifs.good()) return false;
         *tessEvalShader = std::string((std::istreambuf_iterator<char>(ifs)),
                                       (std::istreambuf_iterator<char>()));
      }
      if ((geometryShader != nullptr) && (!geometry_shader.empty()))
      {
         std::ifstream ifs(geometry_shader);
         if (!ifs.good()) return false;
         *geometryShader = std::string((std::istreambuf_iterator<char>(ifs)),
                                       (std::istreambuf_iterator<char>()));
      }
      return true;
   }

   bool OGLProgramUnit::delu(const std::string& key, GLuint* v)
   //-------------------------------------------------------
   {
      std::string k;
      std::transform(key.begin(), key.end(), std::back_inserter(k), ::toupper);
      if (std::regex_match(k, VBOr))
      {
         if (glIsBuffer(*v))
            glDeleteBuffers(1, v);
         return true;
      }
      else if (std::regex_match(k, VBAr))
      {
         if (glIsVertexArray(*v))
            glDeleteVertexArrays(1, v);
         return true;
      }
      else if (std::regex_match(k, TEXr))
      {
         glDeleteTextures(1, v);
         return true;
      }
      return false;
   }

   void OGLProgramUnit::delall()
   //------------------------
   {
      if (vertex_shader != GL_FALSE) glDeleteShader(vertex_shader);
      if (tess_control_shader != GL_FALSE) glDeleteShader(tess_control_shader);
      if (tess_eval_shader != GL_FALSE) glDeleteShader(tess_eval_shader);
      if (geometry_shader != GL_FALSE) glDeleteShader(geometry_shader);
      if (fragment_shader != GL_FALSE) glDeleteShader(fragment_shader);
      if (program != GL_FALSE) glDeleteProgram(program);
      for(auto it = uints.begin(); it != uints.end(); ++it)
      {
         std::string& k = const_cast<std::string&>(it->first);
         GLuint& v = uints[k];
         delu(k, &v);
      }
      program = vertex_shader = tess_control_shader = tess_eval_shader = geometry_shader = fragment_shader =GL_FALSE;
      uints.clear();
      ints.clear();
   }

   bool OGLProgramUnit::del(const std::string& name)
   //----------------------------------------------
   {
      auto it = uints.find(name);
      if (it != uints.end())
      {
         std::string& k = const_cast<std::string&>(it->first);
         GLuint& v = uints[k];
         delu(k, &v);
         uints.erase(it);
         return true;
      }
      auto it2 = ints.find(name);
      if (it2 != ints.end())
      {
         ints.erase(it->first);
         return true;
      }
      return false;
   }
}