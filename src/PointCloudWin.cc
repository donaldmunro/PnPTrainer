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

#include "PointCloudWin.h"

#include <iomanip>
#include <vector>
#include <regex>
#include <sstream>
#include <fstream>
#include <regex>
#include <memory>
#include <algorithm>
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

#include <assert.h>

#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/quaternion.hpp"

#include "tinyply.h"
#include "OGLUtils.h"
#include "util.h"

//#define BOUNDS_VERTICES 1

float PointCloudWin::angle_incr = glm::radians(0.05f);
float PointCloudWin::max_phi = glm::radians(120.0f);

PointCloudWin::PointCloudWin(std::string title, int w, int h, const std::string& shader_dir,
                               const std::string& plyfilename, MatchWin* matchWin,
                               const float scale_, bool yz_flip_, bool is_mean_center_,
                             int glsl_ver, int gl_major, int gl_minor, bool can_resize) :
      oglfiber::OGLFiberWindow(title, w, h, gl_major, gl_minor, can_resize, nullptr), match_window(matchWin),
      glsl_ver(glsl_ver), scale(scale_), yz_flip(yz_flip_), points(scale_, yz_flip_), mean_center(is_mean_center_)
//--------------------------------------------------------------------------------------------------------------------
{
   filesystem::path dir(shader_dir);
   is_good = filesystem::is_directory(dir);
   if (! is_good)
   {
      std::cerr << "Invalid shader directory" << shader_dir << std::endl;
      return;
   }
   if (! filesystem::is_directory(dir / filesystem::path("axes")))
   {
      std::cerr << "Shader directory" << shader_dir << "/axes not found" << std::endl;
      is_good = false;
      return;
   }
   if (! filesystem::is_directory(dir / filesystem::path("cloud")))
   {
      std::cerr << "Shader directory" << shader_dir << "/cloud not found" << std::endl;
      is_good = false;
      return;
   }
   shader_directory = dir;
   filesystem::path p(plyfilename);
   if (! filesystem::is_regular_file(filesystem::canonical(p)))
   {
      std::cerr << "Ply file " << filesystem::canonical(p) << " not valid." << std::endl;
      is_good = false;
      return;
   }
   else
   {
      std::ifstream ifs(filesystem::canonical(p));
      is_good = ifs.good();
      if (! is_good)
      {
         std::cerr << "Ply file " << filesystem::canonical(p) << " not readable" << std::endl;
         return;
      }
   }
   plyfile = p;
}

