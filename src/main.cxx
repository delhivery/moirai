import std;
import moirai.server;

auto main(int argc, char** argv) -> int {
  try {
    Moirai app;
    return app.run(argc, argv);
  } catch (const std::exception& exc) {
    std::cerr << "Unhandled error: " << exc.what() << '\n';
    return 1;
  } catch (...) {
    std::cerr << "Unhandled error\n";
    return 1;
  }
}
