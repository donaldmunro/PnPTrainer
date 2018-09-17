#include <limits.h>
#include <iostream>
#include <iomanip>
#include <memory>
#include <iterator>
#ifdef STDLOCATION
#include <source_location>
namespace source_location = std
#endif
#ifdef LOCATION_EXPERIMENTAL
#include <experimental/source_location>
namespace source_location = std::experimental;
#endif

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <Eigen/Core>
#include <Eigen/Dense>

#ifdef HAVE_SOIL2
#include <SOIL2.h>
#endif

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/norm.hpp"

#include "MatchWin.h"
#include "util.h"
#include "main.hh"
#if !defined(NDEBUG)
#include "SourceLocation.hh"
#endif
#include "ImageWindow.hh"

float MatchWin::angle_incr = glm::radians(0.05f);
float MatchWin::max_phi = glm::radians(120.0f);

MatchWin::MatchWin(const std::string &title, int w, int h, const std::string &shader_dir, bool isBestFeatureOnly,
                   float featureRadius, int glsl_ver, int gl_major, int gl_minor, bool can_resize, GLFWmonitor *mon) :
      oglfiber::OGLFiberWindow(title, w, h, gl_major, gl_minor, can_resize, nullptr), is_best_feature_only(isBestFeatureOnly),
      feature_radius(featureRadius), glsl_ver(glsl_ver), gl_context(*this), caption_writer(GL_TEXTURE1, &gl_context),
      status_info(GL_TEXTURE2, "fonts/Inconsolata-Regular.ttf", 20, false, -0.2f, -0.22f)
//----------------------------------------------------------------------------------------------------
{
   filesystem::path dir = filesystem::canonical(filesystem::path(shader_dir.c_str()));
   is_good = filesystem::is_directory(dir);
   if (! is_good)
   {
      std::cerr << "MatchWin::MatchWin: Invalid shader directory" << shader_dir << std::endl;
      return;
   }
   if (! filesystem::is_directory(dir / filesystem::path("image")))
   {
      std::cerr << "MatchWin::MatchWin: Shader directory " << shader_dir << "/image not found" << std::endl;
      is_good = false;
      return;
   }
   if (! filesystem::is_directory(dir / filesystem::path("cloud")))
   {
      std::cerr << "MatchWin::MatchWin: Shader directory " << shader_dir << "/cloud not found" << std::endl;
      is_good = false;
      return;
   }
   shader_directory = dir;
   is_good = load_font(caption_writer, "large", "fonts/Inconsolata-Regular.ttf", 22, false, "0123456789,.()");
   if (! is_good)
      std::cerr << "Error loading captions font " << "fonts/Inconsolata-Regular.ttf" << std::endl;
//   is_good = load_font(caption_writer, "medium", "fonts/Inconsolata-Regular.ttf", 18, false, "0123456789,.()");
//   is_good = load_font(caption_writer, "small", "fonts/Inconsolata-Regular.ttf", 11, false, "0123456789,.()");
}

void MatchWin::on_initialize(const GLFWwindow *win)
//-------------------------------------------------
{
   is_good = init_shader(shader_directory / filesystem::path("image"), image_unit);
   if (! is_good)
      return;
   is_good = init_shader(shader_directory / filesystem::path("cloud"), pointcloud_unit);
   if (! is_good)
      return;

   GLenum err;
   std::stringstream errs;
   if (! oglutil::isGLOk(err, &errs))
   {
      std::cerr << "MatchWin::on_initialize: Error initializing MatchWin: " << err << ": " << errs.str() << std::endl;
      is_good = false;
   }
   if (! caption_writer.initialize(glsl_ver))
   {
      std::cerr << "MatchWin::on_initialize: Error initializing OpenGLText captions: " << err << ": " << errs.str() << std::endl;
      is_good = false;
   }
   if (! status_info.initialize(glsl_ver))
      std::cerr << "MatchWin::on_initialize: Error initializing Status Info: " << err << ": " << errs.str() << std::endl;

}

void MatchWin::on_resized(int w, int h)
//-------------------------------------------
{
   if ( (window_width == w) && (window_height == h) ) return;
   window_width = w; window_height = h;
   if (image_width <= 0)
   {
      image_width = w / 2;
      image_height = h;
   }
   cloud_start = image_width;
//   P = glm::perspective(glm::radians(45.0f), (float)cloud_width / (float)cloud_height, 0.01f, rangez*3);
//   P = glm::ortho(minx-1, maxx+1, miny-1, maxy+1, 0.01f, rangez*3);
//   IP = glm::inverse(P);
//   if (setup_image_texture()) setup_image_render();
   caption_writer.setViewport(window_width - image_width, window_height);
   status_info.size(window_width - image_width, status_height);
//   status_info.set_timeout(30000);
//   status_info.set("Status:", glm::vec3(1.0, 1.0, 1.0), 10, 20, //);
//                   "Test", glm::vec3(1.0, 1.0, 0.0), 15, 20);
}

