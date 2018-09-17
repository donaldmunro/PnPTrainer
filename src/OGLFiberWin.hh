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

#ifndef _OGLWINDOW_HH_
#define _OGLWINDOW_HH_
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

#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/string_cast.hpp"

#include <memory>
#include <iostream>
#include <initializer_list>
#include <exception>
#include <utility>
#include <unordered_map>
#include <queue>
#include <atomic>
#include <chrono>

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

#include <boost/fiber/all.hpp>

#include "OGLUtils.h"

namespace std
{
   template<>
   struct default_delete<GLFWwindow>
   {
      void operator()(GLFWwindow* win)
      {
         if (win != nullptr)
            glfwDestroyWindow(win);
      }
   };
}

namespace oglfiber
{
   using TimeType = std::chrono::_V2::system_clock::time_point;

   class OGLFiberExecutor;

   struct KeyPress
   {
      int key =-1, scancode =-1, action =-1, modifiers =-1;

      KeyPress(int key, int scancode, int action, int modifiers) : key(key), scancode(scancode), action(action), modifiers(modifiers) {}
   };

   class OGLFiberWindow
   //===================
   {
   public:
      OGLFiberWindow(std::string title, int w, int h, int gl_major =4, int gl_minor = 4, bool can_resize =true,
                GLFWmonitor *mon =nullptr) : name(std::move(title)), width(w), height(h), ogl_major(gl_major),
                                             ogl_minor(gl_minor), resizable(can_resize), monitor(mon),
                                             window(nullptr)
      { is_good = create(&log); }

      //full screen
      OGLFiberWindow(std::string title, GLFWmonitor *mon, int gl_major=4, int gl_minor =4) :
         name(std::move(title)), width(-1), height(-1), ogl_major(gl_major), ogl_minor(gl_minor),
         resizable(false), monitor(mon), window(nullptr)
      {
         if (monitor == nullptr)
         {
            std::cerr << "Monitor cannot be null for full screen window" << std::endl;
            throw std::logic_error("Monitor cannot be null for full screen window");
         }
         is_good = create(&log);
      }

      virtual ~OGLFiberWindow()
      {
         std::cout << "Destroy OGLFiberWindow " << name << std::endl;
      }

      bool good() { return is_good; }

      std::string messages() { return log.str(); }

      void frames_per_second(long fps_) { fps = fps_; fps_ns = (1000000000L / (fps >> 1)); }

      long frames_per_second() { return fps; }

      virtual void on_initialize(const GLFWwindow*) =0;

      virtual void on_resized(int w, int h) =0;

      virtual bool on_render() =0;

      virtual void on_exit() =0;

      bool last_key(KeyPress& kp)
      {
         if (key_queue.empty()) return false;
         kp = std::move(key_queue.front());
         key_queue.pop();
         return true;
      }

      GLFWwindow* GLFW_win() { return window.get(); }

      void request_context() { if (window) glfwMakeContextCurrent(window.get()); }

      void release_context() { if (glfwGetCurrentContext() == window.get()) glfwMakeContextCurrent(nullptr); }

      void request_focus() { if (window) { glfwShowWindow(window.get()); glfwFocusWindow(window.get()); } }

      friend class OGLFiberExecutor;

   protected:
      bool is_good = false;
      int width =-1, height =-1;
      std::queue<KeyPress> key_queue;

      virtual void onCursorUpdate(double xpos, double ypos) {}
      virtual void on_focus(bool has_focus) {}
      virtual void on_mouse_click(int button, int action, int mods) {}
      virtual void on_mouse_scroll(double x, double y) {}
      virtual void on_key_press(KeyPress& keyPress) {}

   private:
      std::string name;
      int ogl_major, ogl_minor;
      bool resizable;
      GLFWmonitor* monitor = nullptr;
      std::stringstream log;
      long fps = 50;
      long fps_ns = (1000000000L / (fps >> 1));
      OGLFiberExecutor* parent = nullptr;
      std::unique_ptr<GLFWwindow> window{nullptr};
      boost::fibers::fiber_specific_ptr<int> last_error;
      boost::fibers::fiber_specific_ptr<std::string> last_error_msg;

      bool create(std::stringstream* errs =nullptr);
      void run();
   };

   class OGLFiberExecutor
   //=====================
   {
   public:
      static OGLFiberExecutor& instance()
      {
         static OGLFiberExecutor instance;
         return instance;
      }

      /*
      is_threaded - true to run fiber in a different thread.
      */
      bool start(std::initializer_list<OGLFiberWindow *> windows_, bool is_threaded =false);
      bool start(std::initializer_list<std::shared_ptr<OGLFiberWindow>> windows_, bool is_threaded =false);

      void stop() { must_stop.store(true); }
      bool is_stopping() { return must_stop.load(); };
      void join()
      {
         if (thread.joinable())
            thread.join();
         else
            if (main_fiber.joinable())
               main_fiber.join();
      }

   private:
      void run();

      std::thread thread;
      boost::fibers::fiber main_fiber;
      std::vector<std::shared_ptr<OGLFiberWindow>> windows;
      std::vector<std::shared_ptr<boost::fibers::fiber>> fibers;
      size_t running = 0;
      std::atomic_bool must_stop; // atomic so an external thread can also terminate loop
      OGLFiberWindow* current_window = nullptr;

      OGLFiberExecutor()
      //----------------
      {
         if (! glfwInit())
         {
            std::cerr <<  "Error initializing glfw" << std::endl;
            throw std::runtime_error("Error initializing glfw");
         }
         glfwSetErrorCallback(glfw_error);
      }

      ~OGLFiberExecutor()
      {
         glfwTerminate();
      }

      OGLFiberExecutor(const OGLFiberExecutor&)= delete;
      OGLFiberExecutor(const OGLFiberExecutor&&)= delete;
      OGLFiberExecutor& operator=(const OGLFiberExecutor&)= delete;

      static std::unordered_map<GLFWwindow*, OGLFiberWindow*> window_lookup;
      static void glfw_error(int err, const char* errmess);
      static void glfw_on_key(GLFWwindow* win, int key, int scancode, int action, int modifier);
      static void glfw_on_focus(GLFWwindow* win, int has_focus);
      static void glfw_on_cursor_position(GLFWwindow* window, double xpos, double ypos);
      static void glfw_on_button(GLFWwindow* window, int button, int action, int mods);
      static void glfw_on_scroll_wheel(GLFWwindow* window, double xoffset, double yoffset);
      static void glfw_on_size(GLFWwindow* window, int width, int height);
      static void glfw_on_close(GLFWwindow *);

      static const size_t MAX_KEYBUF_SIZE = 100;

      friend class OGLFiberWindow;

      void setup_win(OGLFiberWindow *win);
   };
}
#endif // _POINTCLOUDWIDGET_HH_
