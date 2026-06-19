# Developer Guide

This document covers building, testing, and contributing to the Moirai codebase.

## Prerequisites

- GCC 15+ (16+ recommended for full C++26 module stability)
- CMake 4.3+
- Ninja build system
- mold linker
- System libraries: libcurl, OpenSSL, zlib, librdkafka (C++ API)
- Python 3 (for simdjson patching on GCC 16+)

## Building

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Ninja is mandatory -- CMake will reject other generators because C++26 module
dependency scanning requires Ninja.

### Debug builds

```sh
cmake -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j$(nproc)
```

Sanitizers are available as commented blocks in `CMakeLists.txt`. Uncomment the
desired sanitizer (ASAN, TSAN, MSAN, UBSAN) for debug builds.

### Dual-compiler verification

```sh
VERIFY_COMPILER=both tools/verify.sh
```

This rebuilds from scratch under both GCC and Clang, runs the full test suite,
checks for compiler warnings, trailing whitespace, and stale references to
removed abstractions.

---

## Architecture

### Module Map

Moirai is structured as a set of C++26 named modules under the `moirai`
namespace partition:

| Module | Responsibility |
| --- | --- |
| `moirai.app` | Logger, Application lifecycle, signal handling |
| `moirai.date_utils` | Time/date conversion utilities |
| `moirai.json_utils` | simdjson DOM wrapper with thread-local parser pool |
| `moirai.transportation` | TransportCenter (node) and TransportEdge data types |
| `moirai.route_schedule` | Route schedule specification parsing |
| `moirai.solver` | Pathfinding algorithm and CSR graph representation |
| `moirai.search_document` | SearchDocument output schema |
| `moirai.processor` | Load payload processing utilities |
| `moirai.utils` | ScanReader base, TopicMap, blocking queue utilities |
| `moirai.http` | CURL-based HTTP client |
| `moirai.scan_reader` | Abstract reader interface |
| `moirai.file_reader` | Local file reader implementation |
| `moirai.kafka_reader` | Kafka consumer implementation |
| `moirai.search_writer` | OpenSearch bulk writer + DWH audit sinks |
| `moirai.solver_wrapper` | Solver orchestration, caching, API initialization |
| `moirai.server` | CLI parsing, thread pipeline setup, main entry |

### Source Layout

```
modules/moirai.*.cxx    Module interface units (export module declarations)
src/*.cxx               Module implementation units (module :private or impl)
src/main.cxx            Binary entry point
include/                Non-module third-party headers
tests/                  Test executables and fixtures
benchmarks/             Performance benchmarks
```

### Threading Model

The application runs a fixed pipeline of `std::jthread` workers communicating
via bounded `BlockingQueue` channels:

```
Reader (1 thread)
    │
    ▼ load_queue (cap 4096)
    │
Solver (N threads, default nproc-2)
    │
    ▼ solution_queue (cap 4096)
    │
Writer (M threads, default 1)
```

Shutdown is cooperative via `std::stop_token`. The reader thread closes
downstream queues when Kafka returns EOF or a signal arrives. Solver threads
decrement a shared counter; the last solver closes the solution queue.

### Key Data Flow

1. `KafkaReader` polls batches of JSON load payloads from Kafka.
2. `SolverWrapper` parses each load, looks up the origin facility in the
   transportation graph, and runs the solver twice (forward for `earliest`,
   reverse for `ultimate`).
3. The solver uses a binary heap priority queue over a CSR graph with
   schedule-aware edge weights (departure time + transit + processing latency).
4. Results are packaged as `SearchDocument` and enqueued.
5. `SearchWriter` batches documents into OpenSearch bulk requests, optionally
   compresses with gzip, and writes with retry logic.
6. If DWH audit is enabled, each document is also written to a JSONL file
   and/or published to a Kafka audit topic.

### Graph Initialization

At startup, `SolverWrapper`:
1. Fetches all routes from the route API (paginated).
2. Fetches all facilities from the facility API (paginated).
3. Loads facility timing definitions (latencies, cutoffs) from a JSON file.
4. Builds the CSR graph with TransportCenter nodes and TransportEdge edges.

---

## Module Conventions

- First-party code uses `import std` and named project modules.
- Textual `#include` is acceptable only for non-module-ready libraries:
  `blocking_queue.hxx`, `nlohmann/json.hpp`, `curl/curl.h`, `getopt.h`,
  `librdkafka/rdkafkacpp.h`.
- The `BlockingQueue` headers stay textual because exporting `concurrentqueue`
  internals through GCC module BMIs has been unstable.
- GCC 16+ requires a patched simdjson header; the build handles this
  automatically via `cmake/patch_simdjson.py`.

---

## Testing

### Running tests

```sh
ctest --test-dir build --output-on-failure -j$(nproc)
```

### Test executables

| Target | Tests |
| --- | --- |
| `moirai_schedule_tests` | Route schedule parsing and expansion |
| `moirai_solver_tests` | Pathfinding correctness |
| `moirai_processor_tests` | Load payload processing |
| `moirai_logger_tests` | Logger level and sink behavior |
| `moirai_wrapper_tests` | Solver wrapper integration |
| `moirai_init_tests` | Graph initialization from fixtures |
| `moirai_run_tests` | End-to-end pipeline with env overrides |
| `moirai_search_writer_tests` | Bulk indexing logic |
| `moirai_module_smoke` | Module import sanity check |
| `moirai_solver_benchmarks` | Performance regression guard |

