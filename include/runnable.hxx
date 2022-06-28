#ifndef MOIRAI_RUNNABLE
#define MOIRAI_RUNNABLE

#include <atomic>
#include <chrono>
#include <spdlog/logger.h>

class Runnable {
private:
  std::atomic<bool> mStop;

protected:
  static constexpr std::chrono::milliseconds POLL_INTERVAL{256};

  auto stop() const -> bool;

  virtual auto logger() const -> std::shared_ptr<spdlog::logger> = 0;

public:
  void stop(bool);

  virtual auto run() -> int = 0;
};
#endif
