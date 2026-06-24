# User & Operator Guide

This document covers installing, deploying, and operating Moirai as a service or
using it as a standalone binary for local path computation.

## Installation

### Build from source

```sh
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DMOIRAI_SOLVER_QUEUE=binary \
  -DMOIRAI_SIMDJSON_PROVIDER=fetch
cmake --build build -j$(nproc)
```

The resulting binary is `build/moirai`.

### System dependencies

Install with your distribution's package manager:

- `gcc` 16+ with matching `libstdc++` (module source required for `import std`)
- `cmake` 4.3+
- `ninja-build`
- `libcurl-devel`
- `openssl-devel`
- `zlib-devel`
- `librdkafka-devel` (C++ headers required)
- `mold` (linker, used automatically for GCC Release builds)

simdjson 4.6.4 is fetched and compiled automatically unless
`-DMOIRAI_SIMDJSON_PROVIDER=system` is set.

---

## Running Moirai

### Kafka Mode (production)

Moirai consumes package load events from Kafka and writes expected-path results
to OpenSearch in real time.

```sh
moirai \
  --kafka-broker broker1:9092 \
  --kafka-broker broker2:9092 \
  --package-topic loads.expected-path \
  --batch-timeout 500 \
  --batch-size 2048 \
  --route-api https://route-api.internal/routes \
  --route-token "$ROUTE_TOKEN" \
  --facility-api https://facility-api.internal/facilities \
  --facility-token "$FACILITY_TOKEN" \
  --facility-timings /etc/moirai/timings.json \
  --search-uri https://opensearch.internal:9200 \
  --search-user moirai \
  --search-pass "$SEARCH_PASS" \
  --search-index moirai \
  --search-writers 2 \
  --kafka-config group.id=moirai \
  --kafka-config auto.offset.reset=latest \
  --kafka-config security.protocol=SASL_SSL \
  --kafka-config sasl.mechanisms=SCRAM-SHA-512 \
  --kafka-config sasl.username="$KAFKA_USER" \
  --kafka-config sasl.password="$KAFKA_PASS"
```

Repeat `--kafka-broker` for each broker. Repeat `--kafka-config` for each
librdkafka property.

### Local File Mode

For development or one-off path computation:

```sh
moirai \
  --query-from ./loads.json \
  --route-api https://route-api.internal/routes \
  --route-token "$ROUTE_TOKEN" \
  --facility-api https://facility-api.internal/facilities \
  --facility-token "$FACILITY_TOKEN" \
  --facility-timings ./timings.json \
  --search-uri http://localhost:9200 \
  --search-user admin \
  --search-pass admin \
  --search-index moirai-dev
```

`--query-from` enables a file-based reader. Kafka options are not required in
this mode.

---

## CLI Reference

| Flag | Short | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `--route-api` | `-a` | yes | -- | Route API endpoint URI |
| `--facility-timings` | `-b` | yes | -- | Path to facility timings JSON |
| `--facility-api` | `-c` | yes | -- | Facility API endpoint URI |
| `--route-token` | `-e` | yes | -- | Route API auth token |
| `--facility-token` | `-l` | yes | -- | Facility API auth token |
| `--search-uri` | `-s` | yes | -- | OpenSearch endpoint URI |
| `--search-user` | `-u` | yes | -- | OpenSearch username |
| `--search-pass` | `-w` | yes | -- | OpenSearch password |
| `--search-index` | `-i` | yes | -- | OpenSearch index name |
| `--kafka-broker` | `-k` | Kafka mode | -- | Kafka broker (repeatable) |
| `--package-topic` | `-p` | Kafka mode | -- | Load/package Kafka topic |
| `--kafka-config` | `-m` | no | -- | librdkafka property `key=value` (repeatable) |
| `--batch-timeout` | `-t` | no | 1000 | Kafka poll timeout in ms |
| `--batch-size` | `-z` | no | 100 | Kafka batch size |
| `--search-writers` | -- | no | 1 | Number of writer threads |
| `--query-from` | `-q` | no | -- | Local JSON file path (enables file mode) |
| `--help` | `-h` | -- | -- | Print usage and exit |

---

## systemd Deployment

Copy the service unit and create a config file:

