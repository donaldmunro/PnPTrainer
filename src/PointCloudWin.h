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
/*
 * Displayed in a spherical coordinate system. Interaction via
 * mouse drag to rotate (phi, theta) and mouse wheel to adjust r (zoom).
 * Location eye point is obtained by converting from spherical to cartesian
 * coordinates. Up direction is taken as the projection of the Y axis onto
 * the tangent plane of the point (see tangent.tex/tangent.pdf).
 */
#ifndef FIBERGL_POINTCLOUDWIN_H
#define FIBERGL_POINTCLOUDWIN_H

#include <iostream>

#include "OGLFiberWin.hh"
#include "MatchWin.h"
#include "nanoflann.hpp"
#include "util.h"
#include "types.h"

class MatchWin;

template <typename T>
struct PointCloudFlannSource
//==========================
{
   PointCloudFlannSource(float scale_ =1.0, bool yz_flip_ =false) : scale(scale_), flip((yz_flip_) ? -1 : 1) {}

   float scale, flip;
   std::vector<Real3<T>> pts;
   std::vector<std::tuple<T, T, T, T>> colors;

   void clear() { pts.clear(); }
   void add(T x, T y, T z, T* r = nullptr, T* g = nullptr, T* b = nullptr, T* a= nullptr)
   {
      pts.emplace_back(x, y, z);
      bool iscolor = (r != nullptr && g != nullptr && b != nullptr);
      if (iscolor)
      {
         if (a == nullptr)
            colors.push_back(std::make_tuple(*r, *g, *b, 1.0));
         else
            colors.push_back(std::make_tuple(*r, *g, *b, *a));
      }
   }

   Real3<T> get(size_t i) { return Real3<T>(pts[i].x * scale, pts[i].y * scale * flip, pts[i].z * scale * flip); }

   bool is_selected = false;

   inline size_t kdtree_get_point_count() const { return pts.size(); }

   inline T kdtree_get_pt(const size_t i, int dim) const
   //-----------------------------------------------------
   {
      if (dim == 0) return pts[i].x * scale;
      else if (dim == 1) return pts[i].y * scale * flip;
      else return pts[i].z * scale * flip;
   }

   template <class BBOX>
   bool kdtree_get_bbox(BBOX& /* bb */) const { return false; }
};

typedef nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Simple_Adaptor<float, PointCloudFlannSource<float>>,
                                            PointCloudFlannSource<float>, 3> kd_tree_t;

class PointCloudWin : public oglfiber::OGLFiberWindow
//=====================================================
{
public:
/**
 *
 * @param title - Window title
 * @param w - Initial window width
 * @param h - - Initial window heigh
 * @param shader_dir - Base directory for shaders (assumed to contain sub-directories cloud and axes and cloud
 * @param plyfilename - Path to .ply file for points
 * @param scale_ -  Scale points factor to increase spacing
 * @param yz_flip - Some point cloud generators eg Google Tango use a Y positive down, Z forward coord system.
 * In this case specify true to flip to OpenGL coord system.
 * @param is_mean_center_ - True to use point cloud mean as center, false to use median (the spherical coordinate system
 * is defined to always look towards the centre). See also set_center. Allowing the user to drag the center is left
 * as an exercise for the reader.
 * @param glsl_ver - The GLSL version to replace with in shader file (default 440)
 * @param gl_major - OpenGL Major (4)
 * @param gl_minor - OpenGL Minor (4)
 * @param can_resize - Can window be resized
 */
   PointCloudWin(std::string title, int w, int h, const std::string& shader_dir,
                 const std::string& plyfilename, MatchWin* matchWin,
                 float scale_ =1.0f, bool yz_flip_ =false, bool is_mean_center_ = true,
                 int glsl_ver =440, int gl_major =4, int gl_minor = 4, bool can_resize =true);

   void set_center(GLfloat x, GLfloat y, GLfloat z, GLfloat scale =1.0f) { centroid = glm::vec3(x*scale, y*scale, z*scale); }
   void set_r(float _r) { r = _r; cartesian(); }
   void set_point_size(GLfloat psize) { pointSize = psize; }

protected:
   void on_initialize(const GLFWwindow*) override;
   void on_resized(int w, int h) override;
   void on_exit() override {};
   bool on_render() override;
   void onCursorUpdate(double xpos, double ypos) override;
   void on_mouse_click(int button, int action, int mods) override;
   void on_mouse_scroll(double x, double y) override
   {
      r += sgn(y)*0.2f;
      if (r < max_r/6.0f) r = max_r/6.0f;
      if (r > max_r*1.5f) r = max_r*1.5f;
      cartesian();
   }

private:
   MatchWin* match_window = nullptr;
   int glsl_ver;
   int width =0, height =0;
   bool is_dragging = false, yz_flip = false, mean_center =true;
   float scale = 1.0;
   PointCloudFlannSource<float> points;
   std::pair<double, double> cursor_pos, drag_start;
   filesystem::path shader_directory;
   oglutil::OGLProgramUnit axes_unit, pointcloud_unit;
   bool initialised_axes = false, initialised_pc = false;
   size_t count = 0;
   std::unordered_map<size_t, float> selected;
   bool is_selection_change = false;
   size_t selected_index = std::numeric_limits<size_t>::max();
   GLfloat ray_data[12];
   filesystem::path plyfile;
   float minx = std::numeric_limits<float>::max(), maxx = std::numeric_limits<float>::lowest(),
         miny = std::numeric_limits<float>::max(), maxy = std::numeric_limits<float>::lowest(),
         minz = std::numeric_limits<float>::max(), maxz = std::numeric_limits<float>::lowest(),
         rangex =0, rangey =0, rangez =0;
   bool is_color_pointcloud =false, is_alpha_pointcloud = false;
   float max_r = 0, r = std::numeric_limits<float>::quiet_NaN(), phi =PIf/2.0f, theta =0;
   glm::vec3 location{0, 0, 0}, centroid{0, 0, 0}, tangent{0, 1, 0};
   glm::mat4 P, IP, MV, IMV;
   GLfloat pointSize =1.0f; // gl_pointSize equivalent uniform in shader
   GLFWcursor* rotating_cursor = nullptr;
   int last_button =0, last_button_action =0, last_button_mods =0;
   std::unique_ptr<kd_tree_t> index;

   bool init_pointcloud();
   bool init_axes();
   std::unique_ptr<GLfloat[]> load_pointcloud();
   void rotation_update(double xpos, double ypos);

   static float angle_incr;
   static constexpr float margin = 8.0f;
   static float max_phi;
   static constexpr double PI = 3.14159265358979323846264338327;
   static constexpr float PIf = 3.14159265358979f;

   void cartesian();

   void cast_ray();

   void on_match_select_change(float x, float y, float z);

   friend class MatchWin;
};
#endif //FIBERGL_POINTCLOUDWIN_H