void PointCloudWin::on_initialize(const GLFWwindow *win)
//------------------------------------------------------
{
   glClearColor(0.0, 0.0, 0.0, 0.0);
   glEnable(GL_DEPTH_TEST);
   GLenum err;
   std::stringstream errs;
   std::string vertex_glsl, fragment_glsl;
   filesystem::path dir = shader_directory / filesystem::path("axes");
   bool is_axes = true;
   if (oglutil::load_shaders(dir.string(), vertex_glsl, fragment_glsl))
   {
      axes_unit.program = oglutil::compile_link_shader(replace_ver(vertex_glsl.c_str(), glsl_ver),
                                                       replace_ver(fragment_glsl.c_str(), glsl_ver),
                                                       axes_unit.vertex_shader, axes_unit.fragment_shader,
                                                       err, &errs);
      if (axes_unit.program ==  GL_FALSE)
      {
         std::cerr << "Error linking Axis shader program:" << err << ": " << errs.str();
         is_axes = false;
      }
      else
      {
         if (axes_unit.uniform("MVP") == -1)
         {
            is_axes = false;
            std::cerr << "Error linking Axis shader program:" << err << ": " << errs.str();
         }
      }
   }
   else
   {
      std::cerr << "Axes shader directory " << dir.string() << " msg. Axes will not be displayed." << std::endl;
      is_axes = false;
   }

   dir = shader_directory / filesystem::path("cloud");
   if (! oglutil::load_shaders(dir.string(), vertex_glsl, fragment_glsl))
   {
      std::cerr << "Error loading point cloud shaders from " << dir.string() << std::endl;
      is_good = false;
      return;
   }
   pointcloud_unit.program = oglutil::compile_link_shader(replace_ver(vertex_glsl.c_str(), glsl_ver),
                                                          replace_ver(fragment_glsl.c_str(), glsl_ver),
                                                          pointcloud_unit.vertex_shader,
                                                          pointcloud_unit.fragment_shader,
                                                          err, &errs);
   if (pointcloud_unit.program ==  GL_FALSE)
   {
      std::cerr << "Error linking shader program:" << err << ": " << errs.str();
      is_good = false;
      return;
   }
   if ( (pointcloud_unit.uniform("MV") == -1) || (pointcloud_unit.uniform("P") == -1) ||
        (pointcloud_unit.uniform("pointSize") == -1))
   {
      std::cerr << "Error linking shader program:" << err << ": " << errs.str();
      is_good = false;
      return;
   }

   errs.str("OpenGL Initialization msg: ");
   if (! oglutil::isGLOk(err, &errs))
   {
      std::cerr << errs.str().c_str() << std::endl;
      exit(1);
   }
   initialised_pc = init_pointcloud();
   if (! initialised_pc)
   {
      std::cerr << "Error initializing point cloud buffers from " << plyfile.string() << std::endl;
      is_good = false;
      return;
   }
   if (is_axes)
      initialised_axes = init_axes();
   else
      initialised_axes = false;

   glfwSetInputMode(GLFW_win(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
//   glfwSetInputMode(GLFW_win(), GLFW_STICKY_MOUSE_BUTTONS, 1);
}

void PointCloudWin::on_resized(int w, int h)
//-------------------------------------------
{
   width = w;
   height = h;
   glViewport(0, 0, width, height);
//      P = glm::infinitePerspective(glm::radians(45.0f), (float)width / (float)height, 0.1f);
   P = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.01f, rangez*3);
//   P = glm::ortho(minx-1, maxx+1, miny-1, maxy+1, 0.01f, rangez*3);
   IP = glm::inverse(P);
}

bool PointCloudWin::on_render()
//---------------------
{
   oglutil::clearGLErrors();
   GLenum err;
   std::stringstream errs;
   glEnable(GL_DEPTH_TEST);
   glClearColor(0, 0, 0, 1.0);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   MV = glm::lookAt(location, centroid, tangent);//glm::vec3(0, 1, 0));
   IMV = glm::inverse(MV);
   if (initialised_axes)
   {
      axes_unit.activate();
      glm::mat4 MVP = P * MV;
      glUniformMatrix4fv(axes_unit.uniform("MVP"), 1, GL_FALSE, &MVP[0][0]);
      if (! oglutil::isGLOk(err, &errs))
         std::cerr << err << ": " << errs.str().c_str() << std::endl;
      if (! oglutil::isGLOk(err, &errs))
         std::cerr << err << ": " << errs.str().c_str() << std::endl;
      glBindVertexArray(axes_unit.GLuint_get("VAO_AXES"));
      if (! oglutil::isGLOk(err, &errs))
         std::cerr << err << ": " << errs.str().c_str() << std::endl;
      glDrawArrays(GL_LINES, 0, 6);
      glBindVertexArray(0);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glUseProgram(0);
   }
   if (initialised_pc)
   {
      if (is_selection_change)
      {
         glBindBuffer(GL_ARRAY_BUFFER, pointcloud_unit.GLuint_get("VBO_VERTICES"));
         GLfloat sel;
         GLfloat* vertices = static_cast<GLfloat *>(glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
         if (! oglutil::isGLOk(err, &errs))
            std::cout << err << ": " << errs.str() << std::endl;
         if (match_window != nullptr)
            match_window->clear_points(points.flip);
         for (size_t j =0; j<points.pts.size(); j++)
         {
            auto it = selected.find(j);
            if (it == selected.end())
               sel = 0;
            else
            {
               if (match_window != nullptr)
               {
                  Real3<float> pt = points.pts[j];
                  match_window->add_point(pt, it->second);
               }
               if (it->second == 0)
                  sel = 2;
               else
                  sel = 1;
            }
            if (vertices == nullptr)
               glBufferSubData(GL_ARRAY_BUFFER, j * sizeof(GLfloat) * 8 + sizeof(GLfloat) * 3, sizeof(GLfloat),
                               &sel);
            else
               vertices[j * 8 + 3] = sel;
         }
         if (!oglutil::isGLOk(err, &errs)) std::cerr << err << ": " << errs.str().c_str() << std::endl;
         is_selection_change = false;
         if (vertices != nullptr)
            glUnmapBuffer(GL_ARRAY_BUFFER);
         if (!oglutil::isGLOk(err, &errs)) std::cerr << err << ": " << errs.str().c_str() << std::endl;
         glBindBuffer(GL_ARRAY_BUFFER, 0);
         if (!oglutil::isGLOk(err, &errs)) std::cerr << err << ": " << errs.str().c_str() << std::endl;
         if (match_window != nullptr)
            match_window->update_points(this);
         if (!oglutil::isGLOk(err, &errs)) std::cerr << err << ": " << errs.str().c_str() << std::endl;
      }
      pointcloud_unit.activate();
      glUniformMatrix4fv(pointcloud_unit.uniform("MV"), 1, GL_FALSE, &MV[0][0]);
      if (!oglutil::isGLOk(err, &errs)) std::cerr << err << ": " << errs.str().c_str() << std::endl;
      glUniformMatrix4fv(pointcloud_unit.uniform("P"), 1, GL_FALSE, &P[0][0]);
      if (!oglutil::isGLOk(err, &errs)) std::cerr << err << ": " << errs.str().c_str() << std::endl;
      glUniform1f(pointcloud_unit.uniform("pointSize"), pointSize);
      glBindVertexArray(pointcloud_unit.GLuint_get("VAO_VERTICES"));
      //glPointSize(3);
      glEnable(GL_PROGRAM_POINT_SIZE);
      glDrawArrays(GL_POINTS, 0, count);
      glBindVertexArray(0);
#ifdef PCW_DEBUG_SHADER
      const glm::vec3 close_color = glm::vec3(1, 0, 0);
      const glm::vec3 far_color = glm::vec3(0, 0, 0.25);
      const glm::vec4 eye_Position =  MV * glm::vec4(location, 1.0);
      for (const glm::vec3& vPosition : _vertices_)
      {
         glm::vec4 position = MV * glm::vec4(vPosition, 1.0);
         float d = glm::distance(eye_Position, position);
         float scale = static_cast<float>(glm::clamp(1.0 - (d / maxDistance), 0.1, 1.0));
         glm::vec3 colour = glm::mix(close_color, far_color, scale);
         std::cout << glm::to_string(eye_Position) << std::endl;
         std::cout << glm::to_string(position) << " " << glm::to_string(eye_Position) << " "
                   << d << " " << scale << " " << glm::to_string(colour) << std::endl;
      }
#endif

//      for (const glm::vec3& item : vertices)
//      {
//         glm::vec4 wc = MVP*glm::vec4(item.x, item.y, item.z, 1);
//         wc /= wc[3];
//         std::cout << "(" << item.x << ", " << item.y << ", " << item.z << ") = "
//                   << glm::to_string(wc) << std::endl;
//      }
   }

   if (! oglutil::isGLOk(err, &errs))
      std::cerr << err << ": " << errs.str().c_str() << std::endl;
   return true;
}

bool PointCloudWin::init_axes()
//----------------------------------
{
   axes_unit.del("VAO_AXES");
   axes_unit.del("VBO_AXES");

   oglutil::clearGLErrors();
   GLfloat ax = centroid.x, ay = centroid.y, az = centroid.z;
   GLfloat axes[] =
         { minx, ay, az, 1.0f, 1.0f, 0.0f,   maxx, ay,  az, 1.0f, 1.0f, 0.0f,
           ax,  miny, az, 0.0f, 1.0f, 0.0f,  ax, maxy, az, 0.0f, 1.0f, 0.0f,
           ax,  ay, minz, 0.0f, 0.0f, 1.0f,  ax, ay, maxz, 0.0f, 0.0f, 1.0f
         };
   glGenBuffers(1, &axes_unit.GLuint_ref("VBO_AXES"));
   glBindBuffer(GL_ARRAY_BUFFER, axes_unit.GLuint_get("VBO_AXES"));
   glBufferData(GL_ARRAY_BUFFER, sizeof(axes), axes, GL_STATIC_DRAW);
   glBindBuffer(GL_ARRAY_BUFFER, 0);

   glGenVertexArrays(1, &axes_unit.GLuint_ref("VAO_AXES"));
   glBindVertexArray(axes_unit.GLuint_get("VAO_AXES"));
   glBindBuffer(GL_ARRAY_BUFFER, axes_unit.GLuint_get("VBO_AXES"));
   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray (1);
   GLsizei stride = sizeof(GLfloat) * (3 + 3);
   glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, 0);
   glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void *>(3 * sizeof (GLfloat)));
   glBindVertexArray(0);
   GLenum err;
   std::stringstream errs;
   if (! oglutil::isGLOk(err, &errs))
   {
      std::cerr << err << ": " << errs.str().c_str() << std::endl;
      return false;
   }
   return true;
}

