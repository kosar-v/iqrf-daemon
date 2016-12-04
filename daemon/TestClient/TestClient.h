#pragma once

#include "IClient.h"
#include "MqttMessaging.h"
#include "SimpleSerializer.h"
#include "TaskQueue.h"
#include "IMessaging.h"
#include "IScheduler.h"
#include <string>
#include <chrono>
#include <map>
#include <memory>

class IDaemon;

typedef std::basic_string<unsigned char> ustring;

class TestClient : public IClient
{
public:
  TestClient() = delete;
  TestClient(const std::string& name);
  virtual ~TestClient();

  void setDaemon(IDaemon* daemon) override;
  virtual void setSerializer(ISerializer* serializer) override;
  virtual void setMessaging(IMessaging* messaging) override;
  const std::string& getClientName() const override {
    return m_name;
  }
  void start() override;
  void stop() override;

private:
  void handleMsgFromMessaging(const ustring& msg);

  std::string m_name;

  IMessaging* m_messaging;
  IDaemon* m_daemon;
  ISerializer* m_serializer;
};