bool MatchWin::on_render()
//------------------------
{
   oglutil::clearGLErrors();
   GLenum err;
   std::stringstream errs;
#if !defined(NDEBUG)
   SourceLocation& source_location = SourceLocation::instance();
   source_location.push("MatchWin", "on_render()", __LINE__, "", __FILE__);
#endif
   if (is_image_update.load())
   {
      if (setup_image_texture())
      {
         if (image_unit.GLuint_get("VBO") == GL_FALSE)
         {
            glUseProgram(image_unit.program);
            setup_image_render();
         }
      }
      is_image_update.store(false);
   }
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   glClearColor(0, 0, 0, 1.0);

   if (glIsTexture(image_texture))
   {
#if !defined(NDEBUG)
      source_location.update(__LINE__, "Rendering image texture");
#endif
      glViewport(0, 0, image_width, image_height);
      glDisable(GL_BLEND);
      render_image();
   }
   glEnable(GL_DEPTH_TEST);

   if ( (initialised_pc || updated_pc) && (cloud_count > 0) && ((window_width - image_width) > 0) )
   {
      if ((updated_pc) && (gl_vertices))
      {
         if (!init_pointcloud_buffer())
            std::cerr << "MatchWin::update_points: Error initializing pointcloud buffer" << std::endl;
         updated_pc = false;
         gl_vertices.reset(nullptr);
      }
      else if (is_selection_change)
      {
#if !defined(NDEBUG)
         source_location.update(__LINE__, "Updating selected vertices");
#endif
         cartesian();
         glBindBuffer(GL_ARRAY_BUFFER, pointcloud_unit.GLuint_get("VBO_VERTICES"));
         GLfloat *vertices = static_cast<GLfloat *>(glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
         if (vertices != nullptr)
         {
            if (last_selected_index < cloud_count)
               vertices[last_selected_index * 8 + 3] = points[last_selected_index].second;
            if (selected_index < cloud_count)
               vertices[selected_index * 8 + 3] = 0;
            glUnmapBuffer(GL_ARRAY_BUFFER);
         }
         glBindBuffer(GL_ARRAY_BUFFER, 0);
//         regenerate_captions();
         caption_writer.clear_text();
         caption_writer.add_text("large", centroid_text, centroid.x+0.075f, centroid.y-0.05f, centroid.z+0.01f,
                                 reinterpret_cast<void *>(1));
         is_selection_change = false;
      }
#if !defined(NDEBUG)
      source_location.update(__LINE__, "Point cloud render setup");
#endif
      glViewport(image_width, 0, std::max(window_width - image_width, 0), window_height);
      pointcloud_unit.activate();
      MV = glm::lookAt(location, centroid, tangent);//glm::vec3(0, 1, 0));
      IMV = glm::inverse(MV);
      glUniformMatrix4fv(pointcloud_unit.uniform("MV"), 1, GL_FALSE, &MV[0][0]);
      glUniformMatrix4fv(pointcloud_unit.uniform("P"), 1, GL_FALSE, &P[0][0]);
      glUniform1f(pointcloud_unit.uniform("pointSize"), pointSize);
//      glUniform3f(pointcloud_unit.uniform("light"), location.x, location.y, location.z);
      glUniform3f(pointcloud_unit.uniform("light"), centroid.x, centroid.y, centroid.z);
      glUniform3f(pointcloud_unit.uniform("eye"), location.x, location.y, location.z);
      GLuint vao = pointcloud_unit.GLuint_get("VAO_VERTICES");
      glBindVertexArray(pointcloud_unit.GLuint_get("VAO_VERTICES"));

      //glPointSize(3);
      glEnable(GL_PROGRAM_POINT_SIZE);
#if !defined(NDEBUG)
      source_location.update(__LINE__, "Point cloud render");
#endif
      glDrawArrays(GL_POINTS, 0, cloud_count);
      glBindVertexArray(0);

#if !defined(NDEBUG)
      source_location.update(__LINE__, "Caption render");
#endif
      glViewport(image_width, 0, window_width - image_width, window_height);
//      glm::mat4 R = glm::rotate(glm::mat4(), -theta, glm::vec3(0,1,0));
//      glm::mat4 MM(glm::vec4(1, 0, 0, 0), glm::vec4(0, 1, 0, 0), MV[2], MV[3]);
      glm::vec3 O(0, 0, -1);
      auto calc_MVP = [this](float x, float y, float z, void* param) -> glm::mat4
      {
         glm::mat4 Look = glm::lookAt(glm::vec3(0, 0, -1), glm::vec3(x, y, z), glm::vec3(0, 1, 0));
         glm::mat4 Reflect(glm::vec4(-1, 0, 0, 0), glm::vec4(0, -1, 0, 0), glm::vec4(0, 0, 1, 0),
                           glm::vec4(0, 0, 0, 1));
         glm::mat4 V = Reflect * Look;
         return P * V;

//         unsigned long selected = reinterpret_cast<unsigned long>(param);
//         if (selected == 1)
//         {
//            glm::mat4 Look = glm::lookAt(glm::vec3(0, 0, -1), glm::vec3(x, y, z), glm::vec3(0, 1, 0));
//            glm::mat4 Reflect(glm::vec4(-1, 0, 0, 0), glm::vec4(0, -1, 0, 0), glm::vec4(0, 0, 1, 0),
//                              glm::vec4(0, 0, 0, 1));
//            glm::mat4 V = Reflect * Look;
//            return P * V;
//         }
//         else
//         {
////            glm::vec4 pp = MV * glm::vec4(x, y, z, 1);
//            glm::vec3 N = glm::normalize(location - glm::vec3(x, y, z));
//            glm::vec3 R = glm::normalize(glm::cross(glm::vec3(0, 1, 0), N));
//            glm::vec3 U = glm::normalize(glm::cross(N, R));
//            glm::mat4 V = glm::mat4(glm::vec4(R, 0), glm::vec4(U, 0), glm::vec4(N, 0), glm::vec4(x, y, z, 1));
//            glm::mat4 Reflect(glm::vec4(1, 0, 0, 0), glm::vec4(0, -1, 0, 0), glm::vec4(0, 0, 1, 0),
//                              glm::vec4(0, 0, 0, 1));
//            return P * MV * Reflect;
//         }
      };

      oglutil::clearGLErrors();
      caption_writer.render(calc_MVP, 1, 1, 1, 0.7, &errs);
      if (!oglutil::isGLOk(err, &errs))
         std::cerr << "MatchWin::on_render: captions " << err << ": " << errs.str().c_str() << std::endl;
   }

   if (! oglutil::isGLOk(err, &errs))
   {
      std::cerr << "MatchWin::on_render: " << err << " - " << errs.str().c_str() << " " << std::endl;
      return false;
   }
   glUseProgram(0);
#if !defined(NDEBUG)
   source_location.update(__LINE__, "Rendering status info");
#endif
   if (status_info.must_render())
   {
      float w = static_cast<float>(window_width);
      glViewport(image_width, 0, window_width - image_width, status_height);
      glm::mat4 Ps = glm::ortho(0.0f, w, 0.0f, static_cast<float>(status_height), -1.0f, 1.1f);//, 0.1f, 2.1f);
      glm::mat4 Vs = glm::lookAt(glm::vec3(0, 0, -1), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));
      Vs = glm::mat4();
      glm::mat4 MVPs = Ps * Vs;
      status_info.render(MVPs);
   }
#if !defined(NDEBUG)
   source_location.pop();
#endif
   return true;
}

