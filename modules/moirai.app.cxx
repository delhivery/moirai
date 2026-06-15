export module moirai.app;

export import std;

export namespace moirai {

inline constexpr auto TERMINATION_POLL_INTERVAL =
    std::chrono::milliseconds{250};
inline constexpr int INTERRUPT_SIGNAL = 2;
inline constexpr int TERMINATION_SIGNAL = 15;

enum class LogLevel : std::uint8_t {
  debug = 0,
  information = 1,
  error = 2,
};

class Logger {
public:
  using Sink = std::function<void(LogLevel, std::string_view, std::string_view)>;

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

  void set_sink(Sink sink) {
    std::scoped_lock guard(m_lock);
    m_sink = std::move(sink);
  }

  void clear_sink() {
    std::scoped_lock guard(m_lock);
    m_sink = nullptr;
  }

  template <typename... Args>
  void debug(std::format_string<Args...> fmt, Args&&... args) {
    if (!enabled(LogLevel::debug)) {
      return;
    }
    debug(std::format(fmt, std::forward<Args>(args)...));
  }

  template <typename... Args>
  void information(std::format_string<Args...> fmt, Args&&... args) {
    if (!enabled(LogLevel::information)) {
      return;
    }
    information(std::format(fmt, std::forward<Args>(args)...));
  }

  template <typename... Args>
  void error(std::format_string<Args...> fmt, Args&&... args) {
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
    if (m_sink != nullptr) {
      m_sink(severity, label, message);
      return;
    }
    std::clog << std::format("[{:%Y-%m-%d %H:%M:%S}] {:>5} {}\n", now, label,
                             message);
  }

  LogLevel m_level{LogLevel::information};
  std::mutex m_lock;
  Sink m_sink;
};

class Application {
public:
  static auto instance() -> Application& {
    static Application application;
    return application;
  }

  auto logger() -> Logger& { return m_logger; }

  static void install_signal_handlers() {
    std::signal(INTERRUPT_SIGNAL, &Application::handle_signal);
    std::signal(TERMINATION_SIGNAL, &Application::handle_signal);
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
auto wait_for(const std::stop_token& stop_token,
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
