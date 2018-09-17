#ifndef _UTIL_H
#define _UTIL_H

#include <string>
#include <chrono>
#include <time.h>
#include <sstream>
#include <iomanip>

#include <opencv2/core/core.hpp>

template<typename T> inline bool near_zero(T v, T epsilon);
template<> inline bool near_zero(long double v, long double epsilon) { return (fabsl(v) <= epsilon); }
template<> inline bool near_zero(double v, double epsilon) { return (fabs(v) <= epsilon); }
template<> inline bool near_zero(float v, float epsilon) { return (fabsf(v) <= epsilon); }
template <typename T> inline int sgn(T v) { return (v > T(0)) - (v < T(0)); }
inline float add_angle(float angle, float incr, float max)
{
   angle = fmodf(angle + incr, max);
   if (angle < 0) angle += max;
   return angle;
}

std::string replace_ver(const char *pch, int ver);

bool checkerboard_texture(GLuint& image_texture);

template<typename Clock>
std::string datetime(const std::chrono::time_point<Clock>& when)
//-------------------------------------------------------
{
   const time_t timet = std::chrono::system_clock::to_time_t(when);
   std::stringstream ss;
#ifndef HAVE_LOCALTIME_R
   ss << std::put_time(localtime(&timet), "%Y-%m-%d %X");
#else
   struct tm result;
   ss << std::put_time(localtime_r(&timet, &result), "%Y-%m-%d %X");
#endif
   return ss.str();
}

std::string cvtype(cv::InputArray a);

void plot_circles(const cv::Mat& img, double x, double y, int startRadius =2, int radiusInc =1,
                  const std::vector<cv::Scalar>& colors  ={ cv::Scalar(0, 0, 0), cv::Scalar(255, 255, 255),
                                                            cv::Scalar(255, 0, 0),cv::Scalar(0, 255, 255),
                                                            cv::Scalar(0, 0, 255)});
void plot_rectangles(cv::Mat& img, double x, double y, int width, int height, int increment =1,
                     const std::vector<cv::Scalar>& colors  ={ cv::Scalar(0, 0, 0), cv::Scalar(255, 255, 255),
                                                               cv::Scalar(255, 0, 0),cv::Scalar(0, 255, 255),
                                                               cv::Scalar(0, 0, 255)});

#endif
