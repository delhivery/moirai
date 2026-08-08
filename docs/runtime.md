# Runtime Invocation

## Kafka mode

`moirai` requires route and facility API inputs, search output credentials, and
at least one Kafka broker plus a package/load topic.

```sh
moirai \
  --kafka-broker "${BROKER_URI_1}" \
  --package-topic "${LOAD_TOPIC}" \
  --batch-timeout "${BATCH_TIMEOUT_MS}" \
  --batch-size "${BATCH_SIZE}" \
  --route-api "${ROUTE_API_URI}" \
  --route-token "${ROUTE_API_TOKEN}" \
  --facility-api "${FACILITY_API_URI}" \
  --facility-token "${FACILITY_API_TOKEN}" \
  --facility-timings "${FACILITY_TIMINGS_FILE}" \
  --search-uri "${SEARCH_URI}" \
  --search-user "${SEARCH_USER}" \
  --search-pass "${SEARCH_PASS}" \
  --search-index "${SEARCH_INDEX}" \
  --kafka-config group.id="${KAFKA_GROUP_ID}" \
  --kafka-config auto.offset.reset="${KAFKA_AUTO_OFFSET_RESET}"
```

Repeat `--kafka-broker` for additional brokers and repeat `--kafka-config` for
SASL/MSK settings such as `security.protocol`, `sasl.mechanisms`,
`sasl.username`, and `sasl.password`.

## OpenSearch Mapping

The application creates `SEARCH_INDEX` on first startup when it does not already
exist. The generated mapping is explicit and disables dynamic field creation at
the root and inside path sections. Top-level identifiers are `keyword`.
Timestamp strings such as `pdd`, `arrival`, and `departure` are indexed as
OpenSearch `date` fields using the existing `MM/dd/yy HH:mm:ss` payload format.
Canonical timestamp counters such as `pdd_ts`, `arrival_ts`, and `departure_ts`
are `long`, and `updated_at` is an ISO-8601 `date`. Solver timestamps
(`pdd_ts`, `arrival_ts`, and `departure_ts`) and writer timestamps
(`updated_at_ts`) are epoch seconds.

Full path arrays (`earliest.locations` and `ultimate.locations`) are kept in
`_source` only with `enabled: false` to avoid indexing every hop in every path.
The lightweight `first` and `second` path objects remain explicitly indexed for
filtering and sorting. Each path section also indexes `hop_count`,
`location_codes`, and `route_codes` summaries for coarse filtering without
nested-document expansion. Criticality is exposed as the top-level
`is_critical` boolean.

The DWH Kafka audit stream uses the same path summaries, but emits
`earliest.locations` and `ultimate.locations` as JSON array strings so DWH
connectors that reject arrays of objects can still carry the full hop list.

OpenSearch does not allow changing an existing field type in place. If an older
index has dynamically inferred fields such as `earliest.first.arrival` as `text`,
create a replacement index with the new mapping and reindex, or delete/recreate
the index during a controlled maintenance window.

## Solver Queue

Production builds should use the default binary heap solver queue:
`-DMOIRAI_SOLVER_QUEUE=binary`. Existing CMake build directories keep cached
values, so pass this flag explicitly or remove old build directories before
rebuilding. The bucket queue is kept as an experimental option only.

## Capacity-Aware Routing

Capacity-aware online routing is disabled by default. When enabled, the solver
checks capacity on concrete trip instances, keyed internally by route edge and
selected departure epoch minute. Path cache lookups are bypassed for
capacity-aware searches because reservations change the feasible edge set in real
time.