void MatchWin::onCursorUpdate(double xpos, double ypos)
//-----------------------------------------------------
{
   cursor_pos.first = xpos; cursor_pos.second = ypos;
   if (is_dragging_cloud)
   {
      if (in_cloud)
      {
         cloud_rotation_update(xpos, ypos);
         drag_start_cloud = cursor_pos; // after updates !
      }
   }
   else if (is_dragging_image)
   {
      double dragX = drag_start_image.first, dragY = drag_start_image.second;
      int topy = window_height - image_height;
      dragY -= topy;
      ypos -= topy;
      drag_image_rect = cv::Rect((int) std::min(dragX, xpos), (int) std::min(dragY, ypos),
                                 (int) fabs(dragX - xpos), (int) fabs(dragY - ypos));
      std::cout << "Drag rect " << drag_image_rect << std::endl;
      is_image_update.store(true);
   }

   in_cloud = in_image = in_control = false;
   if (xpos <= image_width)
      in_image = true;
   else
      in_cloud = true;
}

void MatchWin::on_mouse_click(int button, int action, int mods)
//------------------------------------------------------------------
{
   last_button = button; last_button_action = action; last_button_mods = mods;
   double mousex = cursor_pos.first;
   double mousey = cursor_pos.second;
   if ( (last_button == GLFW_MOUSE_BUTTON_RIGHT) && (last_button_action == GLFW_PRESS) )
   {
      if (in_cloud)
         choose_3dpt();
      else
         choose_kp(mousex, mousey, mods);
   }
   else if (last_button == GLFW_MOUSE_BUTTON_LEFT)
   {
      if (in_cloud)
      {
         if (last_button_action == GLFW_PRESS)
         {
            is_dragging = is_dragging_cloud = true;
            is_dragging_image = false; drag_image_rect.width = drag_image_rect.height = 0;
            drag_start_cloud = cursor_pos;
            if (rotating_cursor == nullptr)
            {
               rotating_cursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
               glfwSetCursor(GLFW_win(), rotating_cursor);
            }
         }
         else if (last_button_action == GLFW_RELEASE)
         {
            if (is_dragging_image)
            {
               is_dragging = is_dragging_image = false;
               image_drag_end(std::min(mousex, static_cast<double>(image_width)), mousey, mods);
               return;
            }
            is_dragging = is_dragging_cloud = false;
            if (rotating_cursor != nullptr)
            {
               glfwDestroyCursor(rotating_cursor);
               rotating_cursor = nullptr;
               glfwSetInputMode(GLFW_win(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
         }
      }
      else if (in_image)
      {
         if (last_button_action == GLFW_PRESS)
         {
            is_dragging = is_dragging_image = true;
            drag_start_image = cursor_pos;
         }
         else if (last_button_action == GLFW_RELEASE)
         {
            if (is_dragging_cloud)
            {
               is_dragging = is_dragging_cloud = false;
               if (rotating_cursor != nullptr)
               {
                  glfwDestroyCursor(rotating_cursor);
                  rotating_cursor = nullptr;
                  glfwSetInputMode(GLFW_win(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
               }
               return;
            }
            is_dragging = is_dragging_image = false;
            image_drag_end(mousex, mousey, mods);
            drag_image_rect.x = drag_image_rect.y = drag_image_rect.width = drag_image_rect.height = 0;
         }
      }
   }
//         rotation_update(last_xpos, last_ypos);
}

void MatchWin::cloud_rotation_update(double xpos, double ypos)
//-----------------------------------------------------------
{
//   std::cout << "(" << xpos << ", " << ypos << ")" << std::endl;
   float xdiff = static_cast<float>(xpos - drag_start_cloud.first), ydiff = static_cast<float>(ypos - drag_start_cloud.second);
   if (fabs(xdiff) >= fabs(ydiff))
      theta = add_angle(theta, sgn(xdiff) * angle_incr, 2 * PIf);
   else
      phi = add_angle(phi, sgn(ydiff)*angle_incr, PIf);
   cartesian();
}

bool MatchWin::init_shader(const filesystem::path& dir, oglutil::OGLProgramUnit& unit)
//----------------------------------------------------------------------------------------
{
   GLenum err;
   std::stringstream errs;
   std::string vertex_glsl, fragment_glsl;
   if (!oglutil::load_shaders(dir.string(), vertex_glsl, fragment_glsl))
   {
      std::cerr << "MatchWin::init_shader - Error loading point cloud match shaders from " << dir.string() << std::endl;
      return false;
   }
   unit.program = oglutil::compile_link_shader(replace_ver(vertex_glsl.c_str(), glsl_ver),
                                               replace_ver(fragment_glsl.c_str(), glsl_ver),
                                               unit.vertex_shader,
                                               unit.fragment_shader,
                                               err, &errs);
   if (unit.program ==  GL_FALSE)
   {
      std::cerr << "MatchWin::init_shader" << err << " - " << errs.str().c_str() << std::endl;
      return false;
   }
   errs.str("OpenGL Initialization msg: ");
   if (! oglutil::isGLOk(err, &errs))
   {
      std::cerr << "MatchWin::init_shader: " << errs.str().c_str() << std::endl;
      return false;
   }
   return true;
}

bool MatchWin::setup_image_render()
//---------------------------------
{
   GLenum err;
   std::stringstream errs;
   GLfloat quad[] = { -1,-1,   -1, 1,   1,-1,    1,1  };
//   GLfloat quad[] = { 0,0,   0, image.rows,   image.cols,0,    image.rows,image.cols  };
   if (image_unit.GLuint_get("VBO") != GL_FALSE)
      image_unit.del("VBO");
   glGenBuffers(1, &image_unit.GLuint_ref("VBO"));
   glBindBuffer(GL_ARRAY_BUFFER, image_unit.GLuint_get("VBO"));
   glBufferData(GL_ARRAY_BUFFER, sizeof(quad), nullptr, GL_STATIC_DRAW);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   if (image_unit.GLuint_get("VAO") != GL_FALSE)
      image_unit.del("VAO");

   glGenVertexArrays(1, &image_unit.GLuint_ref("VAO"));
   glBindVertexArray(image_unit.GLuint_get("VAO"));
   glBindBuffer(GL_ARRAY_BUFFER, image_unit.GLuint_get("VBO"));
   glEnableVertexAttribArray(0);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
//   glEnableVertexAttribArray(0);
//   glEnableVertexAttribArray (1);
//   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, 0);
//   glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void *>(2 * sizeof (GLfloat)));
//   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
//   glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
   glBindVertexArray(0);
   glBindBuffer(GL_ARRAY_BUFFER, 0);

   errs.str("OpenGL Initialization msg: ");
   if (! oglutil::isGLOk(err, &errs))
   {
      std::cerr << "MatchWin::setup_image_render: " << err << " - " << errs.str().c_str() <<  std::endl;
      return false;
   }
   return true;
}

inline bool create_image_tex(GLuint image_texture, GLsizei width, GLsizei height, const void *data, GLenum format,
                             GLint internalFormat = GL_RGB, GLint unpack = - 1, GLint rowlen = - 1)
//-----------------------------------------------------------------------------------
{
   GLenum  err;
   std::stringstream errs;
   bool ret = true;
   oglutil::clearGLErrors();
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_RECTANGLE, image_texture);
   glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
   if (unpack > 0)
      glPixelStorei(GL_UNPACK_ALIGNMENT, unpack);
   if (rowlen >= 0)
      glPixelStorei(GL_UNPACK_ROW_LENGTH, rowlen);

   glTexImage2D(GL_TEXTURE_RECTANGLE,         // Type of texture
                0,                   // Pyramid level (for mip-mapping) - 0 is the top level
                internalFormat,      // Internal colour format to convert to
                width, height,
                0,                   // Border width in pixels (can either be 1 or 0)
                format,
                GL_UNSIGNED_BYTE,    // Image data type
                data);
   if (! oglutil::isGLOk(err, &errs))
   {
      std::cerr << "MatchWin::setup_image_texture: " << err << " - " << errs.str().c_str() << std::endl;
      ret = false;
   }
   glBindTexture(GL_TEXTURE_RECTANGLE, 0);
   if (unpack > 0)
      glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
   if (rowlen > 0)
      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
   return ret;
}

#ifdef HAVE_SOIL2
inline void saveTex(GLuint image_texture, GLsizei width, GLsizei  height, int channels, const char* name,
                    GLint pack =-1, GLint rowlen =-1)
//------------------------------------------------------------------------------------------------------
{
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_RECTANGLE, image_texture);
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
   glGetTexImage(GL_TEXTURE_RECTANGLE, 0, format, GL_UNSIGNED_BYTE, data.get());
   if (pack > 0)
      glPixelStorei(GL_PACK_ALIGNMENT, 4);
   if (rowlen > 0)
      glPixelStorei(GL_PACK_ROW_LENGTH, 0);
   SOIL_save_image(name, SOIL_SAVE_TYPE_PNG, width, height, channels,
                   static_cast<const unsigned char *const>(data.get()));
   glBindTexture(GL_TEXTURE_RECTANGLE, 0);
}
#endif

bool MatchWin::setup_image_texture()
//----------------------------------
{
   if (image_texture != 0)
   {
      glDeleteTextures(1, &image_texture);
      image_texture = 0;
   }

   if ( (image.data == nullptr) || (image.cols == 0) || (image.rows == 0) )
   {
      std::cerr << "MatchWin::setup_image_texture: Invalid image " << std::endl;
      return false;
   }
#if !defined(NDEBUG)
   SourceLocation& source_location = SourceLocation::instance();
   source_location.push("MatchWin", "setup_image_texture()", __LINE__, "", __FILE__);
#endif
   cv::Mat display_image = cv::Mat(image.rows, image.cols, image.type());
   image.copyTo(display_image);
   if (selected_features)
   {
      for (features_t* feature : *selected_features)
      {
         cv::KeyPoint &kp = feature->keypoint;
         plot_circles(display_image, kp.pt.x, kp.pt.y);
      }
   }
   std::cout << "Drag rect " << is_dragging_image << " " << drag_image_rect.width << "x" << drag_image_rect.height << std::endl;
   if ( (is_dragging_image) && (drag_image_rect.width >= 5) && (drag_image_rect.height >= 5) )
   {
      plot_rectangles(display_image, drag_image_rect.x, drag_image_rect.y, drag_image_rect.width,
                      drag_image_rect.height);
      cv::Mat _img;
      cv::cvtColor(display_image, _img, CV_BGR2RGB);
      cv::imwrite("displayimage2.png", _img);
   }

   glGenTextures(1, &image_texture);
   image_width = display_image.cols; image_height = display_image.rows; image_channels = display_image.channels();
   GLint align = (display_image.step & 3) ? 1 : 4;
   GLint rowlen = static_cast<GLint>(display_image.step / display_image.elemSize());
   std::cout << "Type " << cvtype(display_image) << " step = " << display_image.step << " "
             <<  display_image.step[0] << " (" << (display_image.step & 3) << ") " << " align " << align
             << " " << display_image.elemSize() << " row len " << rowlen << " " << display_image.isContinuous() << " " << display_image.isSubmatrix() << std::endl;
#if !defined(NDEBUG)
   source_location.update(__LINE__, "Uploading image texture");
#endif
   GLenum  format, internal_format;
   switch (image_channels)
   {
      case 3: internal_format = GL_RGB8; format = (is_BGR) ? GL_BGR : GL_RGB; break;
      case 4: internal_format = GL_RGBA8; format = (is_BGR) ? GL_BGRA : GL_RGBA; break;
      case 1: internal_format = GL_R8; format = GL_RED; break;
      default: internal_format = GL_RGB; format = (is_BGR) ? GL_BGR : GL_RGB; break;
   }
   create_image_tex(image_texture, image_width, image_height, display_image.data, format, internal_format, align,
                    rowlen);
   #if !defined(NDEBUG)
      source_location.pop();
   #endif
//      saveTex(image_texture, image_width, image_height, image_channels, "texture.png", align, rowlen);

   return (glIsTexture(image_texture));
}

#ifdef HAVE_SOIL2
void MatchWin::update_image(const char *imgfile, cv::Rect &rect)
//--------------------------------------------------------------
{
   unsigned char *imgdata = nullptr;
   if (imgfile != nullptr)
   {
      imgdata = SOIL_load_image(imgfile, &image_width, &image_height, &image_channels, SOIL_LOAD_AUTO);
      if (imgdata == nullptr)
      {
         std::cerr << "Error loading " << imgfile << std::endl;
         return;
      }
      if (image_texture != 0)
      {
         glDeleteTextures(1, &image_texture);
         image_texture = 0;
      }
      image_texture = SOIL_create_OGL_texture(static_cast<const unsigned char *const>(imgdata),
                                              &image_width, &image_height, image_channels, 0,
                                              SOIL_FLAG_TEXTURE_RECTANGLE);
      SOIL_free_image_data(imgdata);
      if (image_texture == 0)
      {
         std::cerr << "Error creating texture from " << imgfile << std::endl;
         return;
      }
      image = cv::Mat(image_height, image_width,
                      (image_channels == 3) ? CV_8UC3 : ((image_channels == 4) ? CV_8UC3 : CV_8UC1), imgdata);
      is_image_update.store(true);
   }
}

void MatchWin::update_image(void *imgdata, int width, int height, int channels, cv::Rect &rect)
//----------------------------------------------------------------------------------------------
{
   if (imgdata != nullptr)
   {
      if (image_texture != 0)
      {
         glDeleteTextures(1, &image_texture);
         image_texture = 0;
      }
      image_width = width; image_height = height; image_channels = channels;
      image_texture = SOIL_create_OGL_texture(static_cast<const unsigned char *const>(imgdata),
                                              &image_width, &image_height, image_channels, 0,
                                              SOIL_FLAG_TEXTURE_RECTANGLE);
      if (image_texture == 0)
      {
         std::cerr << "Error creating texture from pointer" << std::endl;
         return;
      }
      image = cv::Mat(image_height, image_width,
                      (image_channels == 3) ? CV_8UC3 : ((image_channels == 4) ? CV_8UC3 : CV_8UC1), imgdata);
      is_image_update.store(true);
   }
}
#endif

void MatchWin::render_image()
//---------------------------
{
   GLuint err;
   std::stringstream errs;
   glUseProgram(image_unit.program);
   glUniform1f(image_unit.uniform("z"), image_depth);
   glUniform2f(image_unit.uniform("uImageSize"), image_width, image_height);
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_RECTANGLE, image_texture);
//   saveTex(image_texture, image_width, image_height, image_channels, "texture2.png", 1);
   glUniform1i(image_unit.uniform("tex"), 0);
   glBindVertexArray(image_unit.GLuint_get("VAO"));
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   if (!oglutil::isGLOk(err, &errs))
      std::cerr << "MatchWin::on_render: " << err << " - " << errs.str().c_str() << " " << std::endl;
}

inline void _push_vertex(GLfloat*& vertices, GLfloat x, GLfloat y, GLfloat z, GLfloat w,
                         GLfloat r, GLfloat g, GLfloat b, GLfloat a)
//--------------------------------------------------------------------------------------
{
   *vertices++ = x; *vertices++ = y; *vertices++ = z; *vertices++ = w;
   *vertices++ = r; *vertices++ = g; *vertices++ = b; *vertices++ = a;
}

void MatchWin::update_points(PointCloudWin* pointSource)
//------------------------------------------------------
{
   initialised_pc = false;
   if (points.size() == 0)
   {
      cloud_count = 0;
      return;
   }
   point_source = pointSource;
   oglutil::clearGLErrors();
   caption_writer.clear_text();

   minx = miny = minz = std::numeric_limits<float>::max();
   maxx = maxy = maxz = std::numeric_limits<float>::lowest();
   float max_distance = 0;
   std::sort(std::begin(points), std::end(points),
             [](const std::pair<Real3<float>, float> &lhs, const std::pair<Real3<float>, float> &rhs) -> bool
             //----------------------------------------------------------------------------------
             {
                return (lhs.second < rhs.second);
             });
   if (points.size() > MAX_POINTS)
   {
      for (size_t i=points.size() - 1; i>=MAX_POINTS; i--)
         points.pop_back();
      points.shrink_to_fit();
   }
   size_t n = points.size();
   for (size_t i=0; i<n; i++)
   {
      GLfloat x, y, z;
      std::pair<Real3<float>, float> item = points[i];
      x = item.first.x;
      y = item.first.y*flip_yz;
      z = item.first.z*flip_yz;
      float distance = item.second;
      if (x < minx) minx = x;
      if (x > maxx) maxx = x;
      if (y < miny) miny = y;
      if (y > maxy) maxy = y;
      if (z < minz) minz = z;
      if (z > maxz) maxz = z;
      if (distance > max_distance) max_distance = distance;
   }
//   rangex = fabsf(maxx - minx); rangey = fabsf(maxy - miny); rangez = fabsf(maxz - minz);
   const size_t buffer_size = n*8;
   gl_vertices = std::make_unique<GLfloat[]>(buffer_size);
   GLfloat* vertices_ptr = gl_vertices.get();

   //Normalize to between -1 and 1 (Calculate a and b in Ax = [-1, 1])
   Eigen::Matrix2f A;
   Eigen::Vector2f b;
   A << minx, 1, maxx, 1;
   b << -1, 1;
   Eigen::Vector2f x = A.colPivHouseholderQr().solve(b);
   ax = x[0]; bx = x[1];
   A << miny, 1, maxy, 1;
   x = A.colPivHouseholderQr().solve(b);
   ay = x[0]; by = x[1];
   A << minz, 1, maxz, 1;
   x = A.colPivHouseholderQr().solve(b);
   az = x[0]; bz = x[1];
#ifndef NDEBUG
   GLfloat *vertices_ptr_end = &vertices_ptr[buffer_size];
#endif
   for (size_t i=0; i<n; i++)
   {
      GLfloat x, y, z, red =1.0f, green =0, blue =0, alpha =1.0f;
      std::pair<Real3<float>, float> item = points[i];
      x = item.first.x*ax + bx;
      y = item.first.y*flip_yz*ay + by;
      z = item.first.z*flip_yz*az + bz;
      float distance = item.second;
      std::stringstream ss;
      ss << std::fixed << std::setprecision(3) << item.first.x << "," << item.first.y << "," << item.first.z;
      if (distance == 0)
      {
         centroid = glm::vec3(x, y, z);
         std::stringstream ss;
         ss << std::fixed << std::setprecision(3) << item.first.x << "," << item.first.y << "," << item.first.z;
         centroid_text = ss.str();
         selected_index = i;
      }
//      else
//         caption_writer.add_text("medium", ss.str(), x, y, z, reinterpret_cast<void *>(0), -1, -1, -1, 0.7);
      if (i < colors.size())
         std::tie(red, green, blue, alpha) = colors[i];
      _push_vertex(vertices_ptr, x, y, z, distance, red, green, blue, alpha);
      display_points.emplace_back(x, y, z);
//      std::cout << std::fixed << std::setprecision(4) << x << "," << y << "," << z << std::endl;
   }
   assert(vertices_ptr == vertices_ptr_end);


   max_r = 2; //std::max(fabsf(minz), fabsf(maxz)); // sqrtf(rangex*rangex + rangey*rangey + rangez*rangez);
   r = 1.0; //max_r/2.0f;
   phi = PIf/2.0f; theta =0;
   cloud_count = n;
   P = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 2.1f);
//   P = glm::frustum(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 2.1f);
   IP = glm::inverse(P);
   updated_pc = true;
   cartesian();
//   std::cout << "Centroid: " << glm::to_string(centroid) << " (" << centroid_text << ")" << std::endl;
   caption_writer.add_text("large", centroid_text, centroid.x+0.075f, centroid.y-0.05f, centroid.z+0.01f,
                           reinterpret_cast<void *>(1));
}

bool MatchWin::init_pointcloud_buffer()
//-------------------------------------
{
   std::stringstream errs;
   GLuint err;
   if (! pointcloud_unit) return false;
   if (! gl_vertices) return false;
   pointcloud_unit.del("VAO_VERTICES");
   if (!oglutil::isGLOk(err, &errs)) std::cerr << "MatchWin::init_pointcloud_buffer: " << err << ": " << errs.str().c_str() << std::endl;
   pointcloud_unit.del("VBO_VERTICES");
   if (!oglutil::isGLOk(err, &errs)) std::cerr << "MatchWin::init_pointcloud_buffer: "<< err << ": " << errs.str().c_str() << std::endl;

   oglutil::clearGLErrors();
   GLuint& vao = pointcloud_unit.GLuint_ref("VAO_VERTICES");
   glGenVertexArrays(1, &vao);
   if (!oglutil::isGLOk(err, &errs)) std::cerr << "MatchWin::init_pointcloud_buffer: " << err << ": " << errs.str().c_str() << std::endl;
   GLuint vao2 = pointcloud_unit.GLuint_get("VAO_VERTICES");
   glBindVertexArray(pointcloud_unit.GLuint_get("VAO_VERTICES"));
   if (! glIsVertexArray(vao))
      std::cerr << "MatchWin::init_pointcloud_buffer: " << "Error generating VAO\n";
   glGenBuffers(1, &pointcloud_unit.GLuint_ref("VBO_VERTICES"));
   glBindBuffer(GL_ARRAY_BUFFER, pointcloud_unit.GLuint_get("VBO_VERTICES"));
   glBufferData(GL_ARRAY_BUFFER, cloud_count*8*sizeof(GLfloat), gl_vertices.get(), GL_DYNAMIC_DRAW);
   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray (1);
   GLsizei stride = sizeof(GLfloat) * (4 + 4);
   glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, 0);
   glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void *>(4 * sizeof (GLfloat)));
   glBindVertexArray(0);
   glBindBuffer(GL_ARRAY_BUFFER, 0);

   errs << "OpenGL msg loading pointcloud vertices: ";
   if (! oglutil::isGLOk(err, &errs))
   {
      std::cerr << "MatchWin::init_pointcloud_buffer: " << errs.str().c_str() << std::endl;
      pointcloud_unit.del("VAO_VERTICES");
      pointcloud_unit.del("VBO_VERTICES");
      initialised_pc = false;
      return false;
   }
   initialised_pc = true;
   return true;
}

