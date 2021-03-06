#include "utils/component.hxx"
#include <CLI/App.hpp>

namespace ambasta {
namespace utils {
Component::Component(std::shared_ptr<CLI::App> app)
  : m_app(app)
{}
}
}
