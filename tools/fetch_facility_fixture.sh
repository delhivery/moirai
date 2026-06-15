#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_PATH="${1:-${ROOT}/benchmarks/fixtures/production_facilities.sanitized.json}"

: "${FACILITY_API_URI:?FACILITY_API_URI is required}"
: "${FACILITY_API_TOKEN:?FACILITY_API_TOKEN is required}"

RAW_DIR="$(mktemp -d)"
trap 'rm -rf "${RAW_DIR}"' EXIT

python3 - "${FACILITY_API_URI}" "${FACILITY_API_TOKEN}" "${RAW_DIR}" "${OUTPUT_PATH}" <<'PY'
import json
import os
import sys
import urllib.parse
import urllib.request
from datetime import datetime, timezone

base_uri, token, raw_dir, output_path = sys.argv[1:5]


def with_query(uri, **params):
    parsed = urllib.parse.urlparse(uri)
    query = dict(urllib.parse.parse_qsl(parsed.query, keep_blank_values=True))
    query.update({key: str(value) for key, value in params.items()})
    return urllib.parse.urlunparse(parsed._replace(query=urllib.parse.urlencode(query)))


def fetch(page):
    request = urllib.request.Request(
        with_query(base_uri, page=page, status="active"),
        headers={"Accept": "application/json", "Authorization": f"Bearer {token}"},
    )
    with urllib.request.urlopen(request, timeout=60) as response:
        return json.load(response)


def result_payload(payload):
    if not isinstance(payload, dict):
        raise SystemExit("facility response is not an object")
    result = payload.get("result")
    if not isinstance(result, dict):
        raise SystemExit("facility response missing result object")
    data = result.get("data")
    pages = result.get("total_page_count")
    if not isinstance(data, list) or not isinstance(pages, int):
        raise SystemExit("facility response missing result.data or total_page_count")
    return result, data, pages


all_facilities = []
page = 1
total_pages = 1
while page <= total_pages:
    payload = fetch(page)
    with open(os.path.join(raw_dir, f"page-{page}.json"), "w", encoding="utf-8") as handle:
        json.dump(payload, handle)
    result, data, total_pages = result_payload(payload)
    for facility in data:
        if not isinstance(facility, dict):
            continue
        code = facility.get("facility_code")
        if not isinstance(code, str) or not code:
            continue
        sanitized = {"facility_code": code}
        property_id = facility.get("property_id")
        if isinstance(property_id, str) and property_id:
            sanitized["property_id"] = property_id
        all_facilities.append(sanitized)
    page += 1

os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
with open(output_path, "w", encoding="utf-8") as handle:
    json.dump(
        {
            "fixture_source": "sanitized production facilities",
            "captured_at": datetime.now(timezone.utc).replace(microsecond=0).isoformat(),
            "result": {
                "total_page_count": 1,
                "data": all_facilities,
            },
        },
        handle,
        indent=2,
        sort_keys=True,
    )
    handle.write("\n")

print(f"wrote {len(all_facilities)} sanitized facilities to {output_path}")
PY
