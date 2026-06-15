#!/usr/bin/env python3
"""Sample a running Moirai process and report whether the host is sized well."""

from __future__ import annotations

import argparse
import csv
import os
import re
import statistics
import subprocess
import sys
import time
import urllib.error
import urllib.request
from dataclasses import dataclass
from datetime import UTC, datetime
from pathlib import Path
from typing import Iterable


METRICS_RE = re.compile(
    r"Search writer metrics: .*?audited=(?P<audited>\d+) "
    r"indexed=(?P<indexed>\d+) failed=(?P<failed>\d+) "
    r"retried=(?P<retried>\d+) bulk_requests=(?P<bulk>\d+) "
    r"uploaded_bytes=(?P<bytes>\d+) records_per_sec=(?P<rps>\d+) "
    r"bytes_per_sec=(?P<bps>\d+) queue_depth=(?P<queue>\d+)"
)
STARTUP_RE = re.compile(r"Startup timings: .*?total_ms=(?P<total>\d+)")


@dataclass
class Counters:
    timestamp: float
    process_ticks: int
    cpu_total: int
    cpu_idle: int
    net_rx_bytes: int
    net_tx_bytes: int
    disk_read_bytes: int
    disk_write_bytes: int


@dataclass
class Sample:
    utc_time: str
    elapsed_s: float
    host_cpu_pct: float
    proc_cores: float
    proc_capacity_pct: float
    rss_mb: float
    rss_pct: float
    mem_available_pct: float
    swap_used_mb: float
    threads: int
    open_fds: int
    load1: float
    runnable_tasks: int
    net_rx_mbps: float
    net_tx_mbps: float
    disk_read_mbps: float
    disk_write_mbps: float


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def parse_kb_status(pid: int) -> dict[str, int]:
    result: dict[str, int] = {}
    for line in read_text(Path(f"/proc/{pid}/status")).splitlines():
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        parts = value.strip().split()
        if not parts:
            continue
        try:
            result[key] = int(parts[0])
        except ValueError:
            continue
    return result


def process_ticks(pid: int) -> int:
    content = read_text(Path(f"/proc/{pid}/stat"))
    end = content.rfind(")")
    if end < 0:
        raise RuntimeError(f"Unable to parse /proc/{pid}/stat")
    fields = content[end + 2 :].split()
    utime = int(fields[11])
    stime = int(fields[12])
    return utime + stime


def cpu_totals() -> tuple[int, int]:
    fields = read_text(Path("/proc/stat")).splitlines()[0].split()[1:]
    values = [int(value) for value in fields]
    idle = values[3] + (values[4] if len(values) > 4 else 0)
    return sum(values), idle


def meminfo() -> dict[str, int]:
    result: dict[str, int] = {}
    for line in read_text(Path("/proc/meminfo")).splitlines():
        key, rest = line.split(":", 1)
        result[key] = int(rest.strip().split()[0])
    return result


def loadavg() -> tuple[float, int]:
    fields = read_text(Path("/proc/loadavg")).split()
    runnable = int(fields[3].split("/", 1)[0])
    return float(fields[0]), runnable


def net_bytes() -> tuple[int, int]:
    rx = 0
    tx = 0
    for line in read_text(Path("/proc/net/dev")).splitlines()[2:]:
        name, rest = line.split(":", 1)
        if name.strip() == "lo":
            continue
        fields = rest.split()
        rx += int(fields[0])
        tx += int(fields[8])
    return rx, tx


def disk_bytes() -> tuple[int, int]:
    read_sectors = 0
    write_sectors = 0
    for device in Path("/sys/block").iterdir():
        name = device.name
        if name.startswith(("loop", "ram", "zram", "fd")):
            continue
        if name.startswith("dm-"):
            continue
        stat = (device / "stat").read_text(encoding="utf-8").split()
        read_sectors += int(stat[2])
        write_sectors += int(stat[6])
    return read_sectors * 512, write_sectors * 512


