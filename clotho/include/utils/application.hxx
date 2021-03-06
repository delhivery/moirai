#ifndef UTILS_APPLICATION_HXX
#define UTILS_APPLICATION_HXX

#include "config.hxx"
#include "format.hxx"
#include "utils/component.hxx"
#include <signal.h>
#include <sysexits.h>
#include <systemd/sd-daemon.h>
#include <vector>

namespace ambasta {
namespace utils {

class Application : public Component
{
private:
  std::vector<std::filesystem::path> m_config_files;

  std::vector<std::unique_ptr<Component>> m_components;

protected:
  // called only by init(c, v)
  void init();

  // called only by run
  int init(int, char*[]);

public:
  Application();

  template<class T,
           typename... Args,
           typename std::enable_if<
             std::is_base_of<Component, T>::value>::type* = nullptr>
  void add_component(const std::string& name, const Args&&... args)
  {
    auto t = new T(m_app, std::forward<Args>(args)...);
    m_components.push_back(
      std::make_unique<T>(m_app, std::forward<Args>(args)...));
  }

  virtual int main();

  // public because its called by our main
  int run(int, char*[]);
};

}
}

#define MAIN(App)                                                              \
  int main(int argc, char* argv[])                                             \
  {                                                                            \
    App app;                                                                   \
    return app.run(argc, argv);                                                \
  }

#endif