void MatchWin::cartesian()
//------------------------
{
   float x = r*sinf(phi)*sinf(theta);
   float y = r*cosf(phi);
   float z = r*sinf(phi)*cosf(theta);
   location = glm::vec3(x, y, z);

   // See tangent.tex/tangent.pdf in project root.
   float r2 = r*r;
   tangent = glm::normalize(glm::vec3(-x*y/r2, -y*y/r2 + 1, -y*z/r2));

//   std::cout << std::fixed << std::setprecision(5) << "location: ("
//             << r << ", " << glm::degrees(theta) << ", " << glm::degrees(phi) << ") = ("
//             << x << ", " << y << ", " << z << ")" << std::endl;

}

void MatchWin::choose_3dpt()
//-----------------------
{
   if (! in_cloud) return;
   float mousex = static_cast<float>(cursor_pos.first) - cloud_start;
   float mousey = static_cast<float>(cursor_pos.second);
   float x = (2.0f * mousex) / static_cast<float>(window_width - image_width) - 1.0f;
   float y = 1.0f - (2.0f * mousey) / static_cast<float>(window_height);
   glm::vec4 img_ray(x, y, -1, 1);
   glm::vec4 eye_ray = IP*img_ray;
   eye_ray = glm::vec4(eye_ray.x, eye_ray.y, -1.0, 0.0);
   glm::vec4 world_ray = IMV * eye_ray;
   glm::vec3 ray = glm::normalize(glm::vec3(world_ray.x, world_ray.y, world_ray.z));

//   GLint mx = static_cast<GLint>(cursor_pos.first);
//   GLint my = static_cast<GLint>(cloud_height - cursor_pos.second);
//   float mz;
//   glReadPixels(mx, my, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &mz);
//   glm::vec3 rray = glm::unProject(glm::vec3(static_cast<float>(mx), static_cast<float>(my), mz), MV, P,
//                                   glm::vec4(cloud_start, 0, cloud_width, cloud_height));
//   ray = glm::normalize(rray);

   glm::vec3 O = location;
   float search_radius = 0.5f;
   size_t hit = std::numeric_limits<size_t>::max();
   float dist = std::numeric_limits<float>::max();
   for (size_t i=0; i<cloud_count; i++)
   {
      if (i == selected_index) continue;
      Real3<float> p = display_points[i];
      glm::vec3 C(p.x, p.y, p.z);
      glm::vec3 OC = O - C;
      float b = glm::dot(ray, OC);
      float c = glm::dot(OC, OC) - search_radius;
      float disc2 = b*b - c;
      if (disc2 >= 0)
      {
         float d = glm::distance(O, C);
         if (d < dist)
         {
            hit = i;
            dist = d;

            Real3<float> pp = display_points[hit];
//            std::cout << "hit " << pp.x << ", " << pp.y << ", " << pp.z << " " << d << std::endl;
         }
      }
//      else
//         std::cout << "miss " << p.x << ", " << p.y << ", " << p.z << " " << glm::distance(O, C) << std::endl;
   }
   if ( (hit < cloud_count) && (hit != selected_index) )
   {
      Real3<float> p = display_points[hit];
      last_selected_index = selected_index;
      selected_index = hit;
      centroid = glm::vec3(p.x, p.y, p.z);
      Real3<float> pp = points[hit].first;
      point_source->on_match_select_change(pp.x, pp.y, pp.z);
      std::stringstream ss;
      ss << std::fixed << std::setprecision(3) << points[hit].first.x << ", " << points[hit].first.y  << ", "
         << points[hit].first.z;
      centroid_text = ss.str();
//      std::cout << "Hit Centroid: " << glm::to_string(centroid) << " (" << centroid_text << ")" << std::endl;

      size_t n = std::min(points.size(), MAX_POINTS);
      for (size_t i = 0; i < n; i++)
      {
         std::pair<Real3<float>, float>& item = points[i];
         float x = item.first.x*ax + bx;
         float y = item.first.y*flip_yz*ay + by;
         float z = item.first.z*flip_yz*az + bz;
         glm::vec3 p(x, y, z);
         if (i == hit)
            item.second = 0;
         else
            item.second = glm::distance2(centroid, p);
      }

      is_selection_change = true;
   }
}

