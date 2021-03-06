#include "Scheduler.h"
#include "IqrfLogging.h"
#include "PlatformDep.h"

using namespace std::chrono;

Scheduler::Scheduler()
  :m_dpaTaskQueue(nullptr)
{
}

Scheduler::~Scheduler()
{
}

void Scheduler::updateConfiguration(rapidjson::Value& cfg)
{
  TRC_ENTER("");
  jutils::assertIsObject("", cfg);

  std::vector<std::string> records = jutils::getMemberAsVector<std::string>("Tasks", cfg);

  std::vector<std::shared_ptr<ScheduleRecord>> tempRecords;
  int i = 0;
  for (auto & it : records) {
    tempRecords.push_back(std::shared_ptr<ScheduleRecord>(ant_new ScheduleRecord(it)));
  }

  addScheduleRecords(tempRecords);

  TRC_LEAVE("");
}

void Scheduler::start()
{
  TRC_ENTER("");

  m_dpaTaskQueue = ant_new TaskQueue<ScheduleRecord>([&](const ScheduleRecord& record) {
    handleScheduledRecord(record);
  });

  m_scheduledTaskPushed = false;
  m_runTimerThread = true;
  m_timerThread = std::thread(&Scheduler::timer, this);

  TRC_INF("Scheduler started");

  TRC_LEAVE("");
}

void Scheduler::stop()
{
  TRC_ENTER("");
  {
    m_runTimerThread = false;
    std::unique_lock<std::mutex> lck(m_conditionVariableMutex);
    m_scheduledTaskPushed = true;
    m_conditionVariable.notify_one();
  }

  if (m_timerThread.joinable())
    m_timerThread.join();

  delete m_dpaTaskQueue;
  m_dpaTaskQueue = nullptr;

  TRC_INF("Scheduler stopped");
  TRC_LEAVE("");
}

int Scheduler::handleScheduledRecord(const ScheduleRecord& record)
{
  TRC_DBG("==================================" << std::endl <<
    "Scheduled msg: " << std::endl << FORM_HEX(record.getTask().data(), record.getTask().size()));

  {
    std::lock_guard<std::mutex> lck(m_messageHandlersMutex);
    auto found = m_messageHandlers.find(record.getClientId());
    if (found != m_messageHandlers.end()) {
      TRC_DBG(NAME_PAR(Response, record.getTask()) << " has been passed to: " << NAME_PAR(ClinetId, record.getClientId()));
      found->second(record.getTask());
    }
    else {
      TRC_DBG("Unregistered client: " << PAR(record.getClientId()));
    }
  }

  return 0;
}

void Scheduler::addScheduleRecord(std::shared_ptr<ScheduleRecord>& record)
{
  system_clock::time_point timePoint;
  std::tm timeStr;
  ScheduleRecord::getTime(timePoint, timeStr);
  TRC_DBG(ScheduleRecord::asString(timePoint));

  // lock and insert
  std::lock_guard<std::mutex> lck(m_scheduledTasksMutex);
  system_clock::time_point tp = record->getNext(timeStr);
  m_scheduledTasks.insert(std::make_pair(tp, record));

  // notify timer thread
  std::unique_lock<std::mutex> lckn(m_conditionVariableMutex);
  m_scheduledTaskPushed = true;
  m_conditionVariable.notify_one();
}

void Scheduler::addScheduleRecords(std::vector<std::shared_ptr<ScheduleRecord>>& records)
{
  system_clock::time_point timePoint;
  std::tm timeStr;
  ScheduleRecord::getTime(timePoint, timeStr);
  TRC_DBG(ScheduleRecord::asString(timePoint));

  // lock and insert
  std::lock_guard<std::mutex> lck(m_scheduledTasksMutex);
  for (auto & record : records) {
    system_clock::time_point tp = record->getNext(timeStr);
    m_scheduledTasks.insert(std::make_pair(tp, record));
  }

  // notify timer thread
  std::unique_lock<std::mutex> lckn(m_conditionVariableMutex);
  m_scheduledTaskPushed = true;
  m_conditionVariable.notify_one();
}

