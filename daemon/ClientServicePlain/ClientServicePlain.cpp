#include "ClientServicePlain.h"
#include "DpaTransactionTask.h"
#include "IDaemon.h"
#include "IqrfLogging.h"

ClientServicePlain::ClientServicePlain(const std::string & name)
  :m_name(name)
  , m_messaging(nullptr)
  , m_daemon(nullptr)
{
}

ClientServicePlain::~ClientServicePlain()
{
}

void ClientServicePlain::setDaemon(IDaemon* daemon)
{
  m_daemon = daemon;
}

void ClientServicePlain::setSerializer(ISerializer* serializer)
{
  m_serializerVect.push_back(serializer);
  m_messaging->registerMessageHandler([&](const ustring& msg) {
    handleMsgFromMessaging(msg);
  });
}

void ClientServicePlain::setMessaging(IMessaging* messaging)
{
  m_messaging = messaging;
}

void ClientServicePlain::start()
{
  TRC_ENTER("");

  m_daemon->getScheduler()->registerMessageHandler(m_name, [&](const std::string& msg) {
    ustring msgu((unsigned char*)msg.data(), msg.size());
    handleMsgFromMessaging(msgu);
  });

  TRC_INF("ClientServicePlain :" << PAR(m_name) << " started");

  TRC_LEAVE("");
}

void ClientServicePlain::stop()
{
  TRC_ENTER("");

  m_daemon->getScheduler()->unregisterMessageHandler(m_name);

  TRC_INF("ClientServicePlain :" << PAR(m_name) << " stopped");
  TRC_LEAVE("");
}

void ClientServicePlain::handleMsgFromMessaging(const ustring& msg)
{
  TRC_DBG("==================================" << std::endl <<
    "Received from MESSAGING: " << std::endl << FORM_HEX(msg.data(), msg.size()));

  //to encode output message
  std::ostringstream os;

  //get input message
  std::string msgs((const char*)msg.data(), msg.size());
  std::istringstream is(msgs);

  std::unique_ptr<DpaTask> dpaTask;
  for (auto ser : m_serializerVect) {
    dpaTask = ser->parseRequest(msgs);
    if (dpaTask) {
      break;
    }
  }
  if (dpaTask) {
    DpaTransactionTask trans(*dpaTask);
    m_daemon->executeDpaTransaction(trans);
    int result = trans.waitFinish();
    os << dpaTask->encodeResponse(trans.getErrorStr());
  }
  else {
    //os << m_serializer->getLastError();
    os << "PARSE ERROR";
  }

  ustring msgu((unsigned char*)os.str().data(), os.str().size());
  m_messaging->sendMessage(msgu);
}