void MatchWin::choose_kp(double mousex, double mousey, int mods)
//--------------------------------------------------------------
{
   if ( (match_features == nullptr) || (match_features->empty()) ) return;
   int topy = window_height - image_height;
   mousey = mousey - topy;
   std::vector<features_t*> clicked_features;
   double dd = feature_radius*feature_radius;
   bool is_ctrl = ((mods & GLFW_MOD_CONTROL) == GLFW_MOD_CONTROL);
   bool is_shift = ((mods & GLFW_MOD_SHIFT) == GLFW_MOD_SHIFT);
   for (size_t i=0; i<match_features->size(); i++)
   {
      features_t& feature = match_features->at(i);
      cv::KeyPoint& kp = feature.keypoint;
      double dx = (kp.pt.x - mousex);
      double dy = (kp.pt.y - mousey);
      double d = dx*dx + dy*dy;
      std::cout << d << " " << dd << std::endl;
      if (d <= dd)
         clicked_features.push_back(&feature);
   }
   const size_t nc = clicked_features.size();
   if (nc > 0)
   {
      if (is_ctrl)
      {
         if (selected_features)
         {
            for (features_t* clicked_feature : clicked_features)
            {
               bool& is_selected = clicked_feature->is_selected;
               if (is_selected)
               {
                  for (size_t i=0; i<selected_features->size(); i++)
                  {
                     features_t* feature = selected_features->at(i);
                     if (feature == clicked_feature)
                     {
                        selected_features->erase(selected_features->begin()+i);
                        is_selected = false;
                        break;
                     }
                  }
               }
            }
         }
      }
      else
      {
         if (! selected_features)
            selected_features = std::make_shared<std::vector<features_t*>>();
         if (is_best_feature_only)
         {
            if (nc > 1)
            {
               std::partial_sort(std::begin(clicked_features), std::begin(clicked_features) + 1,
                                 std::end(clicked_features),
                                 [](const features_t* lhs, const features_t* rhs) -> bool
                                       //----------------------------------------------------------------------------------
                                 {
                                    const cv::KeyPoint& kp1 = lhs->keypoint;
                                    const cv::KeyPoint& kp2 = rhs->keypoint;
                                    return (kp2.response < kp1.response);
                                 });
            }
            features_t* clicked_feature = clicked_features[0];
            bool& is_selected = clicked_feature->is_selected;
            if (! is_selected)
            {
               is_selected = true;
               selected_features->push_back(clicked_feature);
            }
            is_image_update.store(true);
            return;
         }
         for (features_t* clicked_feature : clicked_features)
         {
            bool& is_selected = clicked_feature->is_selected;
            if (! is_selected)
            {
               is_selected = true;
               selected_features->push_back(clicked_feature);
            }
         }
      }
      is_image_update.store(true);
   }
}

