#include <iostream>
#include <iomanip>
#include <sstream>
#include <regex>
#include <thread>
#include <optional>
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

#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QtWidgets>
#include <QInputDialog>
#include <QTimer>

#include <opencv2/imgcodecs.hpp>

#include "ImageWindow.hh"
#include "OGLFiberWin.hh"
#include "PointCloudWin.h"
#include "MatchWin.h"
#include "SpinLock.h"

const int OPENGL_MAJOR = 4;
const int OPENGL_MINOR = 5;
const int GLSL_VER = 450;

PointCloudWin* pointcloud = nullptr;
MatchWin* matcher = nullptr;

void chessboard_mat(int blockSize, cv::Mat& chessBoard)
//----------------------------------------
{
   int imageSize=blockSize*8;
   chessBoard = cv::Mat::zeros(imageSize, imageSize, CV_8UC3); // cv::Mat(imageSize,imageSize,CV_8UC3, cv::Scalar::all(0));
   unsigned char color=0;

   for(int i=0;i<imageSize;i=i+blockSize)
   {
      color=~color;
      for(int j=0;j<imageSize;j=j+blockSize)
      {
         cv::Mat ROI=chessBoard(cv::Rect(i,j,blockSize,blockSize));
         ROI.setTo(cv::Scalar::all(color));
         color=~color;
      }
   }
}

int main(int argc, char *argv[])
//-----------------------------
{
   QApplication a(argc, argv);
   QApplication::setApplicationName("PnPTrainer");
   QApplication::setApplicationVersion("0.1");
   QCommandLineParser parser;
   parser.setApplicationDescription("PnPTrainer");
   parser.addHelpOption();
   parser.addVersionOption();
   parser.addPositionalArgument("image", "Image file (png, jpg)");
   parser.addPositionalArgument("threed", "3D pointcloud file (ply)");
   parser.addOption(QCommandLineOption("s", "Shader directory", "shaders", "shaders/"));
   parser.addOption(QCommandLineOption("S", "Scaling factor in point cloud view", "scale", "1"));
   parser.addOption(QCommandLineOption("P", "Point size in point cloud view", "point-size", "-1"));
   parser.addOption({"f", "Flip Y and Z axis for point cloud data."});
   parser.addOption(QCommandLineOption("r", "Click radius for 2D features", "click-radius", "5"));
   parser.addOption({"b", "Choose only best (by response) 2D feature if multiple features are in click radius."});
   parser.process(a);
   std::string shaders_dir = parser.value("s").toStdString();
   filesystem::path shaders_path = filesystem::canonical(filesystem::path(shaders_dir.c_str()));
   if (! filesystem::is_directory(shaders_path))
   {
      std::cerr << "Invalid shader directory " << shaders_dir << std::endl;
      return 1;
   }
   filesystem::path match_shaders_path = shaders_path / filesystem::path("match");
   if (! filesystem::is_directory(match_shaders_path))
   {
      std::cerr << "Invalid matcher window shader directory " << match_shaders_path.string() << std::endl;
      return 1;
   }
   std::string scales = parser.value("S").toStdString();
   std::string points = parser.value("P").toStdString();
   bool is_flipped = parser.isSet("f");
   float scale = 1, point_size;
   char *endptr;
   scale = strtof(scales.c_str(), nullptr);
   if (scale <= 0)
   {
      std::cerr << "Invalid scale (-S " << scales << ")" << std::endl;
      return 1;
   }
   point_size = strtof(points.c_str(), nullptr);
   if (point_size == 0)
   {
      std::cerr << "Invalid point size (-P " << points << ")" << std::endl;
      return 1;
   }
   std::string s = parser.value("r").toStdString();
   float cradius = strtof(s.c_str(), nullptr);
   if (cradius < 1)
   {
      std::cerr << "Invalid 2d feature click radius (" << s << ")" << std::endl;
      return 1;
   }
   bool is_best_response = parser.isSet("b");
   const QStringList args = parser.positionalArguments();
   std::string plyfile, imgfile;

   if (args.length() < 2)
   {
      std::cerr << "Requires image file and pointcloud file on command line" << std::endl;
      std::exit(1);
   }
   else
   {
      std::string img_pattern = R"(.*\.jpg$|.*\.jpeg$|.*\.png$)";
      std::regex img_regex(img_pattern, std::regex_constants::icase);
      imgfile = args.at(0).toStdString();
      filesystem::path p = filesystem::canonical(filesystem::path(imgfile.c_str()));
      if ( (! filesystem::is_regular_file(p)) || (! std::regex_match(imgfile, img_regex)) )
      {
         std::cerr << "Image file " << p.string() << " invalid file or type" << std::endl;
         return 1;
      }
      plyfile = args.at(1).toStdString();
      p = filesystem::canonical(filesystem::path(plyfile.c_str()));
      std::string ply_pattern = ".*\\.ply$";
      std::regex ply_regex(ply_pattern, std::regex_constants::icase);
      if ( (! filesystem::is_regular_file(p)) || (! std::regex_match(plyfile, ply_regex)) )
      {
         std::cerr << "3D file " << p.string() << " invalid file or type" << std::endl;
         return 1;
      }
   }

   oglfiber::OGLFiberExecutor& gl_executor = oglfiber::OGLFiberExecutor::instance();
   matcher = new MatchWin("Match", 1024, 768, match_shaders_path.string(), is_best_response, cradius,
                          GLSL_VER,  OPENGL_MAJOR, OPENGL_MINOR);
   if (! matcher->good())
   {
      std::cerr << "Matcher window creation msg" << std::endl;
      return 1;
   }
   cv::Mat chessboard;
   chessboard_mat(27, chessboard);
   pointcloud = new PointCloudWin("PointCloud", 1024, 768, "shaders/pointcloud/",plyfile, matcher,
                                  scale, is_flipped, false, GLSL_VER,  OPENGL_MAJOR, OPENGL_MINOR);
   if (point_size > 0)
      pointcloud->set_point_size(point_size);
   cv::Rect R(0, 0, chessboard.cols, chessboard.rows);
   matcher->update_image(chessboard, R, nullptr);
   gl_executor.start({pointcloud, matcher}, true);
   ImageWindow imgwin(matcher);
   imgwin.setApplication(&a);
   matcher->set_image_view(&imgwin);
   imgwin.show();
   if (! imgfile.empty())
      imgwin.load(imgfile);
   int ret = a.exec();
   glfwSetWindowShouldClose(pointcloud->GLFW_win(), GLFW_TRUE);
   glfwSetWindowShouldClose(matcher->GLFW_win(), GLFW_TRUE);
   gl_executor.join();
//   glfwTerminate();
   std::cout << "Terminate " << ret << std::endl;
}