| Environment variable | Default | Meaning |
| --- | --- | --- |
| `MOIRAI_CAPACITY_ENABLED` | `false` | Enable capacity checks during forward/reverse path searches |
| `MOIRAI_CAPACITY_MISSING_ALLOWED` | `true` | Treat missing capacity buckets as available; set `false` once all trip capacities are loaded |
| `MOIRAI_CAPACITY_RESERVE_EARLIEST` | `true` | Atomically reserve capacity for the emitted earliest path |
| `MOIRAI_CAPACITY_ALLOW_PROTECTED` | `false` | Allow online searches to consume protected forecast capacity |
| `MOIRAI_CAPACITY_SEARCH_WEEKS` | `1` | Number of weekly schedule cycles to scan beyond the current cycle for an available departure |
| `MOIRAI_CAPACITY_CANDIDATE_COUNT` | `1` | Number of candidate paths to generate for capacity-aware forward searches |
| `MOIRAI_CAPACITY_DEFAULT_DEMAND` | `1` | Capacity units consumed when the load payload omits `capacity_units` |
| `MOIRAI_CAPACITY_DEFAULT_UNITS` | unset | Optional default capacity for missing buckets |
| `MOIRAI_CAPACITY_PRICE_WEIGHT` | `0` | Convert optimizer/forecast edge prices into equivalent routing minutes |
| `MOIRAI_CAPACITY_PROTECTION_WEIGHT` | `0` | Penalize routes that would consume protected forecast capacity |
| `MOIRAI_CAPACITY_CRITICAL_PRICE_WEIGHT` | `MOIRAI_CAPACITY_PRICE_WEIGHT` | Price weight for reverse critical/ultimate searches |
| `MOIRAI_CAPACITY_CRITICAL_PROTECTION_WEIGHT` | `MOIRAI_CAPACITY_PROTECTION_WEIGHT` | Protected-capacity penalty for reverse critical/ultimate searches |
| `MOIRAI_CAPACITY_FORECAST_PROTECTION_RATIO` | `0` | Protect this fraction of each capacity bucket when no explicit protected/forecast value is supplied |
| `MOIRAI_CAPACITY_FORECAST_MIN_PROTECTED_UNITS` | `0` | Minimum protected units to apply with the forecast protection policy |
| `MOIRAI_CAPACITY_FORECAST_MAX_PROTECTED_UNITS` | unset | Maximum protected units to apply with the forecast protection policy |
| `MOIRAI_CAPACITY_STATE_PATH` | unset | Persist capacity buckets and owner reservations across restarts |
| `MOIRAI_CAPACITY_STATE_BACKEND` | `auto` | `auto`, `snapshot`, or `rocksdb`; `auto` uses RocksDB when compiled and the path is not a `.log`, `.txt`, or `.snapshot` file |
| `MOIRAI_CAPACITY_STATE_REBUILD` | `false` | Clear local capacity state before processing stream records |
| `MOIRAI_CAPACITY_RESERVATION_TTL_MINUTES` | `0` | Expire owner reservations after this many minutes; `0` disables expiry |
| `MOIRAI_CAPACITY_FREEZE_BEFORE_DEPARTURE_MINUTES` | `0` | Prevent different same-owner re-reservations inside this departure freeze window |

Load payloads may provide a positive integer `capacity_units` field. If omitted,
the configured default demand is used.

Capacity records can flow through the same solver input stream. They do not emit
search documents. Supported fields:

| Field | Meaning |
| --- | --- |
| `edge` | Numeric solver edge id |
| `edge_code` | Route edge code; resolved against the loaded graph |
| `departure` | Concrete departure epoch minute |
| `departure_ts` | Concrete departure epoch seconds, converted to minutes |
| `capacity` | Total capacity units for the edge departure |
| `reserved` | Reserved units to seed from an external snapshot |
| `consumed` | Consumed units to seed from an external snapshot |
| `mode`, `update_mode`, `op` | `snapshot` by default; `delta` adds numeric fields to the current bucket |
| `protected` | Forecast-protected units not available to normal online search |
| `forecast_protected` | Alias for forecast-derived protected units when `protected` is absent |
| `forecast_units` | Alias for forecast-derived protected units when `protected` / `forecast_protected` are absent |
| `forecast_demand` | Alias for forecast-derived protected units when the fields above are absent |
| `historical_demand` | Alias for protected demand derived from historical features |
| `scenario_demand` | Alias for protected demand from a future burst/scenario model |
| `critical_forecast` | Alias for protected demand reserved for future critical shipments |
| `reserve_ratio`, `protection_ratio`, `forecast_reserve_ratio` | Per-bucket protection-ratio override |
| `min_protected`, `min_protected_units` | Per-bucket minimum protected-unit override |
| `max_protected`, `max_protected_units` | Per-bucket maximum protected-unit override |
| `price` | Scarcity price used by dual-price routing |

