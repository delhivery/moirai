#ifndef UTILS_COMPONENTS_HXX
#define UTILS_COMPONENTS_HXX

#include <CLI/CLI.hpp>
#include <memory>
#include <stop_token>

namespace ambasta {
namespace utils {
class Component
{
protected:
  std::shared_ptr<CLI::App> m_app;

public:
  Component(std::shared_ptr<CLI::App>);

  virtual void initialize() = 0;

  virtual void define_options() = 0;

  virtual int main(std::stop_token) = 0;
};
}
}

#endif
