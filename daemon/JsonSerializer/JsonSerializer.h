#pragma once

#include "ISerializer.h"
#include "ObjectFactory.h"
#include "PrfRaw.h"
#include "PrfThermometer.h"
#include "PrfLeds.h"
#include "PlatformDep.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include <memory>
#include <string>

void parseRequestJson(DpaTask& dpaTask, rapidjson::Value& val);
void encodeResponseJson(DpaTask& dpaTask, rapidjson::Value& val, rapidjson::Document::AllocatorType& alloc);

class PrfRawJson : public PrfRaw
{
public:
  explicit PrfRawJson(rapidjson::Value& val);
  virtual ~PrfRawJson() {}
  std::string encodeResponse(const std::string& errStr) const  override;
};

class PrfThermometerJson : public PrfThermometer
{
public:
  explicit PrfThermometerJson(rapidjson::Value& val);
  virtual ~PrfThermometerJson() {}
  std::string encodeResponse(const std::string& errStr) const  override;
};

template <typename L>
class PrfLedJson : public L
{
public:
  explicit PrfLedJson(rapidjson::Value& val) {
    parseRequestJson(*this, val);
  }

  virtual ~PrfLedJson() {}

  std::string encodeResponse(const std::string& errStr) const override
  {
    rapidjson::Document doc;
    doc.SetObject();

    encodeResponseJson(*this, doc, doc.GetAllocator());

    rapidjson::Value v;
    v = L::getLedState();
    doc.AddMember("LedState", v, doc.GetAllocator());

    v.SetString(errStr.c_str(), doc.GetAllocator());
    doc.AddMember("Status", v, doc.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
  }

};

typedef PrfLedJson<PrfLedG> PrfLedGJson;
typedef PrfLedJson<PrfLedR> PrfLedRJson;

class DpaTaskJsonSerializerFactory : public ObjectFactory<DpaTask, rapidjson::Value>, public ISerializer
{
public:
  virtual ~DpaTaskJsonSerializerFactory() {}
  DpaTaskJsonSerializerFactory();

  std::unique_ptr<DpaTask> parseRequest(const std::string& request) override;
  std::string getLastError() const override;
private:
  std::string m_lastError;
};
