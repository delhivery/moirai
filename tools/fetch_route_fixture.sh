#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_PATH="${1:-${ROOT}/tests/fixtures/real_routes.json}"
MAX_ROUTES="${MOIRAI_ROUTE_FIXTURE_MAX_ROUTES:-6}"

: "${ROUTE_API_URI:?ROUTE_API_URI is required}"
: "${ROUTE_API_TOKEN:?ROUTE_API_TOKEN is required}"

RAW_RESPONSE="$(mktemp)"
trap 'rm -f "${RAW_RESPONSE}"' EXIT

curl --fail --silent --show-error --location --retry 3 \
  --header "Accept: application/json" \
  --header "Authorization: Bearer ${ROUTE_API_TOKEN}" \
  "${ROUTE_API_URI}" \
  --output "${RAW_RESPONSE}"

python3 - "${RAW_RESPONSE}" "${OUTPUT_PATH}" "${MAX_ROUTES}" <<'PY'
import json
import math
import sys
from datetime import datetime, timezone

raw_path, output_path, max_routes_text = sys.argv[1], sys.argv[2], sys.argv[3]
try:
    max_routes = int(max_routes_text)
except ValueError:
    raise SystemExit("MOIRAI_ROUTE_FIXTURE_MAX_ROUTES must be an integer")
if max_routes <= 0:
    raise SystemExit("MOIRAI_ROUTE_FIXTURE_MAX_ROUTES must be positive")

with open(raw_path, "r", encoding="utf-8") as handle:
    payload = json.load(handle)

if isinstance(payload, list):
    routes = payload
elif isinstance(payload, dict) and isinstance(payload.get("data"), list):
    routes = payload["data"]
else:
    raise SystemExit("route response is not a JSON array or object with data[]")


def parse_time(value):
    if not isinstance(value, str) or ":" not in value:
        return None
    day_part = 0
    time_part = value
    if "," in value:
        day_text, time_part = value.split(",", 1)
        day_text = day_text.strip().split(" ", 1)[0]
        try:
            day_part = int(day_text)
        except ValueError:
            return None
    time_part = time_part.strip()
    fields = time_part.split(":")
    if len(fields) < 2:
        return None
    try:
        hours = int(fields[0])
        minutes = int(fields[1])
        seconds = int(fields[2]) if len(fields) > 2 else 0
    except ValueError:
        return None
    return (day_part * 24 * 60) + (hours * 60) + minutes + math.ceil(seconds / 60)


def is_blocked_stop(stop):
    return stop.get("loading_allowed") is False


def loading_stops(route):
    stops = route.get("halt_centers")
    if not isinstance(stops, list):
        return []
    return [stop for stop in stops if isinstance(stop, dict) and not is_blocked_stop(stop)]


def valid_route(route):
    if not isinstance(route, dict):
        return False
    required = ("route_schedule_uuid", "name", "route_type", "reporting_time", "halt_centers")
    if any(not isinstance(route.get(key), str) for key in required[:-1]):
        return False
    if parse_time(route.get("reporting_time")) is None:
        return False
    stops = loading_stops(route)
    if len(stops) < 2:
        return False
    for index, stop in enumerate(stops):
        if not isinstance(stop.get("center_code"), str):
            return False
        if index > 0 and parse_time(stop.get("rel_eta")) is None:
            return False
        if parse_time(stop.get("rel_etd")) is None:
            return False
    for source_index, source in enumerate(stops):
        source_departure = parse_time(source.get("rel_etd"))
        for target in stops[source_index + 1:]:
            target_arrival = parse_time(target.get("rel_eta"))
            if target_arrival is None or source_departure is None:
                return False
            if target_arrival < source_departure:
                return False
    return True


def sanitize_stop(stop, position):
    output = {
        "center_code": stop["center_code"],
        "rel_eta": stop.get("rel_eta", "0:00"),
        "rel_etd": stop.get("rel_etd", "0:00"),
    }
    if "loading_allowed" in stop:
        output["loading_allowed"] = bool(stop["loading_allowed"])
    if isinstance(stop.get("position"), int):
        output["position"] = stop["position"]
    else:
        output["position"] = position
    return output


def sanitize_route(route):
    return {
        "route_schedule_uuid": route["route_schedule_uuid"],
        "name": route["name"],
        "route_type": route["route_type"],
        "reporting_time": route["reporting_time"],
        "days_of_week": route.get("days_of_week", []),
        "halt_centers": [
            sanitize_stop(stop, index)
            for index, stop in enumerate(route["halt_centers"])
            if isinstance(stop, dict)
        ],
    }


valid = sorted(
    (route for route in routes if valid_route(route)),
    key=lambda route: str(route.get("route_schedule_uuid", "")),
)

selected = []


def add_first(predicate):
    for route in valid:
        uuid = route.get("route_schedule_uuid")
        if any(existing.get("route_schedule_uuid") == uuid for existing in selected):
            continue
        if predicate(route):
            selected.append(sanitize_route(route))
            return


add_first(lambda route: any(is_blocked_stop(stop) for stop in route.get("halt_centers", [])) and len(loading_stops(route)) >= 2)
add_first(lambda route: isinstance(route.get("days_of_week"), list) and 0 < len(route["days_of_week"]) < 7)
add_first(lambda route: str(route.get("route_type", "")).lower() == "air")
add_first(lambda route: str(route.get("route_type", "")).lower() == "carting")
add_first(lambda route: str(route.get("route_type", "")).lower() not in {"air", "carting"})

for route in valid:
    if len(selected) >= max_routes:
        break
    uuid = route.get("route_schedule_uuid")
    if not any(existing.get("route_schedule_uuid") == uuid for existing in selected):
        selected.append(sanitize_route(route))

if not selected:
    raise SystemExit("no valid routes found for fixture capture")

with open(output_path, "w", encoding="utf-8") as handle:
    json.dump(
        {
            "fixture_source": "sanitized production scheduled routes",
            "captured_at": datetime.now(timezone.utc).replace(microsecond=0).isoformat(),
            "data": selected,
        },
        handle,
        indent=2,
        sort_keys=True,
    )
    handle.write("\n")

print(f"wrote {len(selected)} sanitized routes to {output_path}")
PY
