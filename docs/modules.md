# C++ Module Boundary

This project builds first-party code as C++26 modules with `import std`.
The supported compiler baseline is GCC 16 or newer with libstdc++'s standard
library module source available. Other compilers are intentionally rejected by
CMake until their `import std` support is compatible with this build.

First-party implementation and test files should prefer `import std` and
project modules over textual standard-library includes.

Textual includes remain acceptable for libraries that are not module-ready in
this build, especially `blocking_queue.hxx`, `blocking_queue_fwd.hxx`,
`nlohmann/json.hpp`, `nlohmann/json_fwd.hpp`, `curl/curl.h`, `getopt.h`, and
`librdkafka/rdkafkacpp.h`. The `BlockingQueue` headers intentionally remain
textual because exporting the `concurrentqueue` internals through GCC module
BMIs has been unstable.

Use `tools/verify.sh` for local pre-commit verification. It recreates `build/`,
runs the full CTest suite including benchmark regression guards, checks
whitespace with `git diff --check`, and scans first-party code for removed
Boost graph/header-era symbols.
