#ifndef MOIRAI_RUNNABLE
#define MOIRAI_RUNNABLE

#include <Poco/Logger.h>
#include <Poco/Runnable.h>

class Runnable : public Poco::Runnable
{
protected:
  static constexpr uint16_t POLL_INTERVAL = 256;

  std::atomic<bool> mStop = false;

  Poco::Logger& mLogger;

public:
  void stop(bool);
}
#endif
