#ifndef _TYPES_H_
#define _TYPES_H_

#include <memory>
#include <utility>
#include <chrono>

#include <opencv2/core/core.hpp>

template <typename T>
struct Real3
//===================
{
   T x, y, z;
   Real3(T x_, T y_, T z_) : x(x_), y(y_), z(z_) {}
   Real3(const Real3<T>& other) : x(other.x), y(other.y), z(other.z) {}
};

struct features_t
{
   size_t index;
   cv::KeyPoint keypoint;
   cv::Mat descriptor;
   bool is_selected = false;

   features_t(size_t index, const cv::KeyPoint& keypoint, const cv::Mat& descriptor, bool is_selected) :
      index(index), keypoint(keypoint), descriptor(descriptor), is_selected(is_selected) {}
};

struct matched_t
{
   Real3<float> point_3d;
   std::shared_ptr<std::vector<features_t*>> features_2d;

   matched_t(const Real3<float>&pt3d, std::shared_ptr<std::vector<features_t*>>& features)
   : point_3d(pt3d), features_2d(features) {}
};

struct DetectorInfo
{
   std::string name;
   std::unordered_map<std::string, std::string> parameters;

   void set(std::string name_, std::initializer_list<std::initializer_list<std::string>> l)
   {
      name = name_;
      parameters.clear();
      for (auto it=l.begin(); it !=l.end(); ++it)
      {
         std::initializer_list<std::string> ll = *it;
         if (ll.size() == 2)
         {
            auto it2=ll.begin();
            std::string k = *it2++;
            std::string v = *it2++;
            parameters[k] = v;
         }
      }
   }
};

using TimeType = std::chrono::_V2::system_clock::time_point;

#endif