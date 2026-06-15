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
