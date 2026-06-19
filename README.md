# Moirai

Real-time expected-path computation for transportation networks. Moirai consumes
package load events from Kafka, finds optimal forward and reverse paths through a
schedule-aware facility/route graph, and publishes results to OpenSearch for
operational visibility and downstream consumption.

## Features

- **Schedule-aware pathfinding** -- Dijkstra-based solver accounts for route
  schedules, facility processing latencies, cutoff times, and day-of-week masks.
- **Dual-path output** -- computes the earliest feasible forward path and the
  latest reverse path that still meets the promised delivery date (PDD).
- **Profile-Guided Optimization** -- first-class CMake integration for
  PGO-instrumented builds trained against production Kafka traffic.
- **DWH audit stream** -- append-only record trail via local JSONL files or
  Kafka topic, with Spark-based Parquet export to S3.
- **Adaptive bulk indexing** -- OpenSearch writer with gzip compression,
  latency-driven batch sizing, retry logic, and shard health monitoring.
- **C++26 modules** -- first-party code uses `import std` and named modules for
  fast incremental builds and strong encapsulation.

## Requirements

| Dependency | Minimum version |
| --- | --- |
| CMake | 4.3 |
| Ninja | any |
| GCC | 15 (16+ recommended) |
| libstdc++ | matching GCC, with `import std` module source |
| libcurl | any |
| OpenSSL | 1.1+ |
| zlib | any |
| librdkafka | any C++ API build |
| simdjson | 4.6.4 (fetched automatically) |
| mold linker | any (GCC Release builds) |

Clang 18.1.2+ with libstdc++ is also supported but GCC is the primary target.

## Quick Start

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Run in local file mode for a quick smoke test:

```sh
./build/moirai \
  --query-from tests/fixtures/load_normal.json \
  --route-api http://route-api/routes \
  --route-token "$TOKEN" \
  --facility-api http://facility-api/facilities \
  --facility-token "$TOKEN" \
  --facility-timings tests/fixtures/timings.json \
  --search-uri http://localhost:9200 \
  --search-user admin \
  --search-pass admin \
  --search-index moirai-dev
```

See [docs/user.md](docs/user.md) for Kafka mode, systemd deployment, DWH
configuration, and PGO training. See [docs/developer.md](docs/developer.md) for
build system details, architecture, and contribution workflow.

## Architecture

```
Kafka (loads) ──> KafkaReader ──> SolverWrapper ──> SearchWriter ──> OpenSearch
                                       │                  │
                                   Route API          DWH Audit
                                  Facility API     (JSONL / Kafka)
```

Moirai runs a fixed thread pipeline:

1. **Reader thread** -- `KafkaReader` (or `FileReader`) polls batches of load
   payloads into a bounded queue.
2. **Solver threads** (N = hardware_concurrency - 2) -- each dequeues loads,
   resolves forward/reverse paths through the transportation graph, and enqueues
   `SearchDocument` results.
3. **Writer threads** (configurable) -- bulk-index documents into OpenSearch and
   optionally emit DWH audit records.

The solver builds a CSR (compressed sparse row) graph at startup from route and
facility API responses, then performs schedule-aware Dijkstra traversals per load.
A configurable path cache avoids redundant computations for repeated
origin/destination/time combinations.

## Project Layout

```
modules/          C++26 module interface units (export module moirai.*)
src/              Module implementation files
include/          Third-party textual headers (blocking_queue, nlohmann/json)
tests/            Test sources and fixtures
benchmarks/       Solver performance benchmarks (also PGO training workload)
tools/            Verification, fixture capture, and export scripts
docs/             Documentation
cmake/            CMake helpers (simdjson patch, RdKafka find module)
```

## Documentation

- [User & Operator Guide](docs/user.md) -- installation, deployment,
  configuration reference, PGO training, DWH setup.
- [Developer Guide](docs/developer.md) -- building, architecture, testing,
  module conventions, contribution workflow.
- [Runtime Invocation](docs/runtime.md) -- CLI flag reference with examples.
- [C++ Module Boundary](docs/modules.md) -- module vs textual-include policy.
- [DWH Kafka Schema](docs/dwh-kafka-schema.md) -- audit topic field reference
  and schema registry payload.

## License

Proprietary.