```sh
cp moirai.service ~/.config/systemd/user/
mkdir -p ~/.config/moirai
cp moirai.conf ~/.config/moirai/moirai.conf
# Edit moirai.conf with your environment values
systemctl --user daemon-reload
systemctl --user enable --now moirai.service
```

The service restarts automatically on failure with a 5-second delay and is
limited to 1 day of runtime before automatic restart (to pick up rotated
credentials or accumulated memory).

### Retention cleanup

```sh
cp moirai-retention.service moirai-retention.timer ~/.config/systemd/user/
systemctl --user enable --now moirai-retention.timer
```

The retention service uses `curl` to delete OpenSearch indices older than
`SEARCH_RETENTION_DAYS`.

---

## Configuration Reference

All runtime behavior is controlled via CLI flags and environment variables.
There is no config file parser in the binary -- systemd's `EnvironmentFile`
provides the variable-to-CLI mapping.

### Environment Variables

#### Solver / Threading

| Variable | Default | Description |
| --- | --- | --- |
| `MOIRAI_ROUTE_EXPANSION_THREADS` | `nproc` | Threads for route spec expansion at startup |
| `MOIRAI_PATH_CACHE_ENABLED` | `true` | Enable path result caching |
| `MOIRAI_PATH_CACHE_MAX_ENTRIES` | `65536` | Maximum cached paths |
| `MOIRAI_PATH_CACHE_BUCKET_MINUTES` | `1` | Cache time-bucket granularity (minutes) |

Solver thread count is not configurable -- it is always
`max(1, hardware_concurrency - 2)`.

#### OpenSearch Writer

| Variable | Default | Description |
| --- | --- | --- |
| `SEARCH_BULK_MAX_RECORDS` | `1024` | Max records per bulk request |
| `SEARCH_BULK_MAX_BYTES` | `8388608` | Max bytes (8 MB) per bulk request |
| `SEARCH_BULK_MAX_RETRIES` | `3` | Retry count for transient errors |
| `SEARCH_BULK_MIN_RECORDS` | `512` | Min records before flushing |
| `SEARCH_BULK_TARGET_LATENCY_MS` | `5000` | Target latency for adaptive mode |
| `SEARCH_BULK_ADAPTIVE` | `false` | Enable latency-driven batch sizing |
| `SEARCH_BULK_GZIP` | `true` | Gzip compress bulk payloads |
| `SEARCH_BULK_GZIP_LEVEL` | `6` | Compression level (0-9) |
| `SEARCH_SHARDS` | `24` | Index shard count (at creation) |
| `SEARCH_REPLICAS` | `1` | Index replica count (at creation) |
| `SEARCH_REFRESH_INTERVAL` | `30s` | Index refresh interval |
| `SEARCH_LOG_SAMPLE_IDS` | `5` | IDs to log per metrics interval |
| `SEARCH_METRICS_INTERVAL_SECONDS` | `30` | Metrics log interval |
| `SEARCH_SHARD_WARN_RATIO` | `1.5` | Shard imbalance warning threshold |
| `SEARCH_SHARD_CRITICAL_RATIO` | `2.0` | Shard imbalance critical threshold |

#### DWH Audit (File)

| Variable | Default | Description |
| --- | --- | --- |
| `DWH_AUDIT_ENABLED` | `false` | Enable local JSONL audit files |
| `DWH_AUDIT_DIR` | `./audit` | Directory for audit files |
| `DWH_AUDIT_ROTATE_RECORDS` | `100000` | Records before file rotation |
| `DWH_AUDIT_ROTATE_BYTES` | `134217728` | Bytes (128 MB) before rotation |
| `DWH_AUDIT_ROTATE_SECONDS` | `300` | Seconds before rotation |

#### DWH Audit (Kafka)