//thread function
void Scheduler::timer()
{
  system_clock::time_point timePoint;
  std::tm timeStr;
  ScheduleRecord::getTime(timePoint, timeStr);
  TRC_DBG(ScheduleRecord::asString(timePoint));

  while (m_runTimerThread) {
    
    { // wait for something in the m_scheduledTasks;
      std::unique_lock<std::mutex> lck(m_conditionVariableMutex);
      m_conditionVariable.wait_until(lck, timePoint, [&]{ return m_scheduledTaskPushed; });
      m_scheduledTaskPushed = false;
    }

    // get actual time
    ScheduleRecord::getTime(timePoint, timeStr);

    std::lock_guard<std::mutex> lck(m_scheduledTasksMutex);

    // fire all expired tasks
    while (!m_scheduledTasks.empty()) {

      auto begin = m_scheduledTasks.begin();
      std::shared_ptr<ScheduleRecord> record = begin->second;

      if (begin->first < timePoint) {
        
        // fire
    	TRC_INF("Task fired at: " << ScheduleRecord::asString(timePoint));
        m_dpaTaskQueue->pushToQueue(*record); //copy record

        // erase fired
        m_scheduledTasks.erase(begin);

        // get and schedule next
        system_clock::time_point nextTimePoint = record->getNext(timeStr);
        if (nextTimePoint >= timePoint) {
          m_scheduledTasks.insert(std::make_pair(nextTimePoint, record));
        }
      }
      else {
        break;
      }
    }

    // get next wakeup time
    if (!m_scheduledTasks.empty()) {
      timePoint = m_scheduledTasks.begin()->first;
    }
    else {
      timePoint += duration<int>(10);
    }

  }
}

void Scheduler::registerMessageHandler(const std::string& clientId, TaskHandlerFunc fun)
{
  std::lock_guard<std::mutex> lck(m_messageHandlersMutex);
  //TODO check success
  m_messageHandlers.insert(make_pair(clientId, fun));
}

void Scheduler::unregisterMessageHandler(const std::string& clientId)
{
  std::lock_guard<std::mutex> lck(m_messageHandlersMutex);
  m_messageHandlers.erase(clientId);
}

std::vector<std::string> Scheduler::getMyTasks(const std::string& clientId)
{
  std::vector<std::string> retval;
  // lock and copy
  std::lock_guard<std::mutex> lck(m_scheduledTasksMutex);
  for (auto & task : m_scheduledTasks) {
    if (task.second->getClientId() == clientId) {
      retval.push_back(task.second->getTask());
    }
  }
  return retval;
}

void Scheduler::removeAllMyTasks(const std::string& clientId)
{
  // lock and remove
  std::lock_guard<std::mutex> lck(m_scheduledTasksMutex);
  for (auto it = m_scheduledTasks.begin(); it != m_scheduledTasks.end(); ++it) {
    if (it->second->getClientId() == clientId) {
      it = m_scheduledTasks.erase(it);
    }
  }
}

/////////////////////////////////////////
ScheduleRecord::ScheduleRecord(const std::string& rec)
{
  m_tm.tm_sec = -1;
  m_tm.tm_min = -1;
  m_tm.tm_hour = -1;
  m_tm.tm_mday = -1;
  m_tm.tm_mon = -1;
  m_tm.tm_year = -1;
  m_tm.tm_wday = -1;
  m_tm.tm_yday = -1;
  m_tm.tm_isdst = -1;

  parse(rec);
}

std::vector<std::string> ScheduleRecord::preparseMultiRecord(const std::string& rec)
{
  std::istringstream is(rec);

  std::string sec("*");
  std::string mnt("*");
  std::string hrs("*");
  std::string day("*");
  std::string mon("*");
  std::string yer("*");
  std::string dow("*");

  is >> sec >> mnt >> hrs >> day >> mon >> yer >> dow;
  //TODO
  std::vector<std::string> retval;
  return retval;
}