Explicit `protected` / forecast fields win over policy-derived protection. If a
capacity record supplies only `capacity`, the configured forecast protection
ratio/min/max policy derives protected capacity for that bucket.

Lifecycle records can also flow through the load stream. They do not emit search
documents. Use `capacity_action`, `reservation_action`, or `action` with
`release` / `consume`, plus an `owner`, `reservation_owner`, `id`, `waybill`, or
`package` field. Release removes an owner reservation; consume removes the owner
reservation and moves its reserved units into consumed capacity.

`MOIRAI_CAPACITY_STATE_PATH` is optional. With the snapshot backend, Moirai writes
an atomic local text snapshot. With the RocksDB backend, Moirai stores the same
bucket and reservation records under `bucket:` and `reservation:` keys. Restore
loads buckets first, then owner reservations, so reserved capacity is
reconstructed from owner state rather than trusting stale aggregate counters.

RocksDB is compiled in only when `pkg-config` can find `rocksdb` at CMake
configure time. If `MOIRAI_CAPACITY_STATE_BACKEND=rocksdb` is set for a build
without RocksDB support, startup fails instead of silently falling back to a
different durability mode.

## Rolling-Horizon Optimization

Rolling-horizon optimization is disabled by default. When enabled together with
capacity routing, the wrapper generates multiple forward candidates for each
load in the current horizon batch, solves a capacity-constrained assignment over
that candidate set, commits the selected reservations, and emits the selected
earliest path. The horizon is bounded by
`MOIRAI_ROLLING_HORIZON_MAX_BATCH_SIZE` and
`MOIRAI_ROLLING_HORIZON_WINDOW_MS`.

| Environment variable | Default | Meaning |
| --- | --- | --- |
| `MOIRAI_ROLLING_HORIZON_ENABLED` | `false` | Optimize candidate choices for each solver batch |
| `MOIRAI_ROLLING_HORIZON_USE_HIGHS` | `true` | Use HiGHS when compiled; otherwise the deterministic greedy fallback is used |
| `MOIRAI_ROLLING_HORIZON_COMMIT` | `false` | Commit optimizer-selected reservations even if `MOIRAI_CAPACITY_RESERVE_EARLIEST=false` |
| `MOIRAI_ROLLING_HORIZON_MAX_BATCH_SIZE` | `64` | Maximum loads to optimize together in one horizon solve |
| `MOIRAI_ROLLING_HORIZON_WINDOW_MS` | `0` | After the first load arrives, wait up to this long for more loads before solving |
| `MOIRAI_ROLLING_HORIZON_SLA_PENALTY` | `1000000` | Objective penalty for selecting an SLA-breaching candidate |
| `MOIRAI_ROLLING_HORIZON_HOP_PENALTY` | `1000` | Objective penalty per hop |
| `MOIRAI_ROLLING_HORIZON_LATENESS_PENALTY` | `100` | Objective penalty per late minute |
| `MOIRAI_ROLLING_HORIZON_PRICE_SCALE` | `10` | Capacity shadow-price scale written back for online dual-price routing |
| `MOIRAI_ROLLING_HORIZON_PRICE_SMOOTHING` | `0` | Exponential smoothing factor for capacity price updates; `0` fully applies the newest price |

HiGHS is compiled in only when CMake finds `highs::highs`. If HiGHS is absent or
returns an incomplete assignment, Moirai uses the greedy optimizer. Candidate
generation is controlled by `MOIRAI_CAPACITY_CANDIDATE_COUNT`; values above `1`
are needed for the optimizer to have alternate route choices.

## simdjson Provider

