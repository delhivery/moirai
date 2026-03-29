#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <format>
#include <iostream>
#include <mutex>
#include <stop_token>
#include <string_view>
#include <thread>
#include <utility>

namespace moirai {

inline constexpr auto TERMINATION_POLL_INTERVAL =
    std::chrono::milliseconds{250};

enum class LogLevel : std::uint8_t {
  debug = 0,
  information = 1,
  error = 2,
};

class Logger {
public:
  [[nodiscard]] auto enabled(LogLevel severity) const -> bool {
    return severity >= m_level;
  }

  void set_level(std::string_view level) {
    if (level == "debug") {
      m_level = LogLevel::debug;
    } else if (level == "error") {
      m_level = LogLevel::error;
    } else {
      m_level = LogLevel::information;
    }
  }

  void debug(std::string_view message) {
    write(LogLevel::debug, "DEBUG", message);
  }

  void information(std::string_view message) {
    write(LogLevel::information, "INFO", message);
  }

  void error(std::string_view message) {
    write(LogLevel::error, "ERROR", message);
  }

  template <typename... Args>
  void debug(std::format_string<Args...> fmt, Args &&...args) {
    if (!enabled(LogLevel::debug)) {
      return;
    }
    debug(std::format(fmt, std::forward<Args>(args)...));
  }

  template <typename... Args>
  void information(std::format_string<Args...> fmt, Args &&...args) {
    if (!enabled(LogLevel::information)) {
      return;
    }
    information(std::format(fmt, std::forward<Args>(args)...));
  }

  template <typename... Args>
  void error(std::format_string<Args...> fmt, Args &&...args) {
    if (!enabled(LogLevel::error)) {
      return;
    }
    error(std::format(fmt, std::forward<Args>(args)...));
  }

private:
  void write(LogLevel severity, std::string_view label,
             std::string_view message) {
    if (!enabled(severity)) {
      return;
    }

    auto now = std::chrono::floor<std::chrono::seconds>(
        std::chrono::system_clock::now());
    std::scoped_lock guard(m_lock);
    std::clog << std::format("[{:%Y-%m-%d %H:%M:%S}] {:>5} {}\n", now, label,
                             message);
  }

  LogLevel m_level{LogLevel::information};
  std::mutex m_lock;
};

class Application {
public:
  static auto instance() -> Application & {
    static Application application;
    return application;
  }

  auto logger() -> Logger & { return m_logger; }

  static void install_signal_handlers() {
    std::signal(SIGINT, &Application::handle_signal);
    std::signal(SIGTERM, &Application::handle_signal);
  }

  void request_termination() { m_termination_requested.store(true); }

  [[nodiscard]] auto termination_requested() const -> bool {
    return m_termination_requested.load() || signal_requested != 0;
  }

  void wait_for_termination_request() const {
    while (!termination_requested()) {
      std::this_thread::sleep_for(TERMINATION_POLL_INTERVAL);
    }
  }

private:
  static void handle_signal(int signal_number) {
    signal_requested = signal_number;
  }

  inline static volatile std::sig_atomic_t signal_requested{0};
  std::atomic<bool> m_termination_requested{false};
  Logger m_logger;
};

template <typename Rep, typename Period>
auto wait_for(const std::stop_token &stop_token,
              std::chrono::duration<Rep, Period> duration) -> bool {
  if (stop_token.stop_requested()) {
    return false;
  }

  std::mutex mutex;
  std::condition_variable_any cond_var;
  std::unique_lock lock(mutex);
  cond_var.wait_for(lock, stop_token, duration, []() -> auto { return false; });
  return !stop_token.stop_requested();
}

} // namespace moirai