def open_fd_count(pid: int) -> int:
    try:
        return len(list(Path(f"/proc/{pid}/fd").iterdir()))
    except OSError:
        return 0


def capture(pid: int) -> Counters:
    total, idle = cpu_totals()
    rx, tx = net_bytes()
    read_bytes, write_bytes = disk_bytes()
    return Counters(
        timestamp=time.monotonic(),
        process_ticks=process_ticks(pid),
        cpu_total=total,
        cpu_idle=idle,
        net_rx_bytes=rx,
        net_tx_bytes=tx,
        disk_read_bytes=read_bytes,
        disk_write_bytes=write_bytes,
    )


def find_pid(process_name: str) -> int:
    candidates: list[tuple[int, int]] = []
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        pid = int(entry.name)
        try:
            comm = read_text(entry / "comm").strip()
            cmdline = read_text(entry / "cmdline").replace("\x00", " ")
            status = parse_kb_status(pid)
        except OSError:
            continue
        if comm == process_name or f"/{process_name}" in cmdline:
            candidates.append((status.get("VmRSS", 0), pid))
    if not candidates:
        raise RuntimeError(f"No running process matched {process_name!r}")
    candidates.sort(reverse=True)
    return candidates[0][1]


def instance_metadata() -> dict[str, str]:
    result: dict[str, str] = {}
    token_request = urllib.request.Request(
        "http://169.254.169.254/latest/api/token",
        method="PUT",
        headers={"X-aws-ec2-metadata-token-ttl-seconds": "60"},
    )
    try:
        with urllib.request.urlopen(token_request, timeout=0.3) as response:
            token = response.read().decode("utf-8")
    except (OSError, urllib.error.URLError):
        return result

    def get(path: str) -> str | None:
        request = urllib.request.Request(
            f"http://169.254.169.254/latest/{path}",
            headers={"X-aws-ec2-metadata-token": token},
        )
        try:
            with urllib.request.urlopen(request, timeout=0.3) as response:
                return response.read().decode("utf-8")
        except (OSError, urllib.error.URLError):
            return None

    for key, path in {
        "instance_type": "meta-data/instance-type",
        "availability_zone": "meta-data/placement/availability-zone",
        "instance_id": "meta-data/instance-id",
    }.items():
        value = get(path)
        if value:
            result[key] = value
    return result


def pct(numerator: float, denominator: float) -> float:
    if denominator <= 0:
        return 0.0
    return (numerator / denominator) * 100.0


def mbps(byte_delta: int, elapsed: float) -> float:
    if elapsed <= 0:
        return 0.0
    return byte_delta / elapsed / (1024 * 1024)


def sample_once(pid: int, previous: Counters, current: Counters, vcpus: int) -> Sample:
    elapsed = current.timestamp - previous.timestamp
    cpu_delta = current.cpu_total - previous.cpu_total
    idle_delta = current.cpu_idle - previous.cpu_idle
    process_delta = current.process_ticks - previous.process_ticks
    ticks_per_second = os.sysconf(os.sysconf_names["SC_CLK_TCK"])
    proc_cpu_pct = (process_delta / ticks_per_second / elapsed) * 100.0

    status = parse_kb_status(pid)
    memory = meminfo()
    load1, runnable = loadavg()
    total_memory_kb = memory.get("MemTotal", 0)
    available_memory_kb = memory.get("MemAvailable", 0)
    swap_total_kb = memory.get("SwapTotal", 0)
    swap_free_kb = memory.get("SwapFree", 0)

    return Sample(
        utc_time=datetime.now(UTC).isoformat(timespec="seconds"),
        elapsed_s=elapsed,
        host_cpu_pct=pct(cpu_delta - idle_delta, cpu_delta),
        proc_cores=proc_cpu_pct / 100.0,
        proc_capacity_pct=proc_cpu_pct / max(vcpus, 1),
        rss_mb=status.get("VmRSS", 0) / 1024,
        rss_pct=pct(status.get("VmRSS", 0), total_memory_kb),
        mem_available_pct=pct(available_memory_kb, total_memory_kb),
        swap_used_mb=max(swap_total_kb - swap_free_kb, 0) / 1024,
        threads=status.get("Threads", 0),
        open_fds=open_fd_count(pid),
        load1=load1,
        runnable_tasks=runnable,
        net_rx_mbps=mbps(current.net_rx_bytes - previous.net_rx_bytes, elapsed),
        net_tx_mbps=mbps(current.net_tx_bytes - previous.net_tx_bytes, elapsed),
        disk_read_mbps=mbps(
            current.disk_read_bytes - previous.disk_read_bytes, elapsed
        ),
        disk_write_mbps=mbps(
            current.disk_write_bytes - previous.disk_write_bytes, elapsed
        ),
    )