inline void _push_vertex(GLfloat*& vertices, GLfloat x, GLfloat y, GLfloat z, GLfloat w,
                         GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
   *vertices++ = x; *vertices++ = y; *vertices++ = z; *vertices++ = w;
   *vertices++ = r; *vertices++ = g; *vertices++ = b; *vertices++ = a;
}

std::unique_ptr<GLfloat[]> PointCloudWin::load_pointcloud()
//---------------------------------------------------------
{
   if (plyfile.empty()) return nullptr;
   std::ifstream ifs(plyfile.c_str(), std::ios::binary);
   if (ifs.fail())
   {
      std::cerr << "Could not open pointcloud file " << plyfile.filename() << std::endl;;
      initialised_pc = false;
      return nullptr;
   }
   tinyply::PlyFile file;
   std::shared_ptr<tinyply::PlyData> verts, colors;
   try
   {
      if (! file.parse_header(ifs))
      {
         std::cerr << "Could not parse pointcloud file header for " << plyfile.filename() << std::endl;
         initialised_pc = false;
         return nullptr;
      }
      for (auto e : file.get_elements())
      {
         if (e.name == "vertex")
         {
            for (auto p : e.properties)
            {
               if ( (p.name == "red") || (p.name == "green") || (p.name == "blue") )
                  is_color_pointcloud = true;
               if (p.name == "alpha")
                  is_alpha_pointcloud = true;
            }
         }
      }

      verts = file.request_properties_from_element("vertex", { "x", "y", "z" });
      try
      {
         if (is_alpha_pointcloud)
            colors = file.request_properties_from_element("vertex", {"red", "green", "blue", "alpha"});
         else
            colors = file.request_properties_from_element("vertex", {"red", "green", "blue"});
      }
      catch (const std::exception & e)
      {
         is_color_pointcloud = is_alpha_pointcloud = false;
         std::cerr << "Could not read colors from pointcloud file " << plyfile.filename() << std::endl;
      }
      file.read(ifs);
   }
   catch (const std::exception & e)
   {
      std::cerr << "Exception: " << e.what() << " reading ply file " << plyfile.filename() << std::endl;
      initialised_pc = false;
      return nullptr;
   }
   if ( (! verts) || (verts->count == 0) )
   {
      std::stringstream ss;
      ss << "No vertices in file " << plyfile.filename();
      std::cerr << ss.str().c_str() << std::endl;
      initialised_pc = false;
      return nullptr;
   }
   if ( (! colors) || (colors->count == 0) )
      is_color_pointcloud = is_alpha_pointcloud = false;

//   size_t bytes = verts->buffer.size_bytes();
#ifdef BOUNDS_VERTICES
   count = verts->count + 8;
#else
   count = verts->count;
#endif
   struct RGB { u_char r,g,b; };
   struct RGBA { u_char r,g,b,a; };
   std::vector<GLfloat> Xs, Ys, Zs;
   Real3<float>* vertdata = reinterpret_cast<Real3<float> *>(verts->buffer.get());
   RGB* RGBdata = nullptr;
   RGBA* RGBAdata = nullptr;
   if (is_color_pointcloud)
   {
      if (is_alpha_pointcloud)
         RGBAdata = reinterpret_cast<RGBA *>(colors->buffer.get());
      else
         RGBdata = reinterpret_cast<RGB *>(colors->buffer.get());
   }

   const size_t buffer_size = count*8;
   std::unique_ptr<GLfloat[]> vertices(new GLfloat[buffer_size]);
   GLfloat *vertices_ptr = vertices.get();
#ifndef NDEBUG
   GLfloat *vertices_ptr_end = &vertices_ptr[buffer_size];
#endif
   const GLfloat flip = (yz_flip) ? -1 : 1;
   double totalx = 0, totaly = 0, totalz = 0;
   double n = 0;
#ifdef PCW_DEBUG_SHADER
   _vertices_.clear();
#endif
   if (! mean_center)
   {
      Xs.resize(verts->count);
      Ys.resize(verts->count);
      Zs.resize(verts->count);
   }
   GLfloat x, y, z, red =1.0f, green =0, blue =0, alpha =1.0f;
   for (size_t i=0; i<count; i++)
   {
      const Real3<float> item = vertdata[i];
      x = item.x * scale;
      y = item.y * scale * flip;
      z = item.z * scale * flip;
      if ( (is_color_pointcloud) && (i < colors->count) )
      {
         if (is_color_pointcloud)
         {
            if (is_alpha_pointcloud)
            {
               red = static_cast<float>(RGBAdata[i].r) / 255.0f;
               green = static_cast<float>(RGBAdata[i].g) / 255.0f;
               blue = static_cast<float>(RGBAdata[i].b) / 255.0f;
               alpha = static_cast<float>(RGBAdata[i].a) / 255.0f;
            }
            else
            {
               red = static_cast<float>(RGBdata[i].r) / 255.0f;
               green = static_cast<float>(RGBdata[i].g) / 255.0f;
               blue = static_cast<float>(RGBdata[i].b) / 255.0f;
               alpha = 1.0f;
            }
         }
      }
      else
      {
         red = alpha = 1.0f;
         green = blue = 0;
      }
      _push_vertex(vertices_ptr, x, y, z, 0, red, green, blue, alpha);
      points.add(item.x, item.y, item.z, &red, &green, &blue, &alpha);
      if (! mean_center)
      {
         Xs[i] = x;
         Ys[i] = y;
         Zs[i] = z;
      }
//      std::cout << std::fixed << std::setprecision(5) << x << ", " << y << ", " << z << std::endl;

      if (x < minx) minx = x;
      if (x > maxx) maxx = x;
      if (y < miny) miny = y;
      if (y > maxy) maxy = y;
      if (z < minz) minz = z;
      if (z > maxz) maxz = z;
      totalx += x; totaly += y; totalz += z;
      n++;
   }
   assert(vertices_ptr == vertices_ptr_end);
   rangex = fabsf(maxx - minx); rangey = fabsf(maxy - miny); rangez = fabsf(maxz - minz);
   max_r = sqrtf(rangex*rangex + rangey*rangey + rangez*rangez);
   if (isnanf(r))
      r = max_r/2.0f;
   phi =PIf/2.0f; theta =0;
   cartesian();

   if (mean_center)
   {
      const auto meanx = static_cast<float>(totalx / n);
      const auto meany = static_cast<float>(totaly / n);
      const auto meanz = static_cast<float>(totalz / n);
      centroid = glm::vec3(meanx, meany, meanz);
   }
   else
   {
      std::nth_element(Xs.begin(), Xs.begin() + Xs.size() / 2, Xs.end());
      std::nth_element(Ys.begin(), Ys.begin() + Ys.size() / 2, Ys.end());
      std::nth_element(Zs.begin(), Zs.begin() + Zs.size() / 2, Zs.end());
      const float medianx = Xs[Xs.size() / 2];
      const float mediany = Ys[Ys.size() / 2];
      const float medianz = Zs[Zs.size() / 2];
      centroid = glm::vec3(medianx, mediany, medianz);
   }
   index.reset(new kd_tree_t(3, points, nanoflann::KDTreeSingleIndexAdaptorParams(10)));
   index->buildIndex();
   return vertices;
}

