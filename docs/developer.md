# Developer Guide

This document covers building, testing, and contributing to the Moirai codebase.

## Prerequisites

- GCC 16+ with libstdc++ `import std` module source (GCC 15 passes the CMake
  check but GCC 16 is the supported baseline per `docs/modules.md`)
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
Solver (N threads, N = max(1, hardware_concurrency - 2))
    │
    ▼ solution_queue (cap 4096)
    │
Writer (M threads, default 1, configurable via --search-writers)
```

Shutdown is cooperative via `std::stop_token`. The reader thread closes
downstream queues when Kafka returns EOF or a signal arrives. Solver threads
decrement a shared counter; the last solver closes the solution queue.

### Key Data Flow

1. `KafkaReader` polls batches of JSON load payloads from Kafka.
2. `SolverWrapper` parses each load, looks up the origin and destination
   facilities in the transportation graph, and runs the solver:
   - Forward from origin at current time → `earliest` path (earliest arrival).
   - If not critical: reverse from destination at PDD → `ultimate` path (latest
     feasible departure).
3. The solver uses a priority queue over a CSR graph. Each edge carries a
   precomputed weekly schedule and duration that accounts for source outbound
   latency, transit time, and target inbound latency.
4. Results are packaged as `SearchDocument` and enqueued.
5. `SearchWriter` batches documents into OpenSearch bulk requests, optionally
   compresses with gzip, and writes with retry logic.
6. If DWH audit is enabled, each document is also written to a JSONL file
   and/or published to a Kafka audit topic.

### Graph Initialization

At startup, the primary `SolverWrapper` builds the graph:

1. Loads facility timing definitions (latencies and cutoffs) from a local JSON
   file into a lookup map.
2. Fetches all facilities from the facility API (paginated via HTTP). For each
   facility, creates a `TransportCenter` node with latencies from the timings
   map and a default cutoff. Also builds facility co-location groups by
   `property_id` for custody edges.
3. Adds transient custody edges between co-located facilities (same property).
4. Fetches all route specifications from the route API (single HTTP call).
   Expands route specs into individual `TransportEdge` objects using
   `MOIRAI_ROUTE_EXPANSION_THREADS` parallel threads, then inserts each edge
   into the solver graph.
5. Calls `finalize_graph()` to trigger CSR construction.

Secondary solver threads share the same `Solver` instance (read-only after
initialization) and the same path cache.

---

## CSR Graph Representation

The solver stores the transportation network as a Compressed Sparse Row (CSR)
graph. This layout provides O(1) adjacency lookup per node and keeps edge data
cache-friendly for traversal.

### Storage Layout

```
m_nodes:            [TransportCenter...]        Dense node array indexed by NodeId
m_edges:            [SolverEdgeHot...]          Dense edge array indexed by EdgeId
m_edge_details:     [SolverEdgeCold...]         Cold edge data (full TransportEdge)
m_outgoing_offsets: [uint32...]                 Prefix-sum offsets (size = nodes+1)
m_outgoing_edges:   [EdgeId...]                 Edge ids sorted by source node
m_incoming_offsets: [uint32...]                 Prefix-sum offsets (size = nodes+1)
m_incoming_edges:   [EdgeId...]                 Edge ids sorted by target node
```

For a node `n`, its outgoing edges are the slice
`m_outgoing_edges[m_outgoing_offsets[n] .. m_outgoing_offsets[n+1]]`, and
similarly for incoming edges.

### Build Process

`rebuild_csr()` constructs the offset and edge-id arrays from the flat edge
list:

1. **Count pass** -- iterate all edges, incrementing `outgoing_offsets[source+1]`
   and `incoming_offsets[target+1]`.
2. **Prefix-sum** -- convert counts into cumulative offsets.
3. **Scatter pass** -- iterate edges again, placing each edge id at its
   computed position using a running cursor per node.

The rebuild is lazy (triggered by `m_csr_dirty` flag) and protected by a mutex
with double-checked locking. Any `add_node` or `add_edge` call sets the dirty
flag. Solver threads calling `outgoing_edges()` or `incoming_edges()` trigger
a rebuild if needed; once built, subsequent calls return a `std::span` slice
with no locking.

### Hot/Cold Edge Split

Edge data is split into two parallel arrays:

- **`SolverEdgeHot`** -- fields accessed on every traversal: source/target node
  ids, pre-computed weekly schedule arrays (up to 7 entries per direction),
  forward/reverse durations, reverse outbound latency, vehicle type, movement
  type. This struct contains only integers and fixed arrays -- no pointers, no
  strings, no heap allocations -- so sequential access during Dijkstra benefits
  from hardware prefetch.
- **`SolverEdgeCold`** -- the full `TransportEdge` struct with string fields
  (route code, name, prefix). Only accessed during path reconstruction, never
  during the main Dijkstra loop.

The `cold` field in `SolverEdgeHot` is an index into `m_edge_details` for
path-building.

### Node and Edge Lookup

Transparent hash maps (`m_node_by_name`, `m_edge_by_name`) provide O(1) lookup
by string code without allocating a `std::string` for each query (heterogeneous
lookup via `std::string_view`).

---

## Solver Algorithm

The pathfinding algorithm is a modified Dijkstra that operates in minutes since
the Unix epoch and accounts for schedule constraints (routes only depart on
specific days/times).

### Time Model

All solver-internal times are `SolverMinute` (uint32_t minutes since epoch).
This gives a range of ~8,171 years, far exceeding operational needs, while
keeping comparisons cheap (integer ops, no chrono overhead in the hot loop).

### Traversal Modes

The solver supports two traversal modes, both using the same `find_path_impl`
template:

- **FORWARD** -- finds the earliest arrival path from source to target, starting
  at a given time. Initial distance = MAX (unvisited). Relaxation condition:
  `next < current_best` (minimize time). Priority queue ordered as min-heap.
  Called as `find_path<FORWARD>(origin, destination, current_time)`.
- **REVERSE** -- finds the latest feasible departure from origin to destination
  that still arrives by a deadline. Initial distance = 0 (unvisited).
  Relaxation condition: `next > current_best` (maximize time). Priority queue
  ordered as max-heap. Called as `find_path<REVERSE>(destination, origin, pdd)`:
  the search starts at the destination with the deadline and walks backward
  through incoming edges to find the latest departure from the origin.

### Edge Traversal (Schedule Resolution)

The `traverse<P>()` function computes the time at the next node given the
current time and an edge's weekly schedule:

**Forward**: Given time `t` at the source node, find the next schedule entry on
or after `minute_of_week(t)` using `std::lower_bound`. The schedule entries
represent the minute-of-week when source-side processing must begin to catch
each departure (computed as `departure_time - source_offset`). The result is
`t + wait + forward_duration`, where `forward_duration` is the total edge
traversal time: `source_offset + transit + target_offset`. These offsets include
facility processing latencies and loading/unloading time. If no schedule entry
remains in the current week, the algorithm wraps to the first entry of the
following week.

**Reverse**: Given time `t` at the target node (a deadline), find the last
schedule entry on or before `minute_of_week(t)` using `std::upper_bound` then
stepping back. The schedule entries represent arrival-minute-of-week at the
target (computed as `departure_time + transit + target_offset`). The result is
`t - wait - reverse_duration`, where `reverse_duration = source_offset + transit
+ target_offset`. This gives the time at the source node when processing would
begin. Path reconstruction adds the edge's `reverse_outbound_latency` to
convert from processing-start time to a departure time at each step.

### Weekly Schedule Precomputation

When an edge is added, `TransportEdge::weight<FORWARD>()` and
`weight<REVERSE>()` each produce a `COST` struct containing a
`schedule_offset`, `duration`, and `days_of_week` bitmask. The schedule offset
is converted to up to 7 sorted minute-of-week values via `build_weekly_schedule`
(one per active day). These are stored in a fixed `std::array<SolverMinute, 7>`
with a count field. This avoids any allocation or branching during traversal --
the hot path is a binary search over at most 7 entries.

For the forward direction, `schedule_offset = departure - source_offset` (when
must processing begin at source to catch this departure).
For the reverse direction, `schedule_offset = departure + transit +
target_offset` (when does this trip arrive at the target).

The `source_offset` and `target_offset` used here incorporate facility
processing latencies (inbound or outbound, depending on movement type) plus
half the loading or unloading time (full unloading for terminal edges).

### Thread-Local Scratch Buffers

Each solver thread maintains a `thread_local SolverScratch` struct containing:

- `distances[N]` -- best-known distance to each node
- `predecessors[N]` -- edge id used to reach each node on the best path
- `generations[N]` -- generation counter to avoid clearing arrays between queries
- `heap` -- binary heap entries
- `path_nodes`, `path_edges` -- reused buffers for path reconstruction

The generation counter trick: instead of clearing `distances[]` (O(N)) between
queries, a per-query generation is incremented. A node's distance is valid only
if `generations[node] == current_generation`. This makes per-query setup O(1)
regardless of graph size.

### Vehicle Type Filtering

Edges are filtered by vehicle type during traversal. `VehicleType` is an enum
where `SURFACE < AIR`. Querying with `VehicleType::AIR` allows both surface and
air edges; querying with `VehicleType::SURFACE` restricts to surface-only. This
is checked per-edge via `edge.vehicle <= V`.

### Path Reconstruction

After Dijkstra terminates (target node popped from the queue), the path is
reconstructed by following `predecessors[]` from the solver's target back to
its source:

- **Forward**: Trace from solver-target to solver-source via `edge.source`,
  collecting nodes and edges. Reverse the collected list to get chronological
  order. Each step records the node, the outbound edge (from cold storage), and
  the node's arrival time from `distances[]`.
- **Reverse**: The solver is called with `find_path<REVERSE>(destination,
  origin, pdd)` -- so the solver-source is the shipment destination and the
  solver-target is the shipment origin. Trace from solver-target (shipment
  origin) toward solver-source (shipment destination) via `edge.target`. Each
  step records `distances[node] + edge.reverse_outbound_latency` as the
  reported time. The `reverse_outbound_latency` is the raw outbound processing
  latency at that node for the edge's movement type, converting the solver's
  internal "processing-start" time to when the package clears outbound
  processing. The final step (shipment destination) records `distances[node]`
  directly with no outbound edge. The resulting path is ordered from shipment
  origin to shipment destination.

---

## Priority Queues

The solver supports two priority queue implementations, selected at compile time
via `-DMOIRAI_SOLVER_QUEUE=binary|bucket`.

### Binary Heap (default, production)

A standard `std::vector`-backed binary heap using `std::push_heap` /
`std::pop_heap` with a custom comparator:

- Forward mode: min-heap (smallest distance first)
- Reverse mode: max-heap (largest distance first)

Stale entries (where `entry.distance != scratch.distance(entry.node)`) are
skipped on pop rather than decreased-key, making this a lazy-deletion Dijkstra.
This avoids the complexity of an indexed heap while being efficient for
transportation networks where the number of stale entries is small relative to
graph size.

Complexity: O((V + E) log V) per query.

### Bucket Queue (experimental)

A `std::map<SolverMinute, std::vector<NodeId>>` keyed by distance. Forward
mode uses `std::less` ordering (smallest bucket first); reverse mode uses
`std::greater` (largest bucket first).

Pop extracts from the back of the first bucket's vector. Empty buckets are
erased. This provides O(1) amortized pop when edge weights cluster into few
distinct values, but has higher constant factors than the binary heap due to
map node allocation.

The bucket queue is retained as an experimental option. Production deployments
should use the binary heap.

---

## BlockingQueue

The inter-thread communication channels use a custom `BlockingQueue<T>` that
wraps `moodycamel::ConcurrentQueue` with bounded capacity, blocking semantics,
and cooperative shutdown.

### Design

```cpp
BlockingQueue<T>(capacity)
```

- **Bounded MPMC** -- multiple producers and consumers, with a configurable
  capacity. Producers block when the queue is full; consumers block when empty.
- **Stop-token aware** -- all blocking operations accept `std::stop_token` and
  unblock when cancellation is requested.
- **Closeable** -- `close()` wakes all waiters and rejects further enqueues.
  Items enqueued concurrently with close are discarded.

### Operations

| Method | Behavior |
| --- | --- |
| `wait_enqueue(value, stop_token)` | Block until slot available or closed/stopped. Returns false on rejection. |
| `wait_dequeue_bulk(span, stop_token)` | Block until at least one item available. Dequeues up to `span.size()` items. Returns 0 on close. |
| `try_dequeue_bulk(items, count)` | Non-blocking bulk dequeue. Returns number dequeued (may be 0). |
| `close()` | Permanently shuts down the queue. Wakes all waiters. |
| `size_approx()` | Current logical size (items enqueued minus dequeued). |

### Synchronization

The queue combines lock-free enqueue/dequeue (from moodycamel) with a mutex +
condition variables for blocking and capacity tracking:

- `m_not_full` -- signaled when items are consumed, waking blocked producers.
- `m_not_empty` -- signaled when items are produced, waking blocked consumers.
- `m_size` -- logical count maintained under the mutex for capacity enforcement.
- `m_pending_pushes` -- tracks in-flight enqueues between reservation and
  completion to prevent over-admission.

This hybrid avoids spinning while still allowing the underlying lock-free queue
to handle the actual data movement without contention.

### Pipeline Queues

The application creates four queues:

| Queue | Type | Capacity | Purpose |
| --- | --- | --- | --- |
| `node_queue` | `std::string` (JSON) | 1 | Reserved for Kafka-driven facility updates (currently unused) |
| `edge_queue` | `std::string` (JSON) | 1 | Reserved for Kafka-driven route updates (currently unused) |
| `load_queue` | `std::string` (JSON) | 4096 | Streaming load payloads from reader to solvers |
| `solution_queue` | `SearchDocument` | 4096 | Streaming results from solvers to writers |

Graph initialization currently happens via direct HTTP calls in SolverWrapper
(`init_nodes`, `init_edges`) at startup, not through the node/edge queues.
The streaming queues use capacity 4096 to buffer bursts without blocking the
reader or solver threads.

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
