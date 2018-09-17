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

#include <opencv2/imgproc.hpp>

std::string replace_ver(const char *pch, int ver)
//----------------------------------------------------------------
{
   if (pch == nullptr)
      return "";
   std::string s(pch);
   std::regex r(R"(\{\{ver\}\})");
   std::stringstream ss;
   ss << ver;
   return std::regex_replace (s, r, ss.str());
}

void create_icosahedron(GLuint& positions, GLuint& indices)
//----------------------------------------------------------
{
   const int Faces[] =
   {
         2, 1, 0,
         3, 2, 0,
         4, 3, 0,
         5, 4, 0,
         1, 5, 0,
         11, 6,  7,
         11, 7,  8,
         11, 8,  9,
         11, 9,  10,
         11, 10, 6,
         1, 2, 6,
         2, 3, 7,
         3, 4, 8,
         4, 5, 9,
         5, 1, 10,
         2,  7, 6,
         3,  8, 7,
         4,  9, 8,
         5, 10, 9,
         1, 6, 10
   };

   const float Verts[] =
   {
         0.000f,  0.000f,  1.000f,
         0.894f,  0.000f,  0.447f,
         0.276f,  0.851f,  0.447f,
         -0.724f,  0.526f,  0.447f,
         -0.724f, -0.526f,  0.447f,
         0.276f, -0.851f,  0.447f,
         0.724f,  0.526f, -0.447f,
         -0.276f,  0.851f, -0.447f,
         -0.894f,  0.000f, -0.447f,
         -0.276f, -0.851f, -0.447f,
         0.724f, -0.526f, -0.447f,
         0.000f,  0.000f, -1.000f
   };

   size_t IndexCount = sizeof(Faces) / sizeof(Faces[0]);

   // Create the VBO for positions:
   GLsizei stride = 3 * sizeof(float);
   glGenBuffers(1, &positions);
   glBindBuffer(GL_ARRAY_BUFFER, positions);
   glBufferData(GL_ARRAY_BUFFER, sizeof(Verts), Verts, GL_STATIC_DRAW);
   glBindBuffer(GL_ARRAY_BUFFER, 0);

   glGenBuffers(1, &indices);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices);
   glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(Faces), Faces, GL_STATIC_DRAW);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

bool checkerboard_texture(GLuint& textureName)
//---------------------------------------------
{
   if (textureName == 0)
      glGenTextures(1, &textureName);
   glBindTexture(GL_TEXTURE_2D, textureName);
   GLubyte pixels[4 * 3] =
         {
               255, 0, 0, // Red
               0, 255, 0, // Green
               0, 0, 255, // Blue
               255, 255, 0  // Yellow
         };

   // Use tightly packed data
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

   // Load the texture
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);

   // Set the filtering mode
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

   glBindTexture(GL_TEXTURE_2D, 0);
   return (glIsTexture(textureName));
}

std::string cvtype(cv::InputArray a)
//--------------------------------
{
   int numImgTypes = 35; // 7 base types, with five channel options each (none or C1, ..., C4)

   int enum_ints[] =       {CV_8U,  CV_8UC1,  CV_8UC2,  CV_8UC3,  CV_8UC4,
                            CV_8S,  CV_8SC1,  CV_8SC2,  CV_8SC3,  CV_8SC4,
                            CV_16U, CV_16UC1, CV_16UC2, CV_16UC3, CV_16UC4,
                            CV_16S, CV_16SC1, CV_16SC2, CV_16SC3, CV_16SC4,
                            CV_32S, CV_32SC1, CV_32SC2, CV_32SC3, CV_32SC4,
                            CV_32F, CV_32FC1, CV_32FC2, CV_32FC3, CV_32FC4,
                            CV_64F, CV_64FC1, CV_64FC2, CV_64FC3, CV_64FC4};

   std::string enum_strings[] = {"CV_8U",  "CV_8UC1",  "CV_8UC2",  "CV_8UC3",  "CV_8UC4",
                                 "CV_8S",  "CV_8SC1",  "CV_8SC2",  "CV_8SC3",  "CV_8SC4",
                                 "CV_16U", "CV_16UC1", "CV_16UC2", "CV_16UC3", "CV_16UC4",
                                 "CV_16S", "CV_16SC1", "CV_16SC2", "CV_16SC3", "CV_16SC4",
                                 "CV_32S", "CV_32SC1", "CV_32SC2", "CV_32SC3", "CV_32SC4",
                                 "CV_32F", "CV_32FC1", "CV_32FC2", "CV_32FC3", "CV_32FC4",
                                 "CV_64F", "CV_64FC1", "CV_64FC2", "CV_64FC3", "CV_64FC4"};
   int typ = a.type();
   for(int i=0; i<numImgTypes; i++)
      if (typ == enum_ints[i]) return enum_strings[i];
   return "unknown image type";
}

void plot_circles(const cv::Mat& img, double x, double y, int startRadius, int radiusInc,const std::vector<cv::Scalar>& colors)
//----------------------------------------------------------------------------------------------------------------
{
   for (cv::Scalar color : colors)
   {
      cv::circle(img, cv::Point2i(cvRound(x), cvRound(y)), startRadius, color, 1);
      startRadius += radiusInc;
   }
}

void plot_rectangles(cv::Mat& img, double x, double y, int width, int height, int increment,
                     const std::vector<cv::Scalar>& colors)
//----------------------------------------------------------------------------------------------------------------
{
   for (cv::Scalar color : colors)
   {
      cv::Point2d pt1(x, y);
      cv::Point2d pt2(x + width, y + height);
      cv::rectangle(img, pt1, pt2, color);
      width += increment; height += increment;
   }
}
