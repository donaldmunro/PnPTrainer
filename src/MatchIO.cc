#include <ostream>
#include <fstream>
#ifdef STD_FILESYSTEM
#include <filesystem>
namespace filesystem = std::filesystem;
#endif
#ifdef FILESYSTEM_EXPERIMENTAL
#include <experimental/filesystem>
#include <fstream>

namespace filesystem = std::experimental::filesystem;
#endif
#ifdef FILESYSTEM_BOOST
#include <boost/filesystem.hpp>
namespace filesystem = boost::filesystem;
#endif

#include <opencv2/core/core.hpp>

#include "MatchIO.h"
#include "json.h"
#define TINYFORMAT_USE_VARIADIC_TEMPLATES
#include "tinyformat.h"

bool JsonMatchIO::write(const char *filename, std::vector<matched_t> &matchedFeatures, const DetectorInfo* detectorInfo,
                        bool is_write_keypoints, bool isPrettyPrint, std::ostream* err = nullptr)
//-----------------------------------------------------------------------------------------------------------------
{
   filesystem::path path(filename);
   if (path.has_parent_path())
   {
      filesystem::path dir = filesystem::canonical(path.parent_path());
      if (! filesystem::is_directory(dir))
      {
         if (err != nullptr)
            *err << "Parent directory " << dir.string() << " not found.";
         return false;
      }
   }
   rapidjson::Document document;
   rapidjson::Document::AllocatorType& allocator = document.GetAllocator();
   document.SetObject();
   const std::function<rapidjson::Value(const cv::KeyPoint&, rapidjson::Document::AllocatorType& allocator)> kpfn
         = &jsoncv::encode_ocv_keypoint;
   rapidjson::Value detector_info(rapidjson::kObjectType);
   rapidjson::Value detector_params(rapidjson::kObjectType);
   if (detectorInfo != nullptr)
   {
      detector_info.AddMember("name", rapidjson::Value().SetString(detectorInfo->name, allocator), allocator);
      for (auto it=detectorInfo->parameters.begin(); it != detectorInfo->parameters.end(); ++it)
      {
         rapidjson::GenericStringRef k(it->first.c_str());
         std::string v = it->second;
         detector_params.AddMember(k, rapidjson::Value().SetString(v, allocator), allocator);
      }
      detector_info.AddMember("parameters", detector_params, allocator);
   }
   else
      detector_info.AddMember("name", rapidjson::Value().SetString("unknown", allocator), allocator);
   document.AddMember("detector", detector_info, allocator);
   rapidjson::Value feature_array(rapidjson::kArrayType);
   for (size_t i=0; i<matchedFeatures.size(); i++)
   {
      matched_t match = matchedFeatures[i];
      Real3<float>& pt = match.point_3d;
      std::vector<features_t*>* features = match.features_2d.get();
      if (features == nullptr)
         continue;

      rapidjson::Value match_record(rapidjson::kObjectType);
      rapidjson::Value pt3d(rapidjson::kObjectType);
      pt3d.AddMember("x", rapidjson::Value().SetFloat(pt.x), allocator);
      pt3d.AddMember("y", rapidjson::Value().SetFloat(pt.y), allocator);
      pt3d.AddMember("z", rapidjson::Value().SetFloat(pt.z), allocator);
      match_record.AddMember("match3d", pt3d, allocator);
      rapidjson::Value array2D(rapidjson::kArrayType);
      for (features_t* feature : *features)
      {
         rapidjson::Value record2D(rapidjson::kObjectType);
         cv::Mat d = feature->descriptor;
         if (d.empty())
            continue;
         record2D.AddMember("descriptor", jsoncv::encode_ocv_mat(d, allocator), allocator);
         if (is_write_keypoints)
            record2D.AddMember("keypoint", jsoncv::encode_ocv_keypoint(feature->keypoint, allocator), allocator);
         array2D.PushBack(record2D, allocator);
      }
      match_record.AddMember("matches2D", array2D, allocator);
      //document.AddMember("record", match_record, allocator);
      feature_array.PushBack(match_record, allocator);
   }
   document.AddMember("matches", feature_array, allocator);
   std::ofstream of(filename);
//   rapidjson::Writer<rapidjson::StringBuffer>* pwriter;
   rapidjson::StringBuffer strbuf;
   if (isPrettyPrint)
   {
       rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(strbuf);
       writer.SetIndent(' ', 2);
//       pwriter = &writer;
      document.Accept(writer);
   }
   else
   {
      rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
//      pwriter = &writer;
      document.Accept(writer);
   }
   //document.Accept(*pwriter);
   of << strbuf.GetString() << std::endl;
   return of.good();
}

bool XMLMatchIO::write(const char *filename, std::vector<matched_t> &matchedFeatures, const DetectorInfo *detectorInfo,
                       bool isWriteKeypoints, bool isPrettyPrint, std::ostream *err)
//--------------------------------------------------------------------------------------------------------------------
{
   cv::FileStorage fs;
   bool ret = true;
   try
   {
      fs.open(cv::String(filename), cv::FileStorage::WRITE || cv::FileStorage::FORMAT_XML);
      if (! fs.isOpened())
         return false;
      fs << "detector" << "{" << "name";
      if (detectorInfo == nullptr)
         fs << "unknown";
      else
      {
         fs << detectorInfo->name << "parameters" << "{";
         for (auto it = detectorInfo->parameters.begin(); it != detectorInfo->parameters.end(); ++it)
            fs << it->first << it->second;
         fs << "}";
      }
      fs << "}" << "matches" << "[";
      for (size_t i = 0; i < matchedFeatures.size(); i++)
      {
         matched_t match = matchedFeatures[i];
         Real3<float> &pt = match.point_3d;
         std::vector<features_t *> *features = match.features_2d.get();
         if (features == nullptr)
            continue;
         fs << "match3d" << "{" << "x" << pt.x << "y" << pt.y << "z" << pt.z << "}";
         fs << "matches2D" << "[";
         for (features_t *feature : *features)
         {

            cv::Mat d = feature->descriptor;
            if (d.empty())
               continue;
            fs << "match2d" << "{";
            fs << "descriptor" << d << "keypoint" << feature->keypoint << "}";
         }
         fs << "]";
      }
      fs << "]";
   }
   catch (...)
   {
      ret = false;
   }
   if (fs.isOpened())
      fs.release();
   return ret;
}


