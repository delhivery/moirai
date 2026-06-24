# Moirai DWH Kafka Dataset Schema

## Dataset

- Kafka topic: `Expected-Path-Prioritization-ESP.audit`
- Kafka key: `waybill`
- Kafka value format: JSON object
- Producer: `moirai` DWH audit Kafka sink
- Semantics: append-only audit stream. Every successful expected-path result is emitted as a new record. Consumers must not compact this topic if they need historical append records.
- OpenSearch behavior differs: OpenSearch upserts by `waybill`, while this Kafka stream preserves every produced record.

## Important DWH Connector Note

The Kafka audit payload stringifies `earliest.locations` and `ultimate.locations`
as JSON arrays encoded inside string fields because `sop.md` says arrays of
objects are not supported by the DWH sink connector. OpenSearch documents still
store those fields as arrays in `_source`; this DWH Kafka projection keeps the
full path available without exposing nested object arrays to the connector.

## Timestamp Conventions

- `updated_at`: ISO-8601 UTC string, for example `2026-06-19T06:40:41Z`.
- `updated_at_ts`: Unix epoch seconds for `updated_at`.
- `pdd`, `arrival`, `departure`: display timestamp strings in `MM/dd/yy HH:mm:ss`.
- `pdd_ts`, `arrival_ts`, `departure_ts`: Unix epoch seconds. Prefer these fields for time arithmetic and partitioning.

## Field Summary

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `waybill` | string | yes | Stable waybill or bag id. Also used as the Kafka message key. |
| `package` | string | yes | Package id associated with the selected expected path. For bags with child packages, this can be a child waybill. |
| `cs_slid` | string | yes | Current scan source/location id from the load payload. May be an empty string. |
| `cs_act` | string | yes | Current scan action from the load payload. May be an empty string. |
| `pid` | string | yes | Parent/process/package id from the load payload. May be an empty string. |
| `fail` | string | no | Failure reason when pathing fails. Omitted for successful pathing. |
| `is_critical` | boolean | yes | True when the shipment is already critical, cannot be pathed, or cannot meet the relevant deadline. |
| `pdd` | string | yes | Display PDD timestamp in `MM/dd/yy HH:mm:ss`. |
| `pdd_ts` | integer bigint | yes | PDD as Unix epoch seconds. |
| `updated_at` | string date-time | yes | UTC time when Moirai emitted the record. |
| `updated_at_ts` | integer bigint | yes | `updated_at` as Unix epoch seconds. |
| `earliest` | object | no | Earliest feasible forward path from current facility to destination. Omitted when unavailable. |
| `ultimate` | object | no | Latest feasible reverse path to still meet PDD. Omitted when unavailable or when the shipment is critical. |

## Path Section Shape

`earliest` and `ultimate` use the same projected structure.

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `hop_count` | integer bigint | yes | Number of locations in the path. |
| `location_codes` | array<string> | yes | Unique facility codes in path order. |
| `route_codes` | array<string> | yes | Unique route ids in path order. Terminal locations do not have route ids. |
| `locations` | string | yes | Full path locations as a JSON array string. Parse this field to recover the per-hop objects. |
| `first` | object | yes | First path location. |
| `second` | object | no | Second path location when the path has at least two hops. |

## Path Location Shape

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `code` | string | yes | Facility code. |
| `facility_name` | string | no | Facility display name, when available from route/facility inputs. |
| `arrival` | string | yes | Arrival display timestamp in `MM/dd/yy HH:mm:ss`. |
| `arrival_ts` | integer bigint | yes | Arrival Unix epoch seconds. |
| `route` | string | no | Outbound route id from this location. Omitted on the terminal location. |
| `route_name` | string | no | Outbound route display name, when available. |
| `departure` | string | no | Departure display timestamp in `MM/dd/yy HH:mm:ss`. Omitted on the terminal location. |
| `departure_ts` | integer bigint | no | Departure Unix epoch seconds. Omitted on the terminal location. |

## DWH Schema Registry Payload