bool PointCloudWin::init_pointcloud()
//-----------------------------------
{
   if (! pointcloud_unit) return false;
   std::unique_ptr<GLfloat[]> vertices = load_pointcloud();
   if (! vertices) return false;
   pointcloud_unit.del("VAO_VERTICES");
   pointcloud_unit.del("VBO_VERTICES");

   oglutil::clearGLErrors();
   glGenBuffers(1, &pointcloud_unit.GLuint_ref("VBO_VERTICES"));
   glBindBuffer(GL_ARRAY_BUFFER, pointcloud_unit.GLuint_get("VBO_VERTICES"));
   glBufferData(GL_ARRAY_BUFFER, count*8*sizeof(GLfloat), vertices.get(), GL_DYNAMIC_DRAW);
   glBindBuffer(GL_ARRAY_BUFFER, 0);

   glGenVertexArrays(1, &pointcloud_unit.GLuint_ref("VAO_VERTICES"));
   glBindVertexArray(pointcloud_unit.GLuint_get("VAO_VERTICES"));
   glBindBuffer(GL_ARRAY_BUFFER, pointcloud_unit.GLuint_get("VBO_VERTICES"));
   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray (1);
   GLsizei stride = sizeof(GLfloat) * (4 + 4);
   glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, 0);
   glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void *>(4 * sizeof (GLfloat)));
   glBindVertexArray(0);
   std::stringstream errs;
   GLuint err;
   errs << "OpenGL msg loading pointcloud vertices: ";
   if (! oglutil::isGLOk(err, &errs))
   {
      std::cerr << errs.str().c_str() << std::endl;
      pointcloud_unit.del("VAO_VERTICES");
      pointcloud_unit.del("VBO_VERTICES");
      initialised_pc = false;
      return false;
   }
   initialised_pc = true;
   return true;
}

