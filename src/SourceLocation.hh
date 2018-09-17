#ifndef __SOURCELOCATION_H__
#define __SOURCELOCATION_H__
#if !defined(NDEBUG)

#include <iostream>
#include <string>
#include <utility>
#include <memory>
#include <stack>

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
#include <GL/freeglut.h>

#include "util.h"

#ifndef _WIN32
#define APIENTRY
#endif

inline void APIENTRY on_opengl_error(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                                     const GLchar* message, const void* param);

struct SourceInfo
//===============
{
   std::string klass, method, file, description;
   int line;

   SourceInfo(std::string klass, std::string method, std::string description = "", const char *fileptr =nullptr,
              int line =-1) : klass(klass), method(method), file((fileptr == nullptr) ? "" :fileptr),
                              description(description), line(line)
   {}

   void set(std::string klass_, std::string method_, std::string description_ = "",
            const char *file_ = nullptr, int line_ =-1)
   //-----------------------------------------------------------------------------------------
   {
      klass = klass_;
      method = method_;
      if (line_ > 0)
         line = line_;
      klass = klass_;
      method = method_;
      description = description_;
      if (file_ != nullptr)
         file = std::string(file_);
   }

   void print(std::ostream& out =std::cout)
   //--------------------------------------
   {
      bool isclass = ! klass.empty(), ismethod = ! method.empty(), isfile = ! file.empty(), isdesc = ! description.empty(),
            isline = (line > 0);
      std::string sline = (line > 0) ? std::to_string(line) : "";
      if ( (! isclass) && (! ismethod) && (! isfile) && (! isdesc) ) return;
      out << klass << ((isclass && ismethod) ? "::" : " ") << method << ((isfile) ? " (" + file : "")
          << ((isfile && isline) ? ":" + sline + ") " : ((isline) ? ("line: " + sline) : ") ")) <<
           ((isdesc) ? " " + description : "") << std::endl;
   }
};

class SourceLocation
//==================
{
public:
   static SourceLocation& instance()
   {
      static SourceLocation me;
      return me;
   }

   void set(std::string klass, std::string method, int line =-1, std::string description = "",
            const char *file = nullptr)
   //-----------------------------------------------------------------------------------------
   {
      if (! info)
         info = std::make_unique<SourceInfo>(klass, method, description, file, line);
      else
         info->set(klass, method, description, file, line);
   }

   void update(int lne =-1, std::string desc ="")
   //----------------------------------------
   {
      if (! info)
         info = std::make_unique<SourceInfo>("", "", desc, nullptr, lne);
      else
      {
         info->description = desc;
         info->line = lne;
      }
   }

   void clear() { info.reset(nullptr); }

   void print(std::ostream& out =std::cout)
   {
      if (info)
         info->print(out);
   }

   void push(std::string klass, std::string method, int line =-1, std::string description = "",
             const char *file = nullptr)
   //---------------------------------------------------------------------------
   {
      if (info)
         stack.push(std::move(info));
      info = std::make_unique<SourceInfo>(klass, method, description, file, line);
   }

   bool pop()
   //--------
   {
      if (stack.empty())
      {
         info.reset(nullptr);
         return false;
      }
      info = std::move(stack.top());
      stack.pop();
      return true;
   }

private:
   SourceLocation()
   {
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS );
      glDebugMessageCallback(on_opengl_error, this);
      glEnable( GL_DEBUG_OUTPUT);
   }
   std::unique_ptr<SourceInfo> info;
   std::stack<std::unique_ptr<SourceInfo>> stack;
};

inline void APIENTRY on_opengl_error(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                                     const GLchar* message, const void* param)
//---------------------------------------------------------------------------------------------------
{
#ifndef SHOW_OTHER
   if (type == GL_DEBUG_TYPE_OTHER) return;
#endif
   std::cout << "---------------------OpenGL Error------------" << std::endl;
   std::cout << "Date:" << datetime(std::chrono::system_clock::now()) << std::endl;
   if (param != nullptr)
   {
      SourceLocation *location = const_cast<SourceLocation*>(reinterpret_cast<const SourceLocation*>(param));
      location->print(std::cout);
   }
   std::cout << "OpenGL Message: "<< message << std::endl;
   std::cout << "type: ";
   switch (type) {
      case GL_DEBUG_TYPE_ERROR:
         std::cout << "ERROR";
         break;
      case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
         std::cout << "DEPRECATED_BEHAVIOR";
         break;
      case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
         std::cout << "UNDEFINED_BEHAVIOR";
         break;
      case GL_DEBUG_TYPE_PORTABILITY:
         std::cout << "PORTABILITY";
         break;
      case GL_DEBUG_TYPE_PERFORMANCE:
         std::cout << "PERFORMANCE";
         break;
      case GL_DEBUG_TYPE_OTHER:
         std::cout << "OTHER";
         break;
   }
   std::cout << std::endl;

   std::cout << "id: " << id << std::endl;
   std::cout << "severity: ";
   switch (severity){
      case GL_DEBUG_SEVERITY_LOW:
         std::cout << "LOW";
         break;
      case GL_DEBUG_SEVERITY_MEDIUM:
         std::cout << "MEDIUM";
         break;
      case GL_DEBUG_SEVERITY_HIGH:
         std::cout << "HIGH";
         break;
   }
   std::cout << std::endl;
   std::cout << "============================================================" << std::endl;
}

#endif
#endif