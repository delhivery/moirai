#!/usr/bin/env python3
"""Capture sanitized load payloads from Kafka for local benchmarks."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
import time
from typing import Any

try:
    from confluent_kafka import Consumer, KafkaException
except ImportError:  # pragma: no cover - exercised on production capture host
    Consumer = None
    KafkaException = Exception


ROUTING_FIELDS = (
    "id",
    "location",
    "destination",
    "time",
    "ipdd_destination",
    "cs_slid",
    "cs_act",
    "pid",
)


def require_env(name: str) -> str:
    value = os.environ.get(name)
    if not value:
        raise SystemExit(f"{name} is required")
    return value


def hash_id(value: Any, salt: str) -> str:
    if not isinstance(value, str) or not value:
        return ""
    digest = hashlib.sha256(f"{salt}:{value}".encode("utf-8")).hexdigest()
    return f"id_{digest[:24]}"


def copy_string(payload: dict[str, Any], key: str) -> str:
    value = payload.get(key)
    return value if isinstance(value, str) else ""


def sanitize_waybill(value: Any, salt: str) -> dict[str, str] | None:
    if not isinstance(value, dict):
        return None
    output = {
        "id": hash_id(value.get("id"), salt),
        "cn": copy_string(value, "cn"),
        "ipdd_destination": copy_string(value, "ipdd_destination"),
    }
    if not output["cn"] and not output["ipdd_destination"]:
        return None
    return output


def sanitize_payload(payload: Any, salt: str) -> dict[str, Any] | None:
    if not isinstance(payload, dict):
        return None

    output: dict[str, Any] = {}
    for field in ROUTING_FIELDS:
        if field == "id":
            output[field] = hash_id(payload.get(field), salt)
        else:
            value = payload.get(field)
            if isinstance(value, str):
                output[field] = value

    if not output.get("location") or not output.get("destination") or not output.get("time"):
        return None

    items = payload.get("items")
    if not isinstance(items, list):
        items = payload.get("item")
    if isinstance(items, list):
        sanitized_items = [
            sanitized
            for item in items
            if (sanitized := sanitize_waybill(item, salt)) is not None
        ]
        if sanitized_items:
            output["items"] = sanitized_items

    return output


def consumer_config(args: argparse.Namespace) -> dict[str, str]:
    config = {
        "bootstrap.servers": require_env("BROKER_URI"),
        "group.id": args.group_id or os.environ.get("KAFKA_GROUP_ID", "MOIRAI_FIXTURE_CAPTURE"),
        "auto.offset.reset": os.environ.get("KAFKA_AUTO_OFFSET_RESET", "earliest"),
        "enable.auto.commit": "false",
        "enable.partition.eof": "false",
    }
    optional = {
        "security.protocol": os.environ.get("KAFKA_SECURITY_PROTOCOL"),
        "sasl.mechanisms": os.environ.get("KAFKA_SASL_MECHANISMS"),
        "sasl.username": os.environ.get("KAFKA_SASL_USERNAME"),
        "sasl.password": os.environ.get("KAFKA_SASL_PASSWORD"),
    }
    for key, value in optional.items():
        if value:
            config[key] = value
    return config


def write_output(path: str, records: list[dict[str, Any]], output_format: str) -> None:
    with open(path, "w", encoding="utf-8") as handle:
        if output_format == "jsonl":
            for record in records:
                handle.write(json.dumps(record, sort_keys=True, separators=(",", ":")))
                handle.write("\n")
        else:
            json.dump(records, handle, indent=2, sort_keys=True)
            handle.write("\n")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--topic", default=os.environ.get("LOAD_TOPIC"))
    parser.add_argument("--output", default="benchmarks/fixtures/production_loads.sanitized.jsonl")
    parser.add_argument("--format", choices=("jsonl", "json"), default="jsonl")
    parser.add_argument("--max-records", type=int, default=100_000)
    parser.add_argument("--max-seconds", type=int, default=300)
    parser.add_argument("--poll-timeout", type=float, default=1.0)
    parser.add_argument("--hash-salt", default=os.environ.get("MOIRAI_FIXTURE_HASH_SALT", "moirai-fixture"))
    parser.add_argument("--group-id", default=None)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if Consumer is None:
        raise SystemExit("Install confluent-kafka on the capture host: python3 -m pip install confluent-kafka")
    if not args.topic:
        raise SystemExit("LOAD_TOPIC or --topic is required")
    if args.max_records <= 0:
        raise SystemExit("--max-records must be positive")
    if args.max_seconds <= 0:
        raise SystemExit("--max-seconds must be positive")

    consumer = Consumer(consumer_config(args))
    records: list[dict[str, Any]] = []
    deadline = time.monotonic() + args.max_seconds
    try:
        consumer.subscribe([args.topic])
        while len(records) < args.max_records and time.monotonic() < deadline:
            message = consumer.poll(args.poll_timeout)
            if message is None:
                continue
            if message.error():
                raise KafkaException(message.error())
            try:
                payload = json.loads(message.value().decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError):
                continue
            sanitized = sanitize_payload(payload, args.hash_salt)
            if sanitized is not None:
                records.append(sanitized)
    finally:
        consumer.close()

    os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)
    write_output(args.output, records, args.format)
    print(f"wrote {len(records)} sanitized load records to {args.output}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
