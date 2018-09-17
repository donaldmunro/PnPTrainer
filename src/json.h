#ifndef _JSON_H_
#define _JSON_H_

#include <opencv2/core.hpp>

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/istreamwrapper.h>

namespace jsoncv
{
   rapidjson::Value encode_ocv_keypoint(const cv::KeyPoint &k, rapidjson::Document::AllocatorType &allocator);

   bool decode_ocv_keypoint(const rapidjson::Document &document, const char *name, cv::KeyPoint &k);

   cv::KeyPoint decode_ocv_keypoint(rapidjson::Value &o);

   rapidjson::Value encode_ocv_mat(const cv::Mat &m, rapidjson::Document::AllocatorType &allocator);

   bool decode_ocv_mat(const rapidjson::Document &document, const char *name, cv::Mat &m);

   bool decode_ocv_rect(const rapidjson::Document &document, const char *name, cv::Rect2d &r);

   cv::Rect2d decode_ocv_rect(rapidjson::Value &o);

   bool decode_ocv_rrect(const rapidjson::Document &document, const char *name, cv::RotatedRect &r);

   cv::RotatedRect decode_ocv_rrect(rapidjson::Value &o);

   template<typename T>
   rapidjson::Value encode_vector(const std::vector<T>& V, rapidjson::Document::AllocatorType& allocator,
                                  const std::function<rapidjson::Value(const T& v,
                                                      rapidjson::Document::AllocatorType& allocator)>& convertor)
//--------------------------------------------------------------------------------------------------------
   {
      rapidjson::Value a(rapidjson::kArrayType);
      for (T item : V)
      {
         rapidjson::Value vv = convertor(item, allocator);
         a.PushBack(vv, allocator);
      }
      return a;
   }
};

#endif //_JSON_H_
