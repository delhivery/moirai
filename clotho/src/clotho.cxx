namespace ambasta {
class ConfigurableComponent
{
public:
  virtual void defineOptions() = 0;
};

class Application : public ConfigurableComponent
{
public:
  void addComponent(ConfigurableComponent&);

  virtual int run(int argc, char* argv[]) = 0;
};
}

class Clotho : public ambasta::Application
{
public:
  void defineOptions() {}
  int run(int argc, char* argv[]) { return 0; }
};

#define MAIN(App)                                                              \
  int main(int argc, char* argv[])                                             \
  {                                                                            \
    App app;                                                                   \
    return app.run(argc, argv);                                                \
  }

MAIN(Clotho);