```json
{
  "schema_name": "Expected-Path-Prioritization-ESP.audit",
  "version": 1,
  "schema": {
    "schema": "Expected-Path-Prioritization-ESP.audit",
    "type": "object",
    "title": "Expected-Path-Prioritization-ESP.audit",
    "required": [
      "waybill",
      "package",
      "cs_slid",
      "cs_act",
      "pid",
      "is_critical",
      "pdd",
      "pdd_ts",
      "updated_at",
      "updated_at_ts"
    ],
    "properties": {
      "waybill": {
        "type": "string",
        "description": "Stable waybill or bag id. This is also the Kafka message key.",
        "classification": ["OCD"],
        "tags": [
          { "key": "deprecated", "value": false },
          { "key": "encrypted", "value": false }
        ]
      },
      "package": {
        "type": "string",
        "description": "Package id associated with the selected expected path. For bags with child packages, this can be a child waybill.",
        "classification": ["OCD"],
        "tags": [
          { "key": "deprecated", "value": false },
          { "key": "encrypted", "value": false }
        ]
      },
      "cs_slid": {
        "type": "string",
        "description": "Current scan source/location id from the load payload. May be an empty string.",
        "classification": ["OCD"],
        "tags": [
          { "key": "deprecated", "value": false },
          { "key": "encrypted", "value": false }
        ]
      },
      "cs_act": {
        "type": "string",
        "description": "Current scan action from the load payload. May be an empty string.",
        "classification": ["OCD"],
        "tags": [
          { "key": "deprecated", "value": false },
          { "key": "encrypted", "value": false }
        ]
      },
      "pid": {
        "type": "string",
        "description": "Parent/process/package id from the load payload. May be an empty string.",
        "classification": ["OCD"],
        "tags": [
          { "key": "deprecated", "value": false },
          { "key": "encrypted", "value": false }
        ]
      },
      "fail": {
        "type": "string",
        "description": "Failure reason when pathing fails. Omitted for successful pathing.",
        "classification": ["OCD"],
        "tags": [
          { "key": "deprecated", "value": false },
          { "key": "encrypted", "value": false }
        ]
      },
      "is_critical": {
        "type": "boolean",
        "description": "True when the shipment is already critical, cannot be pathed, or cannot meet the relevant deadline.",
        "classification": ["OCD"],
        "tags": [
          { "key": "deprecated", "value": false },
          { "key": "encrypted", "value": false }
        ]
      },
      "pdd": {
        "type": "string",
        "description": "Display PDD timestamp in MM/dd/yy HH:mm:ss format.",
        "classification": ["OCD"],
        "tags": [
          { "key": "deprecated", "value": false },
          { "key": "encrypted", "value": false }
        ]
      },
      "pdd_ts": {
        "type": "integer",
        "format": "bigint",
        "description": "PDD as Unix epoch seconds.",
        "classification": ["OCD"],
        "tags": [
          { "key": "deprecated", "value": false },
          { "key": "encrypted", "value": false }
        ]
      },
      "updated_at": {
        "type": "string",
        "format": "date-time",
        "timezone": "UTC",
        "description": "UTC timestamp when Moirai emitted the record.",
        "classification": ["OCD"],
        "tags": [
          { "key": "deprecated", "value": false },
          { "key": "encrypted", "value": false }
        ]
      },
      "updated_at_ts": {
        "type": "integer",
        "format": "bigint",
        "description": "updated_at as Unix epoch seconds.",
        "classification": ["OCD"],
        "tags": [
          { "key": "deprecated", "value": false },
          { "key": "encrypted", "value": false }
        ]
      },
      "earliest": {
        "type": "object",
        "required": [],
        "description": "Earliest feasible forward path from current facility to destination.",
        "classification": ["OCD"],
        "tags": [
          { "key": "deprecated", "value": false },
          { "key": "encrypted", "value": false }
        ],
        "properties": {
          "hop_count": {
            "type": "integer",
            "format": "bigint",
            "description": "Number of locations in the earliest path.",
            "classification": ["OCD"],
            "tags": [
              { "key": "deprecated", "value": false },
              { "key": "encrypted", "value": false }
            ]
          },
          "location_codes": {
            "type": "array",
            "description": "Unique facility codes in earliest path order.",
            "classification": ["OCD"],
            "items": { "type": "string" },
            "tags": [
              { "key": "deprecated", "value": false },
              { "key": "encrypted", "value": false }
            ]
          },
          "route_codes": {
            "type": "array",
            "description": "Unique route ids in earliest path order.",
            "classification": ["OCD"],
            "items": { "type": "string" },
            "tags": [
              { "key": "deprecated", "value": false },
              { "key": "encrypted", "value": false }
            ]
          },
          "locations": {
            "type": "string",
            "description": "Full earliest path as a JSON array string. Parse this field to recover per-hop objects.",
            "classification": ["OCD"],
            "tags": [
              { "key": "deprecated", "value": false },
              { "key": "encrypted", "value": false }
            ]
          },
          "first": {
            "type": "object",
            "required": [],
            "description": "First location in the earliest path.",
            "classification": ["OCD"],
            "tags": [
              { "key": "deprecated", "value": false },
              { "key": "encrypted", "value": false }
            ],
            "properties": {
              "code": { "type": "string", "description": "Facility code.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "facility_name": { "type": "string", "description": "Facility display name when available.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "arrival": { "type": "string", "description": "Arrival display timestamp in MM/dd/yy HH:mm:ss format.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "arrival_ts": { "type": "integer", "format": "bigint", "description": "Arrival Unix epoch seconds.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "route": { "type": "string", "description": "Outbound route id from this location. Omitted for terminal locations.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "route_name": { "type": "string", "description": "Outbound route display name when available.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "departure": { "type": "string", "description": "Departure display timestamp in MM/dd/yy HH:mm:ss format. Omitted for terminal locations.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "departure_ts": { "type": "integer", "format": "bigint", "description": "Departure Unix epoch seconds. Omitted for terminal locations.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] }
            }
          },
          "second": {
            "type": "object",
            "required": [],
            "description": "Second location in the earliest path when present.",
            "classification": ["OCD"],
            "tags": [
              { "key": "deprecated", "value": false },
              { "key": "encrypted", "value": false }
            ],
            "properties": {
              "code": { "type": "string", "description": "Facility code.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "facility_name": { "type": "string", "description": "Facility display name when available.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "arrival": { "type": "string", "description": "Arrival display timestamp in MM/dd/yy HH:mm:ss format.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "arrival_ts": { "type": "integer", "format": "bigint", "description": "Arrival Unix epoch seconds.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "route": { "type": "string", "description": "Outbound route id from this location. Omitted for terminal locations.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "route_name": { "type": "string", "description": "Outbound route display name when available.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "departure": { "type": "string", "description": "Departure display timestamp in MM/dd/yy HH:mm:ss format. Omitted for terminal locations.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "departure_ts": { "type": "integer", "format": "bigint", "description": "Departure Unix epoch seconds. Omitted for terminal locations.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] }
            }
          }
        }
      },
      "ultimate": {
        "type": "object",
        "required": [],
        "description": "Latest feasible reverse path to still meet PDD. Omitted when unavailable or when the shipment is critical.",
        "classification": ["OCD"],
        "tags": [
          { "key": "deprecated", "value": false },
          { "key": "encrypted", "value": false }
        ],
        "properties": {
          "hop_count": {
            "type": "integer",
            "format": "bigint",
            "description": "Number of locations in the ultimate path.",
            "classification": ["OCD"],
            "tags": [
              { "key": "deprecated", "value": false },
              { "key": "encrypted", "value": false }
            ]
          },
          "location_codes": {
            "type": "array",
            "description": "Unique facility codes in ultimate path order.",
            "classification": ["OCD"],
            "items": { "type": "string" },
            "tags": [
              { "key": "deprecated", "value": false },
              { "key": "encrypted", "value": false }
            ]
          },
          "route_codes": {
            "type": "array",
            "description": "Unique route ids in ultimate path order.",
            "classification": ["OCD"],
            "items": { "type": "string" },
            "tags": [
              { "key": "deprecated", "value": false },
              { "key": "encrypted", "value": false }
            ]
          },
          "locations": {
            "type": "string",
            "description": "Full ultimate path as a JSON array string. Parse this field to recover per-hop objects.",
            "classification": ["OCD"],
            "tags": [
              { "key": "deprecated", "value": false },
              { "key": "encrypted", "value": false }
            ]
          },
          "first": {
            "type": "object",
            "required": [],
            "description": "First location in the ultimate path.",
            "classification": ["OCD"],
            "tags": [
              { "key": "deprecated", "value": false },
              { "key": "encrypted", "value": false }
            ],
            "properties": {
              "code": { "type": "string", "description": "Facility code.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "facility_name": { "type": "string", "description": "Facility display name when available.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "arrival": { "type": "string", "description": "Arrival display timestamp in MM/dd/yy HH:mm:ss format.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "arrival_ts": { "type": "integer", "format": "bigint", "description": "Arrival Unix epoch seconds.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "route": { "type": "string", "description": "Outbound route id from this location. Omitted for terminal locations.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "route_name": { "type": "string", "description": "Outbound route display name when available.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "departure": { "type": "string", "description": "Departure display timestamp in MM/dd/yy HH:mm:ss format. Omitted for terminal locations.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "departure_ts": { "type": "integer", "format": "bigint", "description": "Departure Unix epoch seconds. Omitted for terminal locations.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] }
            }
          },
          "second": {
            "type": "object",
            "required": [],
            "description": "Second location in the ultimate path when present.",
            "classification": ["OCD"],
            "tags": [
              { "key": "deprecated", "value": false },
              { "key": "encrypted", "value": false }
            ],
            "properties": {
              "code": { "type": "string", "description": "Facility code.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "facility_name": { "type": "string", "description": "Facility display name when available.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "arrival": { "type": "string", "description": "Arrival display timestamp in MM/dd/yy HH:mm:ss format.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "arrival_ts": { "type": "integer", "format": "bigint", "description": "Arrival Unix epoch seconds.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "route": { "type": "string", "description": "Outbound route id from this location. Omitted for terminal locations.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "route_name": { "type": "string", "description": "Outbound route display name when available.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "departure": { "type": "string", "description": "Departure display timestamp in MM/dd/yy HH:mm:ss format. Omitted for terminal locations.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] },
              "departure_ts": { "type": "integer", "format": "bigint", "description": "Departure Unix epoch seconds. Omitted for terminal locations.", "classification": ["OCD"], "tags": [{ "key": "deprecated", "value": false }, { "key": "encrypted", "value": false }] }
            }
          }
        }
      }
    }
  },
  "schema_metadata": {
    "team_name": "Moirai",
    "team_email": "",
    "description": "Expected path append audit stream emitted by Moirai for DWH consumption.",
    "project_name": "moirai",
    "classification": ["OCD"]
  },
  "created_at": "2026-06-19T00:00:00.000Z",
  "created_by": "moirai",
  "namespace": "kafka"
}
```

