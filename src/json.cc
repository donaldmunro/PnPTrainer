#include <vector>
#include <functional>

#include <opencv2/core.hpp>

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

namespace jsoncv
{
   inline std::string trim(std::string &str, std::string chars)
//-------------------------------------
   {
      if (str.length() == 0)
         return str;
      unsigned long b = str.find_first_not_of(chars);
      unsigned long e = str.find_last_not_of(chars);
      if (b == std::string::npos) return "";
      str = std::string(str, b, e - b + 1);
      return str;
   }

   inline std::string type(cv::InputArray a)
//----------------------------------------
   {
      int numImgTypes = 35; // 7 base types, with five channel options each (none or C1, ..., C4)

      int enum_ints[] = {CV_8U, CV_8UC1, CV_8UC2, CV_8UC3, CV_8UC4,
                         CV_8S, CV_8SC1, CV_8SC2, CV_8SC3, CV_8SC4,
                         CV_16U, CV_16UC1, CV_16UC2, CV_16UC3, CV_16UC4,
                         CV_16S, CV_16SC1, CV_16SC2, CV_16SC3, CV_16SC4,
                         CV_32S, CV_32SC1, CV_32SC2, CV_32SC3, CV_32SC4,
                         CV_32F, CV_32FC1, CV_32FC2, CV_32FC3, CV_32FC4,
                         CV_64F, CV_64FC1, CV_64FC2, CV_64FC3, CV_64FC4};

      std::string enum_strings[] = {"CV_8U", "CV_8UC1", "CV_8UC2", "CV_8UC3", "CV_8UC4",
                                    "CV_8S", "CV_8SC1", "CV_8SC2", "CV_8SC3", "CV_8SC4",
                                    "CV_16U", "CV_16UC1", "CV_16UC2", "CV_16UC3", "CV_16UC4",
                                    "CV_16S", "CV_16SC1", "CV_16SC2", "CV_16SC3", "CV_16SC4",
                                    "CV_32S", "CV_32SC1", "CV_32SC2", "CV_32SC3", "CV_32SC4",
                                    "CV_32F", "CV_32FC1", "CV_32FC2", "CV_32FC3", "CV_32FC4",
                                    "CV_64F", "CV_64FC1", "CV_64FC2", "CV_64FC3", "CV_64FC4"};
      int typ = a.type();
      for (int i = 0; i < numImgTypes; i++)
         if (typ == enum_ints[i]) return enum_strings[i];
      return "unknown image type";
   }

   rapidjson::Value encode_ocv_keypoint(const cv::KeyPoint& k, rapidjson::Document::AllocatorType& allocator)
//-----------------------------------------------------------------------------------------------------------
   {
      rapidjson::Value v(rapidjson::kObjectType);
      v.AddMember("x", rapidjson::Value().SetFloat(k.pt.x), allocator);
      v.AddMember("y", rapidjson::Value().SetFloat(k.pt.y), allocator);
      v.AddMember("size", rapidjson::Value().SetFloat(k.size), allocator);
      v.AddMember("angle", rapidjson::Value().SetFloat(k.angle), allocator);
      v.AddMember("response", rapidjson::Value().SetFloat(k.response), allocator);
      v.AddMember("octave", rapidjson::Value().SetInt(k.octave), allocator);
      v.AddMember("class_id", rapidjson::Value().SetInt(k.class_id), allocator);
      return v;
   }

   bool decode_ocv_keypoint(const rapidjson::Document &document, const char *name, cv::KeyPoint &k)
//------------------------------------------------------------------------------------------
   {
      rapidjson::Value::ConstMemberIterator exists = document.FindMember(name);
      if (exists != document.MemberEnd())
      {
         auto o = exists->value.GetObject();
         k.pt = cv::Point2f(o.FindMember("x")->value.GetFloat(), o.FindMember("y")->value.GetFloat());
         k.size = o.FindMember("size")->value.GetFloat();
         k.angle = o.FindMember("angle")->value.GetFloat();
         k.response = o.FindMember("response")->value.GetFloat();
         k.octave = o.FindMember("octave")->value.GetInt();
         k.class_id = o.FindMember("class_id")->value.GetInt();
         return true;
      }
      return false;
   }


   cv::KeyPoint decode_ocv_keypoint(rapidjson::Value &o)
//-----------------------------------------------
   {
      cv::KeyPoint k;
      k.pt = cv::Point2f(o.FindMember("x")->value.GetFloat(), o.FindMember("y")->value.GetFloat());
      k.size = o.FindMember("size")->value.GetFloat();
      k.angle = o.FindMember("angle")->value.GetFloat();
      k.response = o.FindMember("response")->value.GetFloat();
      k.octave = o.FindMember("octave")->value.GetInt();
      k.class_id = o.FindMember("class_id")->value.GetInt();
      return k;
   }

