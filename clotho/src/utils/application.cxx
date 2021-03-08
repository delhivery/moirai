#include "utils/application.hxx"
#include <CLI/CLI.hpp>
#include <algorithm>
#include <memory>
#include <signal.h>
#include <spdlog/spdlog.h>
#include <sysexits.h>
#include <thread>

namespace ambasta {
namespace utils {

Application::Application()
  : Component(std::make_shared<CLI::App>())
{
  spdlog::set_pattern("[%H:%M:%S %z] [%n] [%^---%L---%$] [thread %t] %v");
}

int
Application::main(std::stop_token token)
{
  return EX_OK;
}

void
Application::init()
{
  std::filesystem::path global_config_path{ std::format(
    "{}/{}", GLOBAL_CONF_DIR, PROJECT_CONFIG_SUBPATH) };

  std::filesystem::path user_config_path{ std::format(
    "{}/{}", std::getenv("XDG_CONFIG_HOME"), PROJECT_CONFIG_SUBPATH) };

  if (std::filesystem::exists(global_config_path)) {
    m_config_files.push_back(global_config_path);
  }

  if (std::filesystem::exists(user_config_path)) {
    m_config_files.push_back(user_config_path);
  }
}

int
Application::init(int argc, char** argv)
{
  init();
  define_options();

  for (auto& component : m_components) {
    component->define_options();
  }

  try {
    m_app->parse(argc, argv);
    initialize();

    for (auto& component : m_components) {
      component->initialize();
    }

    std::for_each(
      m_components.begin(), m_components.end(), [this](auto& component) {
        m_threads.emplace_back([&component](std::stop_token stop_token) {
          component->main(stop_token);
        });
      });
    m_threads.push_back(
      std::jthread{ [this](std::stop_token stop_token) { main(stop_token); } });
    return EX_OK;
  } catch (const CLI::ParseError& exc) {
    m_app->exit(exc);
    return EX_USAGE;
  } catch (const std::exception& exc) {
    spdlog::error("Failed to init submodule: {}", exc.what());
    return EX_CONFIG;
  }
}

int
Application::run(int argc, char** argv)
{

  int l_result_status = EX_OK;

  try {
    sigset_t l_waited_signals;
    sigemptyset(&l_waited_signals);
    sigaddset(&l_waited_signals, SIGINT);
    sigaddset(&l_waited_signals, SIGQUIT);
    sigaddset(&l_waited_signals, SIGTERM);
    sigprocmask(SIG_BLOCK, &l_waited_signals, nullptr);
    // Initialize application
    l_result_status = init(argc, argv);

    if (l_result_status == EX_OK) {
      sd_notify(0, "READY=1");
      spdlog::info("Successfully started");

      int l_signal;
      sigwait(&l_waited_signals, &l_signal);
      sd_notify(0, "STOPPING=1");

      // Deinitialize application
      std::for_each(m_threads.begin(), m_threads.end(), [](auto& thread) {
        thread.request_stop();
        thread.join();
      });
      spdlog::info("Successfully terminated");
    }
  } catch (std::exception& exc) {
    sd_notify(0,
              std::format("STATUS=Failed to start up: {}\n ERRNO={}",
                          exc.what(),
                          l_result_status)
                .c_str());
    return (l_result_status);
  }
  l_result_status = 0;
  return (l_result_status);
}

}
}
