#ifndef MOIRAI_RUNNABLE
#define MOIRAI_RUNNABLE

#include <Poco/Logger.h>
#include <Poco/Runnable.h>

class Runnable : public Poco::Runnable
{
private:
  std::atomic<bool> mStop;

protected:
  static constexpr uint16_t POLL_INTERVAL = 256;

  auto stop() const -> bool;

public:
  void stop(bool);

  virtual auto logger() const -> Poco::Logger& = 0;
};
#endif
