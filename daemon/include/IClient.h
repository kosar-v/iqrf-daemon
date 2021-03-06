#pragma once

#include <string>
#include <functional>

class IDaemon;
class ISerializer;
class IMessaging;

class IClient
{
public:
  virtual ~IClient() {};
  virtual void setDaemon(IDaemon* daemon) = 0;
  virtual void setSerializer(ISerializer* serializer) = 0;
  virtual void setMessaging(IMessaging* messaging) = 0;
  virtual const std::string& getClientName() const = 0;

  virtual void start() = 0;
  virtual void stop() = 0;
};
