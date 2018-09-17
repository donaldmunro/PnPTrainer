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

#include "OGLFiberWin.hh"

#include <pthread.h>

#include <iostream>
#include "sstream"
#include <cstring>
#include <fstream>
#include <memory>
#include <limits>
#include <thread>
#include <exception>
#include <algorithm>

namespace oglfiber
{
   void OGLFiberExecutor::glfw_error(int err, const char* errmess)
   //--------------------------------------------------------------
   {
      std::cerr << "GLFW msg " << err << ": " << errmess << std::endl;
      const OGLFiberExecutor& executor = OGLFiberExecutor::instance();
      if (executor.current_window != nullptr)
      {
         executor.current_window->last_error.reset(new int{err});
         executor.current_window->last_error_msg.reset(new std::string(errmess));
      }
   }

   void OGLFiberWindow::run()
   //-----------------------------------------
   {
      GLFWwindow* win = window.get();
      TimeType last_timestamp = std::chrono::high_resolution_clock::now();
      while (! glfwWindowShouldClose(win))
      {
         if ( (parent != nullptr) && (parent->is_stopping()) )
            break;

         glfwMakeContextCurrent(win);
         TimeType timestamp = std::chrono::high_resolution_clock::now();
         if (! on_render())
         {
            if (parent != nullptr)
               parent->stop();
            return;
         }
         glfwSwapBuffers(win);
         glfwMakeContextCurrent(nullptr);
         glfwPollEvents();
         last_timestamp = timestamp;
         long elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(timestamp - last_timestamp).count();
         long dozetime = fps_ns - elapsed;
         if (dozetime > 0)
            boost::this_fiber::sleep_for(std::chrono::nanoseconds(dozetime));
         else
            boost::this_fiber::yield();
      }
      OGLFiberExecutor::instance().running--;
   }

   //Stuff that must be done on the main thread
   void OGLFiberExecutor::setup_win(OGLFiberWindow *window)
   //----------------------------------------------------------
   {
      window->parent = this;
      std::stringstream errs;
      if (! window->window)
      {
         std::cerr << errs.str() << std::endl;
         throw std::runtime_error("Error initializing window " + window->name);
      }
      GLFWwindow* win = window->window.get();
      window_lookup[win] = window;
      glfwSetKeyCallback(win, &glfw_on_key);
      glfwSetFramebufferSizeCallback(win, &glfw_on_size);
      glfwSetWindowFocusCallback(win, glfw_on_focus);
      glfwSetCursorPosCallback(win, glfw_on_cursor_position);
      glfwSetMouseButtonCallback(win, glfw_on_button);
      glfwSetScrollCallback(win, glfw_on_scroll_wheel);
      glfwGetFramebufferSize(win, &window->width, &window->height);
      glfwSetWindowCloseCallback(win, &glfw_on_close);
   }

   bool OGLFiberExecutor::start(std::initializer_list<OGLFiberWindow *> windows_, bool is_threaded)
   //------------------------------------------------------------------------
   {
      for (OGLFiberWindow *window : windows_)
      {
         windows.emplace_back(window);
         setup_win(window);
      }
      if (is_threaded)
         thread = std::thread(&OGLFiberExecutor::run, this);
      else
      {
         main_fiber = boost::fibers::fiber(&OGLFiberExecutor::run, this);
         main_fiber.join();
      }
      return true;
   }

   bool OGLFiberExecutor::start(std::initializer_list<std::shared_ptr<OGLFiberWindow>> windows_, bool is_threaded)
   //---------------------------------------------------------------------------------------------------------
   {
      windows.insert(windows.end(), windows_.begin(), windows_.end());
      std::for_each(windows.begin(), windows.end(),
                    [this](const std::shared_ptr<OGLFiberWindow> window) { this->setup_win(window.get()); });
      if (is_threaded)
         thread = std::thread(&OGLFiberExecutor::run, this);
      else
         main_fiber = boost::fibers::fiber(&OGLFiberExecutor::run, this);
      return true;
   }

   std::unordered_map<GLFWwindow*, OGLFiberWindow*> OGLFiberExecutor::window_lookup;

   void OGLFiberExecutor::run()
   //---------------------------
   {
      for (const std::shared_ptr<OGLFiberWindow>& window : windows)
      {
         GLFWwindow* win = window->window.get();
         glfwMakeContextCurrent(win);
#ifdef USE_GLEW
         if (glewInit() != GLEW_OK)
         {
            std::cerr <<  "Error initializing GLEW" << std::endl;
            throw std::runtime_error("Error initializing GLEW");
         }
#endif
#ifdef USE_GLAD
         if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress))
         {
            std::cerr << "Error initializing GLAD" << std::endl;
            throw std::runtime_error("Error initializing GLAD");
         }
#endif
         std::cout << "OpenGL: " << ((const char *)glGetString(GL_VENDOR)) << " "
                   << ((const char *)glGetString(GL_RENDERER)) << " "
                   << ((const char *)glGetString(GL_VERSION)) << " (GLSL "
                   << ((const char *)glGetString(GL_SHADING_LANGUAGE_VERSION)) << ")\n";
         window->on_initialize(win);
         window->on_resized(window->width, window->height);
         glfwMakeContextCurrent(nullptr);