Production builds default to a pinned source build of simdjson 4.6.4:
`-DMOIRAI_SIMDJSON_PROVIDER=fetch`. This is the intended Fedora path because the
distribution package can lag the required library version. Do not manually copy a
new `simdjson.h` over the system header; that only upgrades the compile-time
header and can still link against an older `libsimdjson`.

Use `-DMOIRAI_SIMDJSON_PROVIDER=system` only when the host has a matching
simdjson 4.6 or newer CMake package, headers, and library installed. The build
fails if the package version and header version disagree.

## DWH Append Audit

OpenSearch is written with stable waybill document ids, so it stores the current
record for each waybill. Enable the DWH audit sink when downstream consumers need
the historical append stream of every produced expected-path record.

```sh
DWH_AUDIT_ENABLED=true
DWH_AUDIT_DIR=/var/log/expath/audit
DWH_AUDIT_ROTATE_RECORDS=100000
DWH_AUDIT_ROTATE_BYTES=134217728
DWH_AUDIT_ROTATE_SECONDS=300
DWH_AUDIT_KAFKA_ENABLED=false
DWH_AUDIT_KAFKA_TOPIC=Expected-Path-Prioritization-ESP.audit
DWH_AUDIT_KAFKA_BROKERS=
DWH_AUDIT_KAFKA_REQUIRED=true
DWH_AUDIT_KAFKA_FLUSH_TIMEOUT_MS=30000
DWH_AUDIT_KAFKA_QUEUE_RETRIES=3
DWH_AUDIT_KAFKA_CONFIG=
DWH_BUCKET=dwh-prod-datalake
DWH_PREFIX=expectedpath
DWH_EXPORT_WORK_DIR=/home/fedora/.local/state/moirai/dwh-export
DWH_PARQUET_COALESCE=50
DWH_PROCESSING_STALE_SECONDS=1800
DWH_EXPORT_MAX_FILES_PER_RUN=1000
DWH_PROCESSED_RETENTION_SECONDS=0
JAVA_HOME=/usr/lib/jvm/java-25-openjdk
DWH_ALLOW_UNSUPPORTED_JAVA=true
```

The application writes active files as `*.jsonl.open` and atomically publishes
closed files as `*.jsonl`. Each line is the same data document body sent to
OpenSearch, without bulk `index` metadata, so the stream remains append-only for
DWH even though OpenSearch upserts by waybill.

Prefer `DWH_AUDIT_KAFKA_ENABLED=true` when DWH can consume from Kafka directly.
The Kafka audit sink publishes the same append JSON body that the file audit sink
writes, keyed by the stable document id/waybill, to `DWH_AUDIT_KAFKA_TOPIC`.
`DWH_AUDIT_KAFKA_BROKERS` defaults to `BROKER_URI` when omitted. Producer
credentials default to the existing `KAFKA_SECURITY_PROTOCOL`,
`KAFKA_SASL_MECHANISMS`, `KAFKA_SASL_USERNAME`, and `KAFKA_SASL_PASSWORD`
variables, with `DWH_AUDIT_KAFKA_*` credential overrides available when the DWH
topic uses a different Kafka principal. Additional producer properties can be
passed as comma-separated `key=value` entries in `DWH_AUDIT_KAFKA_CONFIG`.
With the default `DWH_AUDIT_KAFKA_REQUIRED=true`, a Kafka audit enqueue or flush
failure fails the writer thread so the service does not silently lose DWH append
records. Set it to `false` only if OpenSearch freshness is more important than a
complete DWH audit stream during Kafka outages.

Kafka audit topics must be append-retaining topics. Do not enable compaction if
DWH needs every historical expected-path record. Once DWH confirms the Kafka
topic is the durable handoff, set `DWH_AUDIT_ENABLED=false` and disable
`moirai-dwh-export.timer` / `moirai-dwh-cleanup.timer` on the host to remove the
local JSONL, Spark, Java, and S3-export path.