void msg(const char *psz)
//-------------------------------
{
   QMessageBox msgBox;
   msgBox.setText(QString(psz));
   msgBox.exec();
}

void msg(const std::stringstream &errs)
//-------------------------------
{
   QMessageBox msgBox;
   msgBox.setText(QString(errs.str().c_str()));
   msgBox.exec();
}

bool yesno(QWidget* parent, const char *title, const char *msg)
//-------------------------------------------
{
   QMessageBox::StandardButton reply;
   reply = QMessageBox::question(parent, title, msg, QMessageBox::Yes|QMessageBox::No);
   return (reply == QMessageBox::Yes);
}

void stop()
{
   if (pointcloud != nullptr)
      glfwSetWindowShouldClose(pointcloud->GLFW_win(), GLFW_TRUE);
   if (matcher != nullptr)
      glfwSetWindowShouldClose(matcher->GLFW_win(), GLFW_TRUE);
   oglfiber::OGLFiberExecutor& gl_executor = oglfiber::OGLFiberExecutor::instance();
   gl_executor.join();
}

//bool open_dir(const std::string& dir, std::string& imgfile, std::string& plyfile)
////-------------------------------------------------------------------------------
//{
//   imgfile = plyfile = "";
//   filesystem::path p(dir);
//   if (! filesystem::is_directory(p))
//   {
//      std::cerr << filesystem::canonical(p) << " is not a directory" << std::endl;
//      return false;
//   }
//   std::string ply_pattern = ".*\\.ply$";
//   std::string img_pattern = R"(.*\.jpg$|.*\.jpeg$|.*\.png$)";
//   std::regex ply_regex(ply_pattern, std::regex_constants::icase), img_regex(img_pattern, std::regex_constants::icase);
//   for (const auto& entry : filesystem::directory_iterator(p))
//   {
//      std::string filename = entry.path().filename().string();
//      if (std::regex_match(filename, ply_regex))
//         plyfile = entry.path().string();
//      else if (std::regex_match(filename, img_regex))
//         imgfile = entry.path().string();
//   }
//   return ( (! imgfile.empty()) && (! plyfile.empty()) );
//}
