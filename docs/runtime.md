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
(`pdd_ts`, `arrival_ts`, and `departure_ts`) are epoch minutes. Writer
timestamps (`updated_at_ts`) are epoch seconds.

Path section arrays (`earliest.locations`, `ultimate.locations`, and
`critical.locations`) are kept in `_source` only with `enabled: false` to avoid
indexing every hop in every path. The lightweight `first` and `second` path
objects remain explicitly indexed for filtering and sorting.

OpenSearch does not allow changing an existing field type in place. If an older
index has dynamically inferred fields such as `earliest.first.arrival` as `text`,
create a replacement index with the new mapping and reindex, or delete/recreate
the index during a controlled maintenance window.

## DWH Append Audit

OpenSearch is written with stable waybill document ids, so it stores the current
record for each waybill. Enable the DWH audit sink when downstream consumers need
the historical append stream of every produced expected-path record.

```sh
DWH_AUDIT_ENABLED=true
DWH_AUDIT_DIR=/home/fedora/.local/state/moirai/audit
DWH_AUDIT_ROTATE_RECORDS=100000
DWH_AUDIT_ROTATE_BYTES=134217728
DWH_AUDIT_ROTATE_SECONDS=300
DWH_BUCKET=dwh-prod-datalake
DWH_PREFIX=expectedpath
DWH_EXPORT_WORK_DIR=/home/fedora/.local/state/moirai/dwh-export
DWH_PARQUET_COALESCE=50
DWH_PROCESSING_STALE_SECONDS=1800
JAVA_HOME=/usr/lib/jvm/java-17-openjdk
```

The application writes active files as `*.jsonl.open` and atomically publishes
closed files as `*.jsonl`. Each line is the same data document body sent to
OpenSearch, without bulk `index` metadata, so the stream remains append-only for
DWH even though OpenSearch upserts by waybill.

The export timer only consumes closed files. It applies the same flattening
contract as the previous `autoconvert.py`: nested structs are flattened with
underscore-separated column names, arrays remain arrays, optional bulk metadata
rows are filtered if present, output is coalesced to 50 parquet files by default,
and parquet is partitioned by string column `ad`.
If a previous exporter run crashes after claiming files, stale
`*.jsonl.processing` files are retried after `DWH_PROCESSING_STALE_SECONDS`.
The PySpark exporter supports Java 17 or 21. Newer Java runtimes can fail inside
Spark/Hadoop before reading local audit files.

The exporter accepts both the new variable names and the old names:
`DWH_BUCKET` or `STORAGE`, and `DWH_PREFIX` or `OUT_PFX`.

```sh
python3 -m pip install --user boto3 pyspark
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