def percentile(values: Iterable[float], percentile_value: float) -> float:
    items = sorted(values)
    if not items:
        return 0.0
    index = (len(items) - 1) * percentile_value / 100.0
    lower = int(index)
    upper = min(lower + 1, len(items) - 1)
    if lower == upper:
        return items[lower]
    fraction = index - lower
    return items[lower] + ((items[upper] - items[lower]) * fraction)


def summarize(values: list[float]) -> dict[str, float]:
    if not values:
        return {"avg": 0.0, "p50": 0.0, "p95": 0.0, "max": 0.0, "min": 0.0}
    return {
        "avg": statistics.fmean(values),
        "p50": percentile(values, 50),
        "p95": percentile(values, 95),
        "max": max(values),
        "min": min(values),
    }


def write_samples(path: Path, samples: list[Sample]) -> None:
    fields = list(Sample.__dataclass_fields__)
    with path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=fields)
        writer.writeheader()
        for sample in samples:
            writer.writerow(
                {
                    key: f"{value:.3f}" if isinstance(value, float) else value
                    for key, value in sample.__dict__.items()
                }
            )


def capture_journal(service: str, since_epoch: float, output_path: Path) -> str:
    since = datetime.fromtimestamp(since_epoch, UTC).strftime(
        "%Y-%m-%d %H:%M:%S UTC"
    )
    command = [
        "journalctl",
        "--user",
        "-u",
        service,
        "--since",
        since,
        "--no-pager",
        "-o",
        "cat",
    ]
    try:
        result = subprocess.run(
            command,
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
    except FileNotFoundError:
        return ""
    output_path.write_text(result.stdout, encoding="utf-8")
    return result.stdout


def parse_journal(text: str) -> dict[str, float | int | None]:
    rps_values: list[float] = []
    queue_values: list[float] = []
    failed_total = 0
    retried_total = 0
    startup_ms: list[int] = []
    for line in text.splitlines():
        metrics = METRICS_RE.search(line)
        if metrics:
            rps_values.append(float(metrics.group("rps")))
            queue_values.append(float(metrics.group("queue")))
            failed_total += int(metrics.group("failed"))
            retried_total += int(metrics.group("retried"))
        startup = STARTUP_RE.search(line)
        if startup:
            startup_ms.append(int(startup.group("total")))
    return {
        "records_per_sec_p50": percentile(rps_values, 50),
        "records_per_sec_p95": percentile(rps_values, 95),
        "queue_depth_p95": percentile(queue_values, 95),
        "queue_depth_max": max(queue_values) if queue_values else 0,
        "failed_total": failed_total,
        "retried_total": retried_total,
        "startup_total_ms_max": max(startup_ms) if startup_ms else None,
        "metrics_count": len(rps_values),
    }


def verdict(samples: list[Sample], vcpus: int, journal: dict[str, float | int | None]) -> list[str]:
    cpu = summarize([sample.host_cpu_pct for sample in samples])
    proc = summarize([sample.proc_capacity_pct for sample in samples])
    rss = summarize([sample.rss_pct for sample in samples])
    mem_available = summarize([sample.mem_available_pct for sample in samples])
    load = summarize([sample.load1 for sample in samples])
    swap_max = max((sample.swap_used_mb for sample in samples), default=0.0)

    findings: list[str] = []
    if cpu["p95"] >= 85 or proc["p95"] >= 80 or load["p95"] > vcpus * 1.25:
        findings.append(
            "CPU headroom is low. If Kafka lag grows, scale up within C8g/C7g."
        )
    elif cpu["p95"] <= 35 and proc["p95"] <= 35:
        findings.append(
            "CPU has substantial headroom at this load; a smaller instance may work."
        )
    else:
        findings.append("CPU sizing looks reasonable for the sampled load.")

    if swap_max > 0 or mem_available["min"] < 10 or rss["max"] > 80:
        findings.append("Memory headroom is low. Prefer M/R class or more RAM.")
    elif rss["max"] < 45 and mem_available["min"] > 35:
        findings.append("Memory has substantial headroom.")
    else:
        findings.append("Memory sizing looks reasonable for the sampled load.")

    if journal["metrics_count"]:
        if float(journal["queue_depth_p95"] or 0) > 0:
            findings.append(
                "Solver/search queue backed up during the sample; inspect writer retries and CPU."
            )
        if int(journal["failed_total"] or 0) > 0 or int(journal["retried_total"] or 0) > 0:
            findings.append(
                "OpenSearch bulk retries/failures occurred; scaling EC2 may not fix that bottleneck."
            )
    else:
        findings.append(
            "No Search writer metrics were found in the journal window; verify service name or run longer."
        )

    findings.append(
        "Final sizing decision still requires Kafka lag: stable/decreasing lag means enough capacity."
    )
    return findings


def report(
    samples: list[Sample],
    pid: int,
    process_name: str,
    vcpus: int,
    metadata: dict[str, str],
    journal_summary: dict[str, float | int | None],
) -> str:
    lines: list[str] = []
    lines.append("Moirai instance sizing assessment")
    lines.append("=" * 34)
    lines.append(f"Process: {process_name} pid={pid}")
    if metadata:
        lines.append(
            "EC2: "
            + " ".join(f"{key}={value}" for key, value in sorted(metadata.items()))
        )
    lines.append(f"vCPUs: {vcpus}")
    lines.append(f"Samples: {len(samples)}")
    lines.append("")

    for label, values, unit in [
        ("Host CPU", [s.host_cpu_pct for s in samples], "%"),
        ("Moirai CPU capacity", [s.proc_capacity_pct for s in samples], "%"),
        ("Moirai cores used", [s.proc_cores for s in samples], "cores"),
        ("Moirai RSS", [s.rss_mb for s in samples], "MiB"),
        ("Moirai RSS memory", [s.rss_pct for s in samples], "%"),
        ("MemAvailable", [s.mem_available_pct for s in samples], "%"),
        ("Load average 1m", [s.load1 for s in samples], ""),
        ("Network RX", [s.net_rx_mbps for s in samples], "MiB/s"),
        ("Network TX", [s.net_tx_mbps for s in samples], "MiB/s"),
        ("Disk read", [s.disk_read_mbps for s in samples], "MiB/s"),
        ("Disk write", [s.disk_write_mbps for s in samples], "MiB/s"),
    ]:
        summary = summarize(values)
        suffix = f" {unit}" if unit else ""
        lines.append(
            f"{label:22} avg={summary['avg']:.2f}{suffix} "
            f"p95={summary['p95']:.2f}{suffix} max={summary['max']:.2f}{suffix}"
        )

    lines.append("")
    lines.append("App metrics from journal")
    lines.append("-" * 24)
    if journal_summary["metrics_count"]:
        lines.append(
            f"records_per_sec p50={journal_summary['records_per_sec_p50']:.0f} "
            f"p95={journal_summary['records_per_sec_p95']:.0f}"
        )
        lines.append(
            f"queue_depth p95={journal_summary['queue_depth_p95']:.0f} "
            f"max={journal_summary['queue_depth_max']:.0f}"
        )
        lines.append(
            f"bulk failed={journal_summary['failed_total']} "
            f"retried={journal_summary['retried_total']}"
        )
    else:
        lines.append("No Search writer metrics found.")
    if journal_summary["startup_total_ms_max"] is not None:
        lines.append(f"startup_total_ms max={journal_summary['startup_total_ms_max']}")

    lines.append("")
    lines.append("Verdict")
    lines.append("-" * 7)
    lines.extend(f"- {finding}" for finding in verdict(samples, vcpus, journal_summary))
    lines.append("")
    lines.append("Interpretation")
    lines.append("- Run this through peak traffic, not only quiet periods.")
    lines.append("- If CPU is low but OpenSearch retries appear, tune/scale OpenSearch first.")
    lines.append("- If RSS or MemAvailable is tight, use a memory-heavier family.")
    lines.append("- If all metrics have large headroom and Kafka lag is flat, test one size down.")
    return "\n".join(lines) + "\n"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Assess whether the current EC2 instance is sized correctly for Moirai."
    )
    parser.add_argument("--pid", type=int, help="Moirai process id. Auto-detected by default.")
    parser.add_argument("--process-name", default="moirai", help="Process name to auto-detect.")
    parser.add_argument("--duration", type=int, default=900, help="Sampling duration in seconds.")
    parser.add_argument("--interval", type=int, default=10, help="Sampling interval in seconds.")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=None,
        help="Directory for samples.csv, journal.log, and report.txt.",
    )
    parser.add_argument(
        "--service",
        default="moirai.service",
        help="systemd user service for optional journal capture.",
    )
    parser.add_argument("--no-journal", action="store_true", help="Skip journal capture.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.duration < 1 or args.interval < 1:
        print("--duration and --interval must be positive", file=sys.stderr)
        return 2

    pid = args.pid or find_pid(args.process_name)
    if not Path(f"/proc/{pid}").exists():
        print(f"Process {pid} does not exist", file=sys.stderr)
        return 2

    output_dir = args.output_dir or Path(
        f"moirai-sizing-{datetime.now(UTC).strftime('%Y%m%dT%H%M%SZ')}"
    )
    output_dir.mkdir(parents=True, exist_ok=True)

    vcpus = os.cpu_count() or 1
    metadata = instance_metadata()
    start_wall = time.time()
    previous = capture(pid)
    samples: list[Sample] = []
    deadline = time.monotonic() + args.duration

    print(
        f"Sampling pid={pid} for {args.duration}s every {args.interval}s; output={output_dir}",
        flush=True,
    )
    while time.monotonic() < deadline:
        sleep_for = min(args.interval, max(0.0, deadline - time.monotonic()))
        time.sleep(sleep_for)
        try:
            current = capture(pid)
            sample = sample_once(pid, previous, current, vcpus)
        except OSError:
            print(f"Process {pid} exited during sampling", file=sys.stderr)
            break
        samples.append(sample)
        previous = current
        print(
            f"{sample.utc_time} cpu={sample.host_cpu_pct:.1f}% "
            f"proc={sample.proc_capacity_pct:.1f}% "
            f"rss={sample.rss_pct:.1f}% mem_avail={sample.mem_available_pct:.1f}%",
            flush=True,
        )

    if not samples:
        print("No samples collected", file=sys.stderr)
        return 1

    write_samples(output_dir / "samples.csv", samples)
    journal_text = ""
    if not args.no_journal:
        journal_text = capture_journal(args.service, start_wall, output_dir / "journal.log")
    journal_summary = parse_journal(journal_text)
    text = report(samples, pid, args.process_name, vcpus, metadata, journal_summary)
    (output_dir / "report.txt").write_text(text, encoding="utf-8")
    print()
    print(text)
    print(f"Wrote {output_dir / 'samples.csv'}")
    if not args.no_journal:
        print(f"Wrote {output_dir / 'journal.log'}")
    print(f"Wrote {output_dir / 'report.txt'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
