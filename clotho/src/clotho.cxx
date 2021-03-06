#include "utils/application.hxx"
#include "utils/component.hxx"

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

  int main()
  {
    std::cout << "Some input set to: " << some_input << std::endl;
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
    m_app->add_option("-c,--config", config_file, "Configuration file");
  }

  int main()
  {
    std::cout << "config file: " << config_file << std::endl;
    return 0;
  }
};

MAIN(Clotho);