void ScheduleRecord::parse(const std::string& rec)
{
  std::istringstream is(rec);

  std::string sec("*");
  std::string mnt("*");
  std::string hrs("*");
  std::string day("*");
  std::string mon("*");
  std::string yer("*");
  std::string dow("*");

  is >> sec >> mnt >> hrs >> day >> mon >> yer >> dow >> m_clientId;
  std::getline(is, m_dpaTask, '|'); // just to get rest of line with spaces
  
  m_tm.tm_sec = parseItem(sec, 0, 59);
  m_tm.tm_min = parseItem(mnt, 0, 59);
  m_tm.tm_hour = parseItem(hrs, 0, 23);
  m_tm.tm_mday = parseItem(day, 1, 31);
  m_tm.tm_mon = parseItem(mon, 1, 12) - 1;
  m_tm.tm_year = parseItem(yer, 0, 9000);
  m_tm.tm_wday = parseItem(dow, 0, 6);
}

int ScheduleRecord::parseItem(std::string& item, int mnm, int mxm)
{
  int num;
  try {
    num = std::stoi(item);
    if (num < mnm || num > mxm) {
      num = -1;
    }
  }
  catch (...) {
    num = -1; //TODO
  }
  return num;
}

system_clock::time_point ScheduleRecord::getNext(const std::tm& actualTime)
{
  bool lower = false;
  std::tm c_tm = actualTime;

  while (true) {
    if (!compareTimeVal(c_tm.tm_sec, m_tm.tm_sec, lower))
      break;
    if (!compareTimeVal(c_tm.tm_min, m_tm.tm_min, lower))
      break;
    if (!compareTimeVal(c_tm.tm_hour, m_tm.tm_hour, lower))
      break;
    //Compare Day of Week if present
    if (m_tm.tm_wday >= 0) {
      int dif = c_tm.tm_wday - m_tm.tm_wday;
      if (dif == 0 && !lower)
        c_tm.tm_mday++;
      else if (dif > 0)
        c_tm.tm_mday += 7 - dif;
      else
        c_tm.tm_mday -= dif;
      break;
    }
    if (!compareTimeVal(c_tm.tm_mday, m_tm.tm_mday, lower))
      break;
    if (!compareTimeVal(c_tm.tm_mon, m_tm.tm_mon, lower))
      break;
    if (!compareTimeVal(c_tm.tm_year, m_tm.tm_year, lower))
      break;

    break;
  }

  std::time_t tt = std::mktime(&c_tm);
  if (tt == -1) {
    throw "no valid system time"; //TODO
  }
  
  system_clock::time_point tp = system_clock::from_time_t(tt);
  m_next = asString(tp);
  return tp;
}

//return true - continue false - finish
bool ScheduleRecord::compareTimeVal(int& cval, int tval, bool& lw) const
{
  int dif;
  if (tval >= 0) {
    lw = (dif = cval - tval) < 0 ? true : dif > 0 ? false : lw;
    cval = tval;
    return true;
  }
  else {
    if (!lw) cval++;
    return false;
  }
}

std::string ScheduleRecord::asString(const std::chrono::system_clock::time_point& tp)
{
  // convert to system time:
  std::time_t t = std::chrono::system_clock::to_time_t(tp);
  std::string ts = ctime(&t);   // convert to calendar time
  ts.resize(ts.size() - 1);       // skip trailing newline
  return ts;
}

void ScheduleRecord::getTime(std::chrono::system_clock::time_point& timePoint, std::tm& timeStr)
{
  timePoint = system_clock::now();
  time_t tt;
  tt = system_clock::to_time_t(timePoint);
  std::tm* timeinfo;
  timeinfo = localtime(&tt);
  timeStr = *timeinfo;
}

