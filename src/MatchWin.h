#ifndef _MATCHWIN_H_
#define _MATCHWIN_H_

#include <iostream>

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

#include <opencv2/core/core.hpp>

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
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "OGLFiberWin.hh"
#include "PointCloudWin.h"
//#include "ImageWindow.hh"
#include "OpenGLText.h"
#include "MatchIO.h"
#include "Status.h"
#include "util.h"
#include "types.h"

static const size_t MAX_POINTS = 20;

class PointCloudWin;

class ImageWindow;

class GLFWContext : public OpenGLContext
//======================================
{
public:
   GLFWContext(oglfiber::OGLFiberWindow& win) : window(win) {}

   virtual void request_context() { window.request_context(); }

   virtual void release_context() { window.release_context(); }

private:
   oglfiber::OGLFiberWindow& window;
};

class ImageWindow;

class MatchWin : public oglfiber::OGLFiberWindow
//===============================================
{
public:
   MatchWin(const std::string &title, int w, int h, const std::string& shader_dir, bool isBestFeatureOnly = false,
            float featureRadius =5.0f, int glsl_ver =440, int gl_major =4, int gl_minor = 4, bool can_resize =true,
            GLFWmonitor *mon =nullptr);

   void set_image_view(ImageWindow* imageWindow) { image_view = imageWindow; }

   void clear_points(float flipyz_) { points.clear(); flip_yz = flipyz_; }
   void add_point(Real3<float> &pt, float distance, std::tuple<float, float, float, float>* color = nullptr)
   {
      points.emplace_back(pt, distance);
      if (color != nullptr)
         colors.push_back(*color);
   }
   void update_points(PointCloudWin* pointSource);
   void update_image(cv::Mat img, cv::Rect& rect, std::vector<features_t>* features, bool isBGR =true)
   {
      image = img; image_rect = rect;  is_BGR = isBGR;
      match_features = features;
      is_image_update.store(true);
   }

#ifdef HAVE_SOIL2
   void update_image(const char* imgfile, cv::Rect& rect);
   void update_image(void* imgdata, int width, int height, int channels, cv::Rect& rect);
#endif

   static float angle_incr;
   static constexpr float margin = 8.0f;
   static float max_phi;
   static constexpr double PI = 3.14159265358979323846264338327;
   static constexpr float PIf = 3.14159265358979f;

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
      if (r < 0) r = 0;
      if (r > max_r*1.5f) r = max_r*1.5f;
      cartesian();
   }
   void on_key_press(oglfiber::KeyPress& keyPress) override;

private:
   int glsl_ver;
   filesystem::path shader_directory;
   bool is_best_feature_only = false;
   float feature_radius =2.0;
   ImageWindow *image_view = nullptr;
   int window_width =-1, window_height =-1, cloud_start=0, image_width =0, image_height =0;
   GLfloat image_depth = 0;
   float flip_yz =1.0f;
   oglutil::OGLProgramUnit image_unit, pointcloud_unit;
   GLuint image_texture = 0;
   std::vector<std::pair<Real3<float>, float>> points;
   std::vector<Real3<float>> display_points;
   std::vector<std::tuple<float, float, float, float>> colors;
   size_t cloud_count = 0, selected_index =std::numeric_limits<size_t>::max(),
          last_selected_index =std::numeric_limits<size_t>::max();
   bool is_selection_change = false;
   std::unique_ptr<GLfloat[]> gl_vertices;
   std::pair<double, double> cursor_pos, drag_start_cloud, drag_start_image;
   GLFWcursor* rotating_cursor = nullptr;
   int last_button =0, last_button_action =0, last_button_mods =0;
   bool initialised_pc = false, updated_pc = false, in_cloud = false, in_image = false, in_control = false,
        is_dragging = false, is_dragging_cloud = false, is_dragging_image = false;
   float minx = std::numeric_limits<float>::max(), maxx = std::numeric_limits<float>::lowest();
   float miny = std::numeric_limits<float>::max(), maxy = std::numeric_limits<float>::lowest();
   float minz = std::numeric_limits<float>::max(), maxz = std::numeric_limits<float>::lowest();
   float ax, ay, az, bx, by, bz;
   float max_r = 0, r = std::numeric_limits<float>::quiet_NaN(), phi =PIf/2.0f, theta =0;
   glm::vec3 location{0, 0, 0}, centroid{0, 0, 0}, tangent{0, 1, 0};
   std::string centroid_text;
   glm::mat4 P, IP, MV, IMV;
   GLfloat pointSize =20.0f; // gl_pointSize equivalent uniform in shader
   OpenGLText caption_writer;
   GLFWContext gl_context;
   PointCloudWin* point_source;
   cv::Mat image;
   cv::Rect drag_image_rect;
   cv::Rect image_rect;
   int image_channels = 0;
   bool is_BGR = false;
   std::vector<features_t>* match_features;
   std::shared_ptr<std::vector<features_t*>> selected_features;
   std::atomic_bool is_image_update{false};
   std::vector<matched_t> matched_features;
   std::string matched_filename;
   Status status_info;
   int status_height = 50;

   bool init_shader(const filesystem::path& dir, oglutil::OGLProgramUnit& unit);
   bool init_pointcloud_buffer();
   void cartesian();
   bool setup_image_render();
   bool setup_image_texture();
   void cloud_rotation_update(double xpos, double ypos);
   void render_image();

   void choose_3dpt();

   void choose_kp(double mousex, double mousey, int mods);

   void image_drag_end(double mouseX, double mousey, int mods);
};

typedef struct
{
   float x, y, z;    // position
   float s, t;       // texture
   float r, g, b, a; // color
} vertex_t;

inline bool load_font(OpenGLText& writer, const std::string& fontid, const char* font, int size, bool isRGB =false,
                      const std::string& preload_chars ="")
{
   if (! writer.add_font(fontid, font, size, isRGB, preload_chars))
   {
      std::cerr << "MatchWin::MatchWin: Error loading font " << font << " size " << size << " name " << fontid
                << std::endl;
      return false;
   }
   return true;
}
#endif //TRAINER3D_MATCHWIN_H