void MatchWin::image_drag_end(double mouseX, double mouseY, int mods)
//------------------------------------------------------------------
{
   if ( (match_features == nullptr) || (match_features->empty()) ||
        (drag_image_rect.width < 5) || (drag_image_rect.height < 5) ) return;
   int topy = window_height - image_height;
   mouseY -= topy;
   double dragX = drag_start_image.first, dragY = drag_start_image.second;
   dragY -= topy;
   cv::Rect R((int) std::min(dragX, mouseX), (int) std::min(dragY, mouseY),
              (int) fabs(dragX - mouseX), (int) fabs(dragY - mouseY));
   std::vector<features_t*> clicked_features;
   bool is_ctrl = ((mods & GLFW_MOD_CONTROL) == GLFW_MOD_CONTROL);
   bool is_shift = ((mods & GLFW_MOD_SHIFT) == GLFW_MOD_SHIFT);
   for (size_t i=0; i<match_features->size(); i++)
   {
      features_t& feature = match_features->at(i);
      cv::Point2f pt = feature.keypoint.pt;
      if (R.contains(pt))
         clicked_features.push_back(&feature);
   }
   drag_image_rect.x =drag_image_rect.y = drag_image_rect.width = drag_image_rect.height = 0;
   const size_t nc = clicked_features.size();
   if (nc > 0)
   {
      if (is_ctrl)
      {
         if (selected_features)
         {
            for (features_t *clicked_feature : clicked_features)
            {
               bool &is_selected = clicked_feature->is_selected;
               if (is_selected)
               {
                  for (size_t i = 0; i < selected_features->size(); i++)
                  {
                     features_t *feature = selected_features->at(i);
                     if (feature == clicked_feature)
                     {
                        selected_features->erase(selected_features->begin() + i);
                        is_selected = false;
                        break;
                     }
                  }
               }
            }
         }
      }
      else
      {
         if (! selected_features)
            selected_features = std::make_shared<std::vector<features_t*>>();
         for (features_t* clicked_feature : clicked_features)
         {
            bool& is_selected = clicked_feature->is_selected;
            if (! is_selected)
            {
               is_selected = true;
               selected_features->push_back(clicked_feature);
            }
         }
      }
   }
   is_image_update.store(true);
}

