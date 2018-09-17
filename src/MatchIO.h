#ifndef _MATCHIO_H_
#define _MATCHIO_H_

#include "types.h"

class MatchIO
{
public:
   virtual bool write(const char* filename, std::vector<matched_t>& matchedFeatures,
                      const DetectorInfo* detectorInfo =nullptr, bool isWriteKeypoints =false, bool isPrettyPrint =true,
                      std::ostream* err = nullptr) =0;
   virtual ~MatchIO() {} //shut up compiler
};

class JsonMatchIO : public MatchIO
{
public:
   virtual bool write(const char *filename, std::vector<matched_t> &matchedFeatures,
                      const DetectorInfo* detectorInfo, bool isWriteKeypoints, bool isPrettyPrint,
                      std::ostream* err) override ;
};

class XMLMatchIO : public MatchIO
{
public:
   virtual bool write(const char *filename, std::vector<matched_t> &matchedFeatures,
                      const DetectorInfo* detectorInfo, bool isWriteKeypoints, bool isPrettyPrint,
                      std::ostream* err) override ;
};


#endif //PNPTRAINER_MATCHIO_H
