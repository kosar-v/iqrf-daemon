#include "MessagingController.h"
#include "IqrfLogging.h"

#include <signal.h>
#include <chrono>
#include <thread>

#include "PlatformDep.h"

#ifdef WIN
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

std::unique_ptr<MessagingController> msgCtrl;

//-------------------------------------------------
//catches CTRL-C to stop main loop
void SignalHandler(int signal)
{
  switch (signal)
  {
  case SIGINT:
  case SIGTERM:
  case SIGABRT:
#ifndef WIN
  case SIGQUIT:
  case SIGKILL:
  case SIGSTOP:
  case SIGHUP:
#endif
    if (nullptr != msgCtrl)
      msgCtrl->exit();
    break;

  default:
    // ...
    break;
  }
}

#ifdef WIN
//--------------------------------------------------------------------------------------------------
/// \brief Handler for close signals on Windows.
///
BOOL WINAPI ConsoleHandlerRoutine(DWORD dwCtrlType)
{
  if (dwCtrlType == CTRL_CLOSE_EVENT)
  {
    if (msgCtrl)
      msgCtrl->exit();
    std::this_thread::sleep_for(std::chrono::milliseconds(10000)); //Win waits ~10 sec and then exits anyway
    return TRUE;
  }
  return FALSE;
}
#endif

//--------------------------------------------------------------------------------------------------
int main(int argc, char** argv)
{
#if defined(WIN) && defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  std::string configFile;

  if (SIG_ERR == signal(SIGINT, SignalHandler)) {
    std::cerr << std::endl << "Could not set control handler for SIGINT";
    return -1;
  }

  if (SIG_ERR == signal(SIGTERM, SignalHandler)) {
    std::cerr << std::endl << "Could not set control handler for SIGTERM";
    return -1;
  }
  if (SIG_ERR == signal(SIGABRT, SignalHandler)) {
    std::cerr << std::endl << "Could not set control handler for SIGABRT";
    return -1;
  }
#ifndef WIN
  if (SIG_ERR == signal(SIGQUIT, SignalHandler)) {
    std::cerr << std::endl << "Could not set control handler for SIGQUIT";
    return -1;
  }
#else
  if (SetConsoleCtrlHandler(ConsoleHandlerRoutine, TRUE) == 0) {
    std::cerr << std::endl << "SetConsoleCtrlHandler failed: GetLastError: " << GetLastError();
    return -1;
  }
#endif

  if (argc < 2) {
    std::cerr << std::endl << "Usage" << std::endl;
    std::cerr << "  iqrf_startup <config file>" << std::endl << std::endl;
    std::cerr << "Example" << std::endl;
    std::cerr << "  iqrf_startup config.json" << std::endl;
    return (-1);
  }
  else {
    configFile = argv[1];
  }

  std::cout << std::endl << argv[0] << " started";

  try {
    msgCtrl = std::unique_ptr<MessagingController>(ant_new MessagingController(configFile));
    msgCtrl->watchDog();
  }
  catch (std::exception &e) {
    std::cerr << std::endl << "Fatal error: " << e.what();
  }

  std::cout << std::endl << argv[0] << " finished";
}