   rapidjson::Value encode_ocv_mat(const cv::Mat &m, rapidjson::Document::AllocatorType &allocator)
//-----------------------------------------------------------------------------------------------------------
   {
      rapidjson::Value v(rapidjson::kObjectType);
      v.AddMember("status", rapidjson::Value().SetBool(true), allocator);
      v.AddMember("type", rapidjson::Value().SetInt(m.type()), allocator);
      std::string typname = type(m);
      v.AddMember("typename", rapidjson::Value().SetString(typname, allocator), allocator);
      v.AddMember("rows", rapidjson::Value().SetInt(m.rows), allocator);
      v.AddMember("cols", rapidjson::Value().SetInt(m.cols), allocator);
      rapidjson::Value a(rapidjson::kArrayType);
      switch (m.type())
      {
         case CV_32FC1:
            for (int row = 0; row < m.rows; row++)
            {
               const float *ptr = m.ptr<float>(row);
               for (int col = 0; col < m.cols; col++)
                  a.PushBack(rapidjson::Value().SetFloat(ptr[col]), allocator);
            }
            break;
         case CV_64FC1:
            for (int row = 0; row < m.rows; row++)
            {
               const double *ptr = m.ptr<double>(row);
               for (int col = 0; col < m.cols; col++)
                  a.PushBack(rapidjson::Value().SetDouble(ptr[col]), allocator);
            }
            break;
         case CV_8U:
            for (int row = 0; row < m.rows; row++)
            {
               const uchar *ptr = m.ptr<uchar>(row);
               for (int col = 0; col < m.cols; col++)
                  a.PushBack(rapidjson::Value().SetUint(ptr[col]), allocator);
            }
            break;
         default:
            v.Clear();
            v.AddMember("status", rapidjson::Value().SetBool(false), allocator);
            return v;
            //throw std::runtime_error("OpenCV mat type not supported (" + typname + ")");
      }
      v.AddMember("data", a, allocator);
      return v;
   }

   bool decode_ocv_mat(const rapidjson::Document &document, const char *name, cv::Mat &m)
//------------------------------------------------------------------------------------
   {
      rapidjson::Value::ConstMemberIterator exists = document.FindMember(name);
      if (exists != document.MemberEnd())
      {
         auto o = exists->value.GetObject();
         if ((o.FindMember("status") != document.MemberEnd()) && (!o.FindMember("status")->value.GetBool()))
            return false;

         int type = o.FindMember("type")->value.GetInt(), rows = o.FindMember("rows")->value.GetInt(),
               cols = o.FindMember("cols")->value.GetInt();
         m = cv::Mat::zeros(rows, cols, type);
         auto aa = o.FindMember("data")->value.GetArray();
         int i = 0;
         switch (type)
         {
            case CV_32FC1:
            {
               for (int row = 0; row < rows; row++)
               {
                  float *ptr = m.ptr<float>(row);
                  for (int col = 0; col < cols; col++)
                     ptr[col] = aa[i++].GetFloat();
               }
               break;
            }
            case CV_64FC1:
            {
               for (int row = 0; row < rows; row++)
               {
                  double *ptr = m.ptr<double>(row);
                  for (int col = 0; col < cols; col++)
                     ptr[col] = aa[i++].GetDouble();
               }
            }
               break;
            case CV_8U:
            {
               for (int row = 0; row < rows; row++)
               {
                  uchar *ptr = m.ptr<uchar>(row);
                  for (int col = 0; col < cols; col++)
                     ptr[col] = static_cast<uchar>(aa[i++].GetUint());
               }
            }
               break;
         }
         return true;
      }
      return false;
   }

   bool decode_ocv_rect(const rapidjson::Document &document, const char *name, cv::Rect2d &r)
//------------------------------------------------------------------------------------------
   {
      rapidjson::Value::ConstMemberIterator exists = document.FindMember(name);
      if (exists != document.MemberEnd())
      {
         auto o = exists->value.GetObject();
         r.x = o.FindMember("x")->value.GetInt();
         r.y = o.FindMember("y")->value.GetInt();
         r.width = o.FindMember("width")->value.GetInt();
         r.height = o.FindMember("height")->value.GetInt();
         return true;
      }
      return false;
   }

   cv::Rect2d decode_ocv_rect(rapidjson::Value &o)
//-----------------------------------------------
   {
      cv::Rect2d r;
      r.x = o.FindMember("x")->value.GetInt();
      r.y = o.FindMember("y")->value.GetInt();
      r.width = o.FindMember("width")->value.GetInt();
      r.height = o.FindMember("height")->value.GetInt();
      return r;
   }

   bool decode_ocv_rrect(const rapidjson::Document &document, const char *name, cv::RotatedRect &r)
//--------------====----------------------------------------------------------------------------
   {
      rapidjson::Value::ConstMemberIterator exists = document.FindMember(name);
      if (exists != document.MemberEnd())
      {
         auto o = exists->value.GetObject();
         r.center.x = o.FindMember("cx")->value.GetInt();
         r.center.y = o.FindMember("cy")->value.GetInt();
         r.size.width = o.FindMember("width")->value.GetInt();
         r.size.height = o.FindMember("height")->value.GetInt();
         r.angle = o.FindMember("angle")->value.GetFloat();
         return true;
      }
      return false;
   }

   cv::RotatedRect decode_ocv_rrect(rapidjson::Value &o)
//-----------------------------------------------
   {
      cv::RotatedRect r;
      r.center.x = o.FindMember("cx")->value.GetInt();
      r.center.y = o.FindMember("cy")->value.GetInt();
      r.size.width = o.FindMember("width")->value.GetInt();
      r.size.height = o.FindMember("height")->value.GetInt();
      r.angle = o.FindMember("angle")->value.GetFloat();
      return r;
   }
}