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