         boost::fibers::fiber* pfiber = new boost::fibers::fiber(std::bind(&OGLFiberWindow::run, window));
         std::shared_ptr<boost::fibers::fiber> fiber(pfiber);
         fiber->detach();
         fibers.push_back(fiber);

   //      fibers.emplace_back(std::bind(&OGLWindow::run, window));
      }

      must_stop.store(false);
      while (! must_stop.load())
         boost::this_fiber::sleep_for(std::chrono::milliseconds(300));
      for (const std::shared_ptr<OGLFiberWindow>& window : windows)
      {
         glfwSetWindowShouldClose(window->window.get(), GLFW_TRUE);
         boost::this_fiber::yield();
      }
      for (const std::shared_ptr<OGLFiberWindow>& window : windows)
      {
         window->on_exit();
         boost::this_fiber::yield();
      }
   }

   void OGLFiberExecutor::glfw_on_key(GLFWwindow* win, int key, int scancode, int action, int modifier)
   //---------------------------------------------------------------------------------------------------
   {
      auto it = window_lookup.find(win);
      if (it != window_lookup.end())
      {
         OGLFiberWindow* window = it->second;
         window->key_queue.emplace(key, scancode, action, modifier);
         if (window->key_queue.size() > MAX_KEYBUF_SIZE)
            window->key_queue.pop();
         window->on_key_press(window->key_queue.back());
      }
   }

   void OGLFiberExecutor::glfw_on_size(GLFWwindow *win, int width, int height)
   //----------------------------------------------------------------------------
   {
      auto it = window_lookup.find(win);
      if (it != window_lookup.end())
      {
         OGLFiberWindow* window = it->second;
         //glfwGetFramebufferSize(win, &window.width, &window.height);
         window->width = width;
         window->height = height;
         glfwMakeContextCurrent(win);
         window->on_resized(width, height);
         glfwMakeContextCurrent(nullptr);
      }
   }

   void OGLFiberExecutor::glfw_on_close(GLFWwindow *)
   //------------------------------------------------
   {
      OGLFiberExecutor& executor = OGLFiberExecutor::instance();
      executor.must_stop.store(true);
   }

   void OGLFiberExecutor::glfw_on_focus(GLFWwindow* win, int has_focus)
   //------------------------------------------------------------------
   {
      auto it = window_lookup.find(win);
      if (it != window_lookup.end())
      {
         OGLFiberWindow* window = it->second;
         window->on_focus(has_focus == GLFW_TRUE);
      }
   }

   void OGLFiberExecutor::glfw_on_cursor_position(GLFWwindow *win, double xpos, double ypos)
   //------------------------------------------------------------------------------------------
   {
      auto it = window_lookup.find(win);
      if (it != window_lookup.end())
      {
         OGLFiberWindow* window = it->second;
         window->onCursorUpdate(xpos, ypos);
      }
   }

   void OGLFiberExecutor::glfw_on_button(GLFWwindow* win, int button, int action, int mods)
   //-----------------------------------------------------------------------------------------
   {
      auto it = window_lookup.find(win);
      if (it != window_lookup.end())
      {
         OGLFiberWindow* window = it->second;
         window->on_mouse_click(button, action, mods);
      }
   }

   void OGLFiberExecutor::glfw_on_scroll_wheel(GLFWwindow* win, double xoffset, double yoffset)
   //------------------------------------------------------------------------------------------
   {
      auto it = window_lookup.find(win);
      if (it != window_lookup.end())
      {
         OGLFiberWindow* window = it->second;
         window->on_mouse_scroll(xoffset, yoffset);
      }
   }

   bool OGLFiberWindow::create(std::stringstream* errs)
   //---------------------------------------------
   {
      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, ogl_major);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, ogl_minor);
      glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
      glfwWindowHint(GLFW_RESIZABLE, ((resizable) ? GL_TRUE : GL_FALSE));
      glfwWindowHint(GLFW_SAMPLES, 4);
      const std::string title = ((name.empty()) ? "" : name);
      const GLFWvidmode* mode;
      if (monitor != nullptr)
      {
         mode = glfwGetVideoMode(monitor);
         glfwWindowHint(GLFW_RED_BITS, mode->redBits);
         glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
         glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
         glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
      }
      else
         mode = nullptr;
      GLFWwindow* win = nullptr;
      if ( (width > 0) && (height > 0) )
         win = glfwCreateWindow(width, height, title.c_str(), monitor, nullptr);
      else if (mode != nullptr)
      {
         if (width <= 0) width = mode->width;
         if (height < 0) height = mode->height;
         win = glfwCreateWindow(width, height, title.c_str(), monitor, nullptr);
         glfwSetWindowMonitor(win, monitor, 0, 0, width, height, mode->refreshRate);
      }
      else
      {
         if (errs)
            *errs << "Window initialization msg for window " << name << ". Width/height not set and no monitor set"
                  << std::endl;
         return false;
      }
      if (win != nullptr)
         window.reset(win);
      else
      {
         if (errs)
            *errs << "Error creating glfw window " << title << std::endl;
         return false;
      }
      return true;
   }
}