| Variable | Default | Description |
| --- | --- | --- |
| `DWH_AUDIT_KAFKA_ENABLED` | `false` | Enable Kafka audit publishing |
| `DWH_AUDIT_KAFKA_TOPIC` | -- | Target Kafka topic |
| `DWH_AUDIT_KAFKA_BROKERS` | consumer brokers | Kafka brokers for audit producer |
| `DWH_AUDIT_KAFKA_REQUIRED` | `true` | Fail writer on Kafka errors |
| `DWH_AUDIT_KAFKA_FLUSH_TIMEOUT_MS` | `30000` | Producer flush timeout |
| `DWH_AUDIT_KAFKA_QUEUE_RETRIES` | `3` | Enqueue retry attempts |
| `DWH_AUDIT_KAFKA_CONFIG` | -- | Extra producer properties (`key=val,...`) |
| `DWH_AUDIT_KAFKA_SECURITY_PROTOCOL` | inherited | Override security protocol |
| `DWH_AUDIT_KAFKA_SASL_MECHANISMS` | inherited | Override SASL mechanism |
| `DWH_AUDIT_KAFKA_SASL_USERNAME` | inherited | Override SASL username |
| `DWH_AUDIT_KAFKA_SASL_PASSWORD` | inherited | Override SASL password |

#### DWH Export (Parquet)

| Variable | Default | Description |
| --- | --- | --- |
| `DWH_BUCKET` | -- | S3 bucket for Parquet upload |
| `DWH_PREFIX` | `expectedpath` | S3 key prefix |
| `DWH_EXPORT_WORK_DIR` | -- | Local working directory for export |
| `DWH_PARQUET_COALESCE` | `50` | Number of output Parquet files |
| `DWH_PROCESSING_STALE_SECONDS` | `1800` | Seconds before retrying stale files |
| `DWH_EXPORT_MAX_FILES_PER_RUN` | `1000` | Max files per export invocation |
| `DWH_PROCESSED_RETENTION_SECONDS` | `0` | Retention for processed JSONL (0=delete, -1=keep) |
| `DWH_ALLOW_UNSUPPORTED_JAVA` | `false` | Allow Java 25+ with Spark 4.2 |

---

## OpenSearch Index

Moirai creates the target index on first startup when it does not exist. The
mapping is fully explicit with dynamic field creation disabled.

Key mapping properties:
- Top-level identifiers (`waybill`, `package`, `cs_slid`, etc.) are `keyword`.
- Timestamps (`pdd`, `arrival`, `departure`) are `date` fields accepting
  `MM/dd/yy HH:mm:ss` format.
- Epoch-second counters (`pdd_ts`, `arrival_ts`, `departure_ts`, `updated_at_ts`)
  are `long`.
- `updated_at` is ISO-8601 `date`.
- Full path arrays (`earliest.locations`, `ultimate.locations`) are stored in
  `_source` only (not indexed).
- `first` and `second` path locations are explicitly indexed for filtering.
- Summary fields (`hop_count`, `location_codes`, `route_codes`) enable coarse
  path filtering.
- In the DWH Kafka audit stream only, `earliest.locations` and
  `ultimate.locations` are emitted as JSON array strings so the DWH connector
  does not ingest arrays of objects.

If upgrading from an older dynamically-mapped index, recreate the index or
reindex into a new index with the explicit mapping.

---

## Profile-Guided Optimization (PGO)

PGO produces measurably faster builds by training the compiler with production
workload profiles.

### Step 1: Generate profiles

```sh
cmake -B build-pgo-gen -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DMOIRAI_PGO_MODE=generate \
  -DMOIRAI_PGO_DIR=/path/to/pgo-profiles
cmake --build build-pgo-gen -j$(nproc)
```

Run the PGO-instrumented binary against production Kafka traffic for 30-45
minutes. The systemd service handles this:

```sh
# Generate a PGO training config from production config
PGO_RUN_ID=$(date -u +%Y%m%d%H%M%S)
grep -Ev '^(MOIRAI_ROUTE_EXPANSION_THREADS|MOIRAI_BINARY|KAFKA_GROUP_ID|KAFKA_AUTO_OFFSET_RESET|SEARCH_INDEX|DWH_AUDIT_ENABLED|DWH_AUDIT_DIR)=' \
  ~/.config/moirai/moirai.conf > ~/.config/moirai/moirai-pgo-train.conf
printf 'MOIRAI_ROUTE_EXPANSION_THREADS=1\nMOIRAI_BINARY=%s/moirai/build-pgo-gen/moirai\nKAFKA_GROUP_ID=MOIRAI_PGO_TRAIN_%s\nKAFKA_AUTO_OFFSET_RESET=earliest\nSEARCH_INDEX=moirai.pgo.train.%s\nDWH_AUDIT_ENABLED=true\nDWH_AUDIT_DIR=/var/log/expath/pgo-audit\n' \
  "$HOME" "$PGO_RUN_ID" "$PGO_RUN_ID" >> ~/.config/moirai/moirai-pgo-train.conf

cp moirai-pgo-train.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user start moirai-pgo-train.service
```

The service runs for up to 45 minutes then stops. Profile data accumulates in
`MOIRAI_PGO_DIR`.

### Step 2: Build with profiles

```sh
cmake -B build-pgo-use -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DMOIRAI_PGO_MODE=use \
  -DMOIRAI_PGO_DIR=/path/to/pgo-profiles
cmake --build build-pgo-use -j$(nproc)
```

Deploy `build-pgo-use/moirai` as the production binary.

### Offline PGO (benchmark-only)

When production Kafka access is unavailable, use the solver benchmark as a
training workload:

```sh
cmake -B build-pgo-gen -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DMOIRAI_PGO_MODE=generate
cmake --build build-pgo-gen -j$(nproc) moirai_solver_benchmarks
./build-pgo-gen/moirai_solver_benchmarks

cmake -B build-pgo-use -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DMOIRAI_PGO_MODE=use \
  -DMOIRAI_PGO_DIR=build-pgo-gen/pgo
cmake --build build-pgo-use -j$(nproc)
```

### Tips

- Keep profile data on an attached data volume, not the root volume.
- Use a separate Kafka consumer group for PGO training to avoid disturbing
  production offsets.
- Use a separate OpenSearch index for PGO writes.
- The `moirai_pgo_train` CMake target runs the solver benchmark in one step:
  `ninja -C build-pgo-gen moirai_pgo_train`.

---

## DWH Audit Setup

The DWH audit produces an append-only stream of every expected-path result.
OpenSearch upserts by waybill (latest-wins), while the DWH audit preserves
every historical record.

### Kafka audit (preferred)

Set `DWH_AUDIT_KAFKA_ENABLED=true` and configure the topic. The Kafka audit
sink publishes JSON records keyed by waybill to an append-retaining topic.

Do not enable compaction on the audit topic if DWH needs every historical
record.

### File audit with Parquet export

Set `DWH_AUDIT_ENABLED=true`. Moirai writes `*.jsonl.open` files and
atomically renames them to `*.jsonl` on rotation. The export timer converts
closed files to Parquet and uploads to S3.

```sh
python3 -m venv ~/.local/share/moirai-exporter-venv
~/.local/share/moirai-exporter-venv/bin/python -m pip install --upgrade pip boto3 pyspark
cp moirai-dwh-export.service moirai-dwh-export.timer ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now moirai-dwh-export.timer
```

Once Kafka audit is confirmed as the durable handoff, disable the file audit
path and remove the export timer.

---

## Build Options

| CMake Variable | Values | Default | Description |
| --- | --- | --- | --- |
| `MOIRAI_PGO_MODE` | `""`, `generate`, `use` | `""` | PGO instrumentation mode |
| `MOIRAI_PGO_DIR` | path | `${BUILD}/pgo` | Profile data directory |
| `MOIRAI_SOLVER_QUEUE` | `binary`, `bucket` | `binary` | Solver priority queue implementation |
| `MOIRAI_SIMDJSON_PROVIDER` | `fetch`, `system` | `fetch` | simdjson source |
| `MOIRAI_ENABLE_GCC_LTO` | `ON`, `OFF` | `OFF` | GCC LTO (experimental with modules) |
| `MOIRAI_BUILD_APP` | `ON`, `OFF` | `ON` | Build main executable |
| `ANALYZE` | `ON`, `OFF` | `OFF` | Enable iwyu + clang-tidy |

---

## Monitoring

Moirai logs to stderr/journal at three levels: DEBUG, INFORMATION, ERROR.

The search writer emits metrics every `SEARCH_METRICS_INTERVAL_SECONDS`
including:
- Records indexed / failed per interval
- Bytes written
- Bulk request latency (p50, p99)
- Shard size distribution and imbalance alerts

Shard imbalance warnings fire at `SEARCH_SHARD_WARN_RATIO` (1.5x) and errors
at `SEARCH_SHARD_CRITICAL_RATIO` (2.0x) of median shard size.
