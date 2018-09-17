#ifndef __AXES_H__
#define __AXES_H__

#include <iostream>
#include <string>
#include <regex>

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

#include "glm/glm.hpp"

#include "OGLUtils.h"

class Axes
{
public:
   bool initialize(int glsl_ver =440)
   //--------------------------------
   {
      GLenum err;
      std::stringstream errs;
      std::regex r(R"(\{\{ver\}\})");
      std::stringstream ss;
      ss << glsl_ver;
      std::string vertex = std::regex_replace(vertex_glsl, r, ss.str());
      std::string fragment = std::regex_replace(fragment_glsl, r, ss.str());
      if (program.compile_link(vertex.c_str(), fragment.c_str()) != GL_NO_ERROR)
      {
         std::cerr << "OpenGLText::compile_glsl - Error compiling OpenGLString shaders" << std::endl
                   << program.log.str() << std::endl;
         return false;
      }

      glGenBuffers(1, &program.GLuint_ref("VBO_AXES"));
      glBindBuffer(GL_ARRAY_BUFFER, program.GLuint_get("VBO_AXES"));
      glBufferData(GL_ARRAY_BUFFER, 36*sizeof(GLfloat), nullptr, GL_STATIC_DRAW);
      glBindBuffer(GL_ARRAY_BUFFER, 0);

      glGenVertexArrays(1, &program.GLuint_ref("VAO_AXES"));
      glBindVertexArray(program.GLuint_get("VAO_AXES"));
      glBindBuffer(GL_ARRAY_BUFFER, program.GLuint_get("VBO_AXES"));
      glEnableVertexAttribArray(0);
      glEnableVertexAttribArray (1);
      GLsizei stride = sizeof(GLfloat) * (3 + 3);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, 0);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void *>(3 * sizeof (GLfloat)));
      glBindVertexArray(0);

      if (! oglutil::isGLOk(err, &errs))
      {
         std::cerr << err << ": " << errs.str().c_str() << std::endl;
         return false;
      }
      return true;
   }

   bool render(const glm::mat4& MV, const glm::mat4& P, const glm::vec3& origin, const GLfloat length,
               const glm::vec3& colorX, const glm::vec3& colorY, const glm::vec3& colorZ)
   //-------------------------------------------------------------------------------------------------------------
   {
      GLenum err;
      std::stringstream errs;
      GLfloat len = length/2;
      GLfloat minx = origin.x - len, maxx = origin.x + len, miny = origin.y - len, maxy = origin.y + len,
            minz = origin.z - len, maxz = origin.z + len;

      oglutil::clearGLErrors();
      GLfloat axes[] =
      { minx, origin.y, origin.z,  colorX.r, colorX.g, colorX.b,  maxx, origin.y,  origin.z, colorX.r, colorX.g, colorX.b,
        origin.x,  miny, origin.z, colorY.r, colorY.g, colorY.b,  origin.x, maxy, origin.z,  colorY.r, colorY.g, colorY.b,
        origin.x,  origin.y, minz, colorZ.r, colorZ.g, colorZ.b,  origin.x, origin.y, maxz,  colorZ.r, colorZ.g, colorZ.b
      };
      glBindBuffer(GL_ARRAY_BUFFER, program.GLuint_get("VBO_AXES"));
      glBufferData(GL_ARRAY_BUFFER, sizeof(axes), axes, GL_STATIC_DRAW);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      if (! oglutil::isGLOk(err, &errs))
      {
         std::cerr << err << ": " << errs.str().c_str() << std::endl;
         return false;
      }
      
      program.activate();
      glm::mat4 MVP = P * MV;
      glUniformMatrix4fv(program.uniform("MVP"), 1, GL_FALSE, &MVP[0][0]);
      if (! oglutil::isGLOk(err, &errs))
         std::cerr << err << ": " << errs.str().c_str() << std::endl;
      if (! oglutil::isGLOk(err, &errs))
         std::cerr << err << ": " << errs.str().c_str() << std::endl;
      glBindVertexArray(program.GLuint_get("VAO_AXES"));
      if (! oglutil::isGLOk(err, &errs))
         std::cerr << err << ": " << errs.str().c_str() << std::endl;
      glDrawArrays(GL_LINES, 0, 6);
      glBindVertexArray(0);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glUseProgram(0);
      return oglutil::isGLOk(err, &errs);
   }

   bool render(const glm::mat4& MV, const glm::mat4& P, const glm::vec3& origin, const GLfloat length,
               const glm::vec3 X, const glm::vec3 Y, const glm::vec3 Z,
               const glm::vec3& colorX, const glm::vec3& colorY, const glm::vec3& colorZ)
   //-------------------------------------------------------------------------------------------------------------
   {
      GLenum err;
      std::stringstream errs;
      GLfloat len = length/2;
      glm::vec3 Xp0 = origin - len*X, Xp1 = origin + len*X, Yp0 = origin - len*Y, Yp1 = origin + len*Y,
                Zp0 = origin - len*Z, Zp1 = origin + len*Z;
      GLfloat minx = Xp0.x, maxx = Xp1.x, miny = Yp0.y, maxy = Yp1.y, minz = Zp0.z, maxz = Zp1.z;
      oglutil::clearGLErrors();
      GLfloat axes[] =
      { minx, origin.y, origin.z,  colorX.r, colorX.g, colorX.b,  maxx, origin.y,  origin.z, colorX.r, colorX.g, colorX.b,
        origin.x,  miny, origin.z, colorY.r, colorY.g, colorY.b,  origin.x, maxy, origin.z,  colorY.r, colorY.g, colorY.b,
        origin.x,  origin.y, minz, colorZ.r, colorZ.g, colorZ.b,  origin.x, origin.y, maxz,  colorZ.r, colorZ.g, colorZ.b
      };
      glBindBuffer(GL_ARRAY_BUFFER, program.GLuint_get("VBO_AXES"));
      glBufferData(GL_ARRAY_BUFFER, sizeof(axes), axes, GL_STATIC_DRAW);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      if (! oglutil::isGLOk(err, &errs))
      {
         std::cerr << err << ": " << errs.str().c_str() << std::endl;
         return false;
      }

      program.activate();
      glm::mat4 MVP = P * MV;
      glUniformMatrix4fv(program.uniform("MVP"), 1, GL_FALSE, &MVP[0][0]);
      if (! oglutil::isGLOk(err, &errs))
         std::cerr << err << ": " << errs.str().c_str() << std::endl;
      if (! oglutil::isGLOk(err, &errs))
         std::cerr << err << ": " << errs.str().c_str() << std::endl;
      glBindVertexArray(program.GLuint_get("VAO_AXES"));
      if (! oglutil::isGLOk(err, &errs))
         std::cerr << err << ": " << errs.str().c_str() << std::endl;
      glDrawArrays(GL_LINES, 0, 6);
      glBindVertexArray(0);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glUseProgram(0);
      return oglutil::isGLOk(err, &errs);
   }

private:
   oglutil::OGLProgramUnit program;
   inline static std::string vertex_glsl = R"(
#version {{ver}} core
uniform mat4 MVP;
layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vColor;
smooth out vec3 color;

void main()
{
   color = vColor;
   gl_Position = MVP * vec4(vPosition, 1.0);
}
)";

   inline static std::string fragment_glsl = R"(
#version {{ver}} core
smooth in vec3 color;
layout(location = 0) out vec4 FragColor;
void main() { FragColor = vec4(color, 1); }

)";
};

#endif