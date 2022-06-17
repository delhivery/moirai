#ifndef MOIRAI_RUNNABLE
#define MOIRAI_RUNNABLE

#include <Poco/Logger.h>
#include <Poco/Runnable.h>

class Runnable : public Poco::Runnable
{
protected:
  std::atomic<bool> mStop = false;
  Poco::Logger& mLogger;
}
#endif