CLI tests verify help output, required option validation, and argument parsing.

### Test framework

Tests use a minimal custom framework (`tests/test_helpers.hxx`) with:
- `expect_eq()`, `expect_true()` -- assertion macros with source location
- `ScopedLogCapture` -- redirects logger output for validation
- `FakeHttp` -- mock HTTP client returning fixture responses
- `WrapperHarness` -- sets up queue infrastructure for integration tests
- `fixture_path()`, `read_fixture()` -- load JSON from `tests/fixtures/`
- `ScopedEnv` -- temporarily override environment variables

### Writing tests

1. Create `tests/<name>_tests.cxx`.
2. Add an executable target in `CMakeLists.txt` using `moirai_configure_test()`.
3. Import the module under test and use test helpers.
4. Add fixture files to `tests/fixtures/` as needed.

---

## Benchmarks

The solver benchmark (`benchmarks/solver_benchmarks.cxx`) exercises the
pathfinding hot path with realistic graph sizes. It serves dual purpose:

1. **Regression guard** -- runs as part of `ctest` with timing assertions.
2. **PGO training workload** -- the `moirai_pgo_train` CMake target runs it
   directly for profile generation.

---

## Build Configuration

### CMake options

| Variable | Values | Default | Purpose |
| --- | --- | --- | --- |
| `MOIRAI_PGO_MODE` | `""` / `generate` / `use` | `""` | PGO mode |
| `MOIRAI_PGO_DIR` | path | `${BUILD}/pgo` | Profile output/input dir |
| `MOIRAI_SOLVER_QUEUE` | `binary` / `bucket` | `binary` | Priority queue impl |
| `MOIRAI_SIMDJSON_PROVIDER` | `fetch` / `system` | `fetch` | simdjson source |
| `MOIRAI_ENABLE_GCC_LTO` | `ON` / `OFF` | `OFF` | GCC LTO (unstable) |
| `MOIRAI_BUILD_APP` | `ON` / `OFF` | `ON` | Build binary target |
| `ANALYZE` | `ON` / `OFF` | `OFF` | iwyu + clang-tidy |
| `BUILD_TESTING` | `ON` / `OFF` | `ON` | Build test targets |

### Compiler flags (Release)

GCC:
```
-O3 -fgraphite-identity -floop-nest-optimize -fno-semantic-interposition -fno-plt
-fuse-ld=mold
-Wall -Wextra -Wpedantic -Werror
```

LTO (`-fwhole-program -flto=auto`) is available via `MOIRAI_ENABLE_GCC_LTO=ON`
but disabled by default due to GCC module BMI instability.

---

## Verification Workflow

Before pushing, run:

```sh
tools/verify.sh
```

This script:
1. Removes and recreates the build directory.
2. Configures with the binary solver queue.
3. Builds with full warnings (`-Wall -Wextra -Wpedantic -Werror`).
4. Runs the complete CTest suite.
5. Fails on any compiler warning.
6. Runs `git diff --check` for whitespace errors.
7. Scans for stale Boost/header-era symbols in first-party code.

Set `VERIFY_COMPILER=gcc`, `clang`, or `both` to control which toolchains are
tested. Set `BUILD_JOBS` to override parallelism.

---

## Code Style

- C++26 standard, using `import std` for standard library access.
- `-Werror` enforced -- all warnings are build failures.
- No Boost dependencies (fully removed; verify.sh scans for stale references).
- Prefer value types and move semantics over heap allocation.
- Thread safety via `std::jthread` + `std::stop_token` and bounded queues.
- Exceptions are caught at thread boundaries and trigger graceful shutdown.

---

## Tools

| Script | Purpose |
| --- | --- |
| `tools/verify.sh` | Full build + test verification |
| `tools/capture_load_fixture.py` | Capture Kafka load payloads as test fixtures |
| `tools/fetch_route_fixture.sh` | Download route API responses as fixtures |
| `tools/fetch_facility_fixture.sh` | Download facility API responses as fixtures |
| `tools/export_audit_to_parquet.py` | DWH JSONL-to-Parquet export (Spark) |
| `tools/assess_instance_sizing.py` | Analyze solver performance for sizing |
| `tools/results_to_csv.py` | Convert search results to CSV |
| `cmake/patch_simdjson.py` | Patch simdjson.h for GCC 16+ modules |

---

## Adding a New Module

1. Create `modules/moirai.<name>.cxx` with the module interface:
   ```cpp
   export module moirai.<name>;
   import std;
   // exports...
   ```
2. Create `src/<name>.cxx` with the implementation (if needed).
3. Add both files to `MOIRAI_MODULE_SOURCES` and `MOIRAI_IMPLEMENTATION_SOURCES`
   in `CMakeLists.txt`.
4. Other modules can then `import moirai.<name>;`.

---

## Dependency Management

- **simdjson**: Pinned at 4.6.4 via FetchContent. The build verifies
  header/library version consistency at configure time.
- **librdkafka**: Found via CMake package or pkg-config fallback.
- **CURL, OpenSSL, zlib**: System packages via `find_package()`.
- **nlohmann/json**: Vendored header in `include/nlohmann/`.
- **moodycamel::ConcurrentQueue**: Vendored headers in `include/cameron314/`.

---

## CI

GitHub Actions runs CodeQL analysis on push to main and weekly:
- C++ static analysis
- JavaScript scanning (if any tooling scripts)
- Python scanning (export/fixture tools)