## Sample Kafka Value

```json
{
  "waybill": "BAGCC3918756",
  "package": "BAGCC3918756",
  "cs_slid": "INMHAOQD",
  "cs_act": "UD",
  "pid": "",
  "is_critical": false,
  "earliest": {
    "hop_count": 4,
    "location_codes": ["INMHAOQD", "INKAAZXQ", "INKAATKI", "INTNAQGG"],
    "route_codes": [
      "thanos::sroute:af47c481-04bc-4ab9-9dbb-445e39eb387b",
      "thanos::sroute:181104cc-e5ce-465e-8bcb-929610efe468",
      "thanos::sroute:4f7af42b-0c84-4782-a011-250c001c792f"
    ],
    "locations": "[{\"code\":\"INMHAOQD\",\"facility_name\":\"Example Origin\",\"arrival\":\"06/19/26 06:00:00\",\"arrival_ts\":1781848800,\"route\":\"thanos::sroute:af47c481-04bc-4ab9-9dbb-445e39eb387b\",\"route_name\":\"Example Route 1\",\"departure\":\"06/19/26 07:00:00\",\"departure_ts\":1781852400},{\"code\":\"INKAAZXQ\",\"facility_name\":\"Example Hub\",\"arrival\":\"06/19/26 12:00:00\",\"arrival_ts\":1781870400}]",
    "first": {
      "code": "INMHAOQD",
      "facility_name": "Example Origin",
      "arrival": "06/19/26 06:00:00",
      "arrival_ts": 1781848800,
      "route": "thanos::sroute:af47c481-04bc-4ab9-9dbb-445e39eb387b",
      "route_name": "Example Route 1",
      "departure": "06/19/26 07:00:00",
      "departure_ts": 1781852400
    },
    "second": {
      "code": "INKAAZXQ",
      "facility_name": "Example Hub",
      "arrival": "06/19/26 12:00:00",
      "arrival_ts": 1781870400
    }
  },
  "pdd": "06/20/26 23:59:00",
  "pdd_ts": 1782007140,
  "updated_at": "2026-06-19T06:40:41Z",
  "updated_at_ts": 1781851241
}
```
