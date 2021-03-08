#include "utils/application.hxx"
#include "utils/component.hxx"
#include <thread>

using namespace std::literals;

class Reader : public ambasta::utils::Component
{
private:
  std::string some_input;

public:
  Reader(std::shared_ptr<CLI::App> app)
    : ambasta::utils::Component(app)
  {}

  void initialize() {}

  void define_options()
  {
    auto option_group = m_app->add_option_group("reader");
    option_group->add_option("-s, --some_input", some_input, "Read some input");
  }

  int main(std::stop_token stop_token)
  {
    while (true) {
      std::this_thread::sleep_for(0.2s);
      std::cout << "Component thread" << std::endl;

      if (stop_token.stop_requested()) {
        std::cout << "Component stopping" << std::endl;
        break;
      }
    }
    return EX_OK;
  }
};

class Clotho : public ambasta::utils::Application
{
private:
  std::filesystem::path config_file;

public:
  void initialize() { add_component<Reader>("filereader"); }

  void define_options()
  {
    m_app->add_option("-x,--xonfig", config_file, "Configuration file");
  }

  int main(std::stop_token stop_token)
  {
    std::cout << "config file: " << config_file << std::endl;

    while (true) {
      std::this_thread::sleep_for(0.5s);
      std::cout << "Main thread" << std::endl;

      if (stop_token.stop_requested()) {
        std::cout << "Application stopping" << std::endl;
        break;
      }
    }
    return EX_OK;
  }
};

MAIN(Clotho);