void PointCloudWin::cartesian()
//---------------------------------
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

void PointCloudWin::onCursorUpdate(double xpos, double ypos)
//-----------------------------------------------------------
{
   cursor_pos.first = xpos; cursor_pos.second = ypos;
   if (is_dragging)
   {
      rotation_update(xpos, ypos);
      drag_start = cursor_pos;
   }
}

void PointCloudWin::on_mouse_click(int button, int action, int mods)
//------------------------------------------------------------------
{
   last_button = button; last_button_action = action; last_button_mods = mods;
   if ( (last_button == GLFW_MOUSE_BUTTON_RIGHT) && (last_button_action == GLFW_RELEASE) )
      cast_ray();
   else if (last_button == GLFW_MOUSE_BUTTON_LEFT)
   {
      if (last_button_action == GLFW_PRESS)
      {
         is_dragging = true;
         drag_start = cursor_pos;
         if (rotating_cursor == nullptr)
         {
            rotating_cursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
            glfwSetCursor(GLFW_win(), rotating_cursor);
         }
      }
      else if (last_button_action == GLFW_RELEASE)
      {
         is_dragging = false;
         if (rotating_cursor != nullptr)
         {
            glfwDestroyCursor(rotating_cursor);
            rotating_cursor = nullptr;
            glfwSetInputMode(GLFW_win(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
         }
      }
   }
//         rotation_update(last_xpos, last_ypos);
}

void PointCloudWin::rotation_update(double xpos, double ypos)
//-----------------------------------------------------------
{
//   std::cout << "(" << xpos << ", " << ypos << ")" << std::endl;
   float xdiff = static_cast<float>(xpos - drag_start.first), ydiff = static_cast<float>(ypos - drag_start.second);
   if (fabs(xdiff) >= fabs(ydiff))
      theta = add_angle(theta, sgn(xdiff) * angle_incr, 2 * PIf);
   else
      phi = add_angle(phi, sgn(ydiff)*angle_incr, PIf);
   cartesian();
}

void PointCloudWin::cast_ray()
//----------------------------
{
   float x = (2.0f * static_cast<float>(cursor_pos.first)) / width - 1.0f;
   float y = 1.0f - (2.0f * static_cast<float>(cursor_pos.second)) / height;
   glm::vec4 img_ray(x, y, -1, 1);
   glm::vec4 eye_ray = IP*img_ray;
   eye_ray = glm::vec4(eye_ray.x, eye_ray.y, -1.0, 0.0);
   glm::vec4 world_ray = IMV * eye_ray;
   glm::vec3 ray = glm::normalize(glm::vec3(world_ray.x, world_ray.y, world_ray.z));

//   GLfloat depth;
//   glReadPixels(cursor_pos.first, height - cursor_pos.second - 1, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
//   glm::vec4 viewport = glm::vec4(0, 0, width, height);
//   glm::vec3 wincoord = glm::vec3(cursor_pos.first, height - cursor_pos.second - 1, depth);
//   glm::vec3 objcoord = glm::unProject(wincoord, MV, P, viewport);

   glm::vec3 O = location; //(0, 0, 0);
   float search_radius = 0.25;
   size_t hit = std::numeric_limits<size_t>::max();
   float dist = std::numeric_limits<float>::max();
   for (size_t i=0; i<points.kdtree_get_point_count(); i++)
   {
      if (i == selected_index) continue;
      Real3<float> p = points.get(i);
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
         }
      }
   }
   if (hit < points.kdtree_get_point_count())
   {
      selected.clear();
      selected[hit] = 0;
      selected_index = hit;
      search_radius = 3;
      std::vector<std::pair<size_t, float>> ret_matches;
      nanoflann::SearchParams params;
      Real3<float> p = points.get(hit);
      const float query_pt[3] = { p.x, p.y, p.z};
      const size_t no = index->radiusSearch(&query_pt[0], search_radius, ret_matches, params);
      for (size_t i = 0; i < no; i++)
      {
         selected[ret_matches[i].first] = ret_matches[i].second;
//         std::cout << "idx[" << i << "]=" << ret_matches[i].first << " dist[" << i << "]=" << ret_matches[i].second
//                   << " (" << points.get(i).x << ", " << points.get(i).y << ", " << points.get(i).z << ")" << std::endl;

      }
      glm::vec3 start = O + 0.0f*ray, end = O + dist*ray;
      ray_data[0] = start.x; ray_data[1] = start.y; ray_data[2] = start.z;
      ray_data[3] = 1; ray_data[4] = 1; ray_data[5] = 1;
      ray_data[6] = end.x; ray_data[7] = end.y; ray_data[8] = end.z;
      ray_data[9] = 1; ray_data[10] = 1; ray_data[11] = 1;
      is_selection_change = true;
   }
}

void PointCloudWin::on_match_select_change(float x, float y, float z)
//-------------------------------------------------------------------
{
   const float flip = (yz_flip) ? -1 : 1;
   x *= scale; y *= flip*scale; z *= flip*scale;
   for (auto it=selected.begin(); it != selected.end(); ++it)
   {
      size_t i = it->first;
      Real3<float> p = points.get(i);
//      std::cout << i << ": " <<  x << "," << y << "," << z << " -> " << p.x << "," << p.y << "," << p.z
//                << glm::distance(glm::vec3(x, y, z), glm::vec3(p.x, p.y, p.z)) << std::endl;
      if ( (near_zero(x - p.x, 0.000001f)) && (near_zero(y - p.y, 0.000001f)) && (near_zero(z - p.z, 0.000001f)) )
      {
         selected[selected_index] = 1;
         selected_index = i;
         selected[selected_index] = 0;
         is_selection_change = true;
         break;
      }
   }
}