void MatchWin::on_key_press(oglfiber::KeyPress& keyPress)
//-------------------------------------------------------
{
   if (keyPress.action != GLFW_PRESS) return;
   switch (keyPress.key)
   {
      case GLFW_KEY_ENTER:
         if ( (selected_index < cloud_count) && (selected_features) && (selected_features->size() > 0) )
//              && (yesno(image_view, "Match Confirmation", "Confirm Matches (Ctrl-Backspace can undo)")) )
         {
            status_info.set_timeout(10000);
            status_info.set("Match saved (Ctrl-Backspace to undo)", glm::vec3(1.0, 1.0, 0.0), 15, 20);
            matched_features.emplace_back(points[selected_index].first, selected_features);
            selected_features.reset();
            is_image_update.store(true);

         }
         break;
      case GLFW_KEY_BACKSPACE:
         if ( ((keyPress.modifiers & GLFW_MOD_CONTROL) == GLFW_MOD_CONTROL) && (matched_features.size() > 0) )
         {
            matched_features.pop_back();
            status_info.set_timeout(8000);
            status_info.set("Last match undone", glm::vec3(1.0, 1.0, 0.0), 15, 20);
         }
         break;
      case GLFW_KEY_S:
         if ( ((keyPress.modifiers & GLFW_MOD_CONTROL) == GLFW_MOD_CONTROL) && (matched_features.size() > 0) )
         {
            filesystem::path dir, path;
            if (matched_filename.empty())
            {
               dir = filesystem::canonical(filesystem::current_path());
               path = dir / filesystem::path("matches.json");
            }
            else
            {
               path = filesystem::path(matched_filename.c_str());
               if (path.has_parent_path())
                  dir = filesystem::canonical(path.parent_path());
               else
               {
                  dir = filesystem::canonical(filesystem::current_path());
                  path = dir / path;
               }
            }
//            char result[NAME_MAX+1];
            QMetaObject::invokeMethod(image_view, "save_file_dialog",  Qt::BlockingQueuedConnection,
                                      Q_ARG(QString, "Save Matches"), Q_ARG(QString, path.c_str()),
                                      Q_ARG(QString, "Matches (*.json *.xml)"));
            std::string s = image_view->save_dialog_name();// = save_file_GUI_thread(image_view, "Save Matches", path.c_str(), "Matches (*.json *.xml)");
            if (! s.empty())
            {
               matched_filename = s;
               filesystem::path p(matched_filename.c_str());
               std::unique_ptr<MatchIO> match_io;
               if (p.has_extension())
               {
                  std::string ext = p.extension();
                  trim(ext);
                  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                  if (ext == "json")
                     match_io.reset(new JsonMatchIO);
                  else
                     match_io.reset(new XMLMatchIO);
               }
               else
                  match_io.reset(new XMLMatchIO);
               std::stringstream errs;
               errs << "Writing " << matched_filename;
               if (! match_io->write(matched_filename.c_str(), matched_features, &image_view->detector(), true,
                                     true, &errs))
               {
                  QMetaObject::invokeMethod(image_view, "msgbox", Qt::QueuedConnection, Q_ARG(QString, errs.str().c_str()));
                  this->request_focus();
                  status_info.set_timeout(15000);
                  status_info.set("ERROR:", glm::vec3(1.0, 1.0, 1.0), 5, 20,
                                  errs.str().c_str(), glm::vec3(1.0, 0.0, 0.0), 5, 20);
               }
               else
               {
                  this->request_focus();
                  status_info.set_timeout(10000);
                  status_info.set("Saved", glm::vec3(1.0, 1.0, 0.0), 15, 20);
               }
            }
            else
            {
               this->request_focus();
               status_info.set_timeout(10000);
               status_info.set("Not saved", glm::vec3(1.0, 1.0, 1.0), 15, 20);
            }
         }
   }
}

/*
void MatchWin::regenerate_captions()
//----------------------------------
{
   caption_writer.clear_text();
   size_t n = points.size();
   Real3 centroid_item(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
                       std::numeric_limits<float>::quiet_NaN());
   for (size_t i = 0; i < n; i++)
   {
      GLfloat x, y, z, red = 1.0f, green = 0, blue = 0, alpha = 1.0f;
      std::pair<Real3<float>, float> item = points[i];
      x = item.first.x * ax + bx;
      y = item.first.y * flip_yz * ay + by;
      z = item.first.z * flip_yz * az + bz;

      std::stringstream ss;
      ss << std::fixed << std::setprecision(3) << item.first.x << "," << item.first.y << "," << item.first.z;
      if (selected_index != i)
         caption_writer.add_text("medium", ss.str(), x+0.009f, y, z, reinterpret_cast<void *>(0), -1, -1, -1, 0.7);
      else
         centroid_item = item.first;
   }
   std::stringstream ss;
   ss << std::fixed << std::setprecision(3) << centroid_item.x << "," << centroid_item.y << "," << centroid_item.z;
   caption_writer.add_text("large", ss.str(), centroid.x, centroid.y-0.05f, centroid.z-0.05f,
                           reinterpret_cast<void *>(1));
}

*/