The export timer only consumes closed files. It applies the same flattening
contract as the previous `autoconvert.py`: nested structs are flattened with
underscore-separated column names, arrays remain arrays, optional bulk metadata
rows are filtered if present, output is coalesced to 50 parquet files by default,
and parquet is partitioned by string column `ad`.
If a previous exporter run crashes after claiming files, stale
`*.jsonl.processing` files are retried after `DWH_PROCESSING_STALE_SECONDS`.
`DWH_EXPORT_MAX_FILES_PER_RUN` limits each timer invocation so a recovery run
does not claim an unbounded live backlog into `*.jsonl.processing`. For Spark
4.2 and Java 25 deployments, set `DWH_ALLOW_UNSUPPORTED_JAVA=true`; omit it when
running the stable Java 17/21 Spark path.
`DWH_PROCESSED_RETENTION_SECONDS=0` deletes local source JSONL immediately after
successful parquet upload. Use a positive value to keep a short local recovery
window, or `-1` to archive indefinitely.

The exporter accepts both the new variable names and the old names:
`DWH_BUCKET` or `STORAGE`, and `DWH_PREFIX` or `OUT_PFX`.

During PGO training, keep both profile data and append audit output on attached
data volumes instead of the root volume. The generated compiler profile files
should stay under the repo mount, for example
`-DMOIRAI_PGO_DIR=/home/fedora/moirai/.pgo`. The high-volume audit stream should
use the larger log volume.

Use a dedicated PGO config file rather than inline `Environment=` overrides in a
unit that also has `EnvironmentFile=%h/.config/moirai/moirai.conf`. systemd
loads environment files at exec time, so the production config can override
inline PGO values such as `SEARCH_INDEX` and `KAFKA_GROUP_ID`.

```sh
PGO_RUN_ID=$(date -u +%Y%m%d%H%M%S)
mkdir -p /var/log/expath/pgo-audit
grep -Ev '^(MOIRAI_ROUTE_EXPANSION_THREADS|MOIRAI_BINARY|KAFKA_GROUP_ID|KAFKA_AUTO_OFFSET_RESET|SEARCH_INDEX|DWH_AUDIT_ENABLED|DWH_AUDIT_DIR)=' ~/.config/moirai/moirai.conf > ~/.config/moirai/moirai-pgo-train.conf
printf 'MOIRAI_ROUTE_EXPANSION_THREADS=1\nMOIRAI_BINARY=%s/moirai/build-pgo-gen/moirai\nKAFKA_GROUP_ID=MOIRAI_PGO_TRAIN_%s\nKAFKA_AUTO_OFFSET_RESET=earliest\nSEARCH_INDEX=moirai.pgo.train.%s\nDWH_AUDIT_ENABLED=true\nDWH_AUDIT_DIR=/var/log/expath/pgo-audit\n' "$HOME" "$PGO_RUN_ID" "$PGO_RUN_ID" >> ~/.config/moirai/moirai-pgo-train.conf
cp moirai-pgo-train.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user start moirai-pgo-train.service
```

```sh
python3 -m venv ~/.local/share/moirai-exporter-venv
~/.local/share/moirai-exporter-venv/bin/python -m pip install --upgrade pip boto3 pyspark
cp moirai-dwh-export.service ~/.config/systemd/user/
cp moirai-dwh-export.timer ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now moirai-dwh-export.timer
```

## Local file mode

`--query-from` switches the reader to local file mode. Kafka broker and package
topic options are not required in this mode, but route/facility/search options
are still required because the solver graph and writer are initialized normally.

```sh
moirai \
  --query-from ./loads.json \
  --route-api "${ROUTE_API_URI}" \
  --route-token "${ROUTE_API_TOKEN}" \
  --facility-api "${FACILITY_API_URI}" \
  --facility-token "${FACILITY_API_TOKEN}" \
  --facility-timings "${FACILITY_TIMINGS_FILE}" \
  --search-uri "${SEARCH_URI}" \
  --search-user "${SEARCH_USER}" \
  --search-pass "${SEARCH_PASS}" \
  --search-index "${SEARCH_INDEX}"
```
