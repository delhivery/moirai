# Real Route Fixtures

`tests/fixtures/real_routes.json` is a sanitized, frozen sample of production
scheduled-route data used only for regression tests. It must not contain bearer
tokens, Kafka credentials, search credentials, or raw route API dumps.

To refresh it manually:

```sh
export ROUTE_API_URI='https://thanos-bk.delhivery.com/v1/scheduledroutes?contractual_route=false&source=ep'
export ROUTE_API_TOKEN='...'
tools/fetch_route_fixture.sh
```

To capture a larger ignored benchmark fixture:

```sh
export ROUTE_API_URI='https://thanos-bk.delhivery.com/v1/scheduledroutes?contractual_route=false&source=ep'
export ROUTE_API_TOKEN='...'
MOIRAI_ROUTE_FIXTURE_MAX_ROUTES=100000 \
  tools/fetch_route_fixture.sh benchmarks/fixtures/production_routes.sanitized.json
```

The fetch script writes only parser-relevant route fields:
`route_schedule_uuid`, `name`, `route_type`, `reporting_time`,
`days_of_week`, and a reduced `halt_centers` list. Live fetching is intentionally
not part of `tools/verify.sh` or CTest.

Facility and load benchmark fixtures are intentionally ignored by git. Use:

```sh
export FACILITY_API_URI='https://faas-api.delhivery.com/v3/facilities/'
export FACILITY_API_TOKEN='...'
tools/fetch_facility_fixture.sh

python3 -m pip install confluent-kafka
export BROKER_URI='...'
export LOAD_TOPIC='Expected-Path-Prioritization-ESP.info'
export KAFKA_SECURITY_PROTOCOL='SASL_SSL'
export KAFKA_SASL_MECHANISMS='SCRAM-SHA-512'
export KAFKA_SASL_USERNAME='...'
export KAFKA_SASL_PASSWORD='...'
tools/capture_load_fixture.py --max-records 100000 --max-seconds 300
```

Also provide the production `FACILITY_TIMINGS_FILE` beside the sanitized
facility fixture when running production-shaped benchmarks.
