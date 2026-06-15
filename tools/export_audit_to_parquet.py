#!/usr/bin/env python3
"""Convert closed Moirai audit NDJSON files to parquet and upload them to S3."""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
from datetime import UTC, datetime
from pathlib import Path
from tempfile import TemporaryDirectory
from time import sleep

try:
    import boto3
    from pyspark.conf import SparkConf
    from pyspark.sql import DataFrame, SparkSession, functions
except ImportError as exc:  # pragma: no cover - exercised on deployment hosts
    raise SystemExit(
        "Install export dependencies: python3 -m pip install --user boto3 pyspark"
    ) from exc


def env_path(names: tuple[str, ...], fallback: str) -> Path:
    for name in names:
        value = os.environ.get(name)
        if value:
            return Path(value).expanduser()
    return Path(fallback).expanduser()


def env_string(names: tuple[str, ...], fallback: str = "") -> str:
    for name in names:
        value = os.environ.get(name)
        if value:
            return value
    return fallback


AUDIT_DIR = env_path(("DWH_AUDIT_DIR", "LOG_DIR"), "~/.local/state/moirai/audit")
WORK_DIR = env_path(("DWH_EXPORT_WORK_DIR",), "~/.local/state/moirai/dwh-export")
BUCKET = env_string(("DWH_BUCKET", "STORAGE"), "dwh-prod-datalake")
PREFIX = env_string(("DWH_PREFIX", "OUT_PFX"), "expectedpath").strip("/")
COALESCE = int(os.environ.get("DWH_PARQUET_COALESCE", "50"))
UPLOAD_RETRY_SECONDS = int(os.environ.get("DWH_UPLOAD_RETRY_SECONDS", "60"))
PROCESSING_STALE_SECONDS = int(
    os.environ.get("DWH_PROCESSING_STALE_SECONDS", "1800")
)
MAX_FILES_PER_RUN = int(os.environ.get("DWH_EXPORT_MAX_FILES_PER_RUN", "0"))
PROCESSED_RETENTION_SECONDS = int(
    os.environ.get("DWH_PROCESSED_RETENTION_SECONDS", "-1")
)
SUPPORTED_JAVA_MAJORS = {17, 21}
JAVA_VERSION_RE = re.compile(r'version "(\d+)(?:\.(\d+))?')


def flatten_structs(nested_df: DataFrame) -> DataFrame:
    stack = [(tuple(), nested_df)]
    columns = []

    while stack:
        parents, dataframe = stack.pop()
        flat_cols = [
            functions.col(".".join(parents + (column,))).alias(
                "_".join(parents + (column,))
            )
            for column, dtype in dataframe.dtypes
            if dtype[:6] != "struct"
        ]
        nested_cols = [
            column for column, dtype in dataframe.dtypes if dtype[:6] == "struct"
        ]
        columns.extend(flat_cols)

        for nested_col in nested_cols:
            stack.append((parents + (nested_col,), dataframe.select(nested_col + ".*")))

    return nested_df.select(columns)


def bool_env(name: str) -> bool:
    return os.environ.get(name, "").strip().lower() in {"1", "true", "yes", "on"}


def java_binary() -> str:
    java_home = os.environ.get("JAVA_HOME")
    if java_home:
        return str(Path(java_home).expanduser() / "bin" / "java")
    return "java"


def java_major_version() -> int:
    try:
        result = subprocess.run(
            [java_binary(), "-version"],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
    except FileNotFoundError as exc:
        raise SystemExit(
            "Java runtime not found. Install java-17-openjdk-headless or "
            "java-21-openjdk-headless and set JAVA_HOME."
        ) from exc

    match = JAVA_VERSION_RE.search(result.stdout)
    if match is None:
        raise SystemExit(f"Unable to parse Java version from: {result.stdout.strip()}")
    major = int(match.group(1))
    if major == 1 and match.group(2) is not None:
        return int(match.group(2))
    return major


def validate_java_runtime() -> None:
    if bool_env("DWH_ALLOW_UNSUPPORTED_JAVA"):
        return
    major = java_major_version()
    if major not in SUPPORTED_JAVA_MAJORS:
        supported = ", ".join(str(version) for version in sorted(SUPPORTED_JAVA_MAJORS))
        raise SystemExit(
            f"Unsupported Java major version {major} for Spark/Hadoop export. "
            f"Use Java {supported}, set JAVA_HOME, or set "
            "DWH_ALLOW_UNSUPPORTED_JAVA=true to bypass."
        )


def claim_ready_files() -> list[Path]:
    AUDIT_DIR.mkdir(parents=True, exist_ok=True)
    claimed: list[Path] = []

    def can_claim_more() -> bool:
        return MAX_FILES_PER_RUN == 0 or len(claimed) < MAX_FILES_PER_RUN

    if PROCESSING_STALE_SECONDS > 0:
        cutoff = datetime.now(UTC).timestamp() - PROCESSING_STALE_SECONDS
        for path in sorted(AUDIT_DIR.glob("*.jsonl.processing")):
            if not can_claim_more():
                return claimed
            if path.is_file() and path.stat().st_mtime < cutoff:
                claimed.append(path)

    files = sorted(path for path in AUDIT_DIR.glob("*.jsonl") if path.is_file())
    for path in files:
        if not can_claim_more():
            break
        claimed_path = path.with_suffix(path.suffix + ".processing")
        try:
            path.rename(claimed_path)
        except FileNotFoundError:
            continue
        claimed.append(claimed_path)
    return claimed


def build_spark(tmp_root: Path) -> SparkSession:
    config = SparkConf()
    config.set("spark.app.name", "moirai-dwh-export")
    config.set("spark.executor.memory", os.environ.get("DWH_SPARK_EXECUTOR_MEMORY", "20g"))
    config.set("spark.driver.memory", os.environ.get("DWH_SPARK_DRIVER_MEMORY", "10g"))
    config.set("spark.local.dir", str(tmp_root / "spark"))
    return SparkSession.builder.config(conf=config).getOrCreate()


def dataframe_for_file(spark: SparkSession, path: Path) -> DataFrame | None:
    dataframe = spark.read.option("mode", "DROPMALFORMED").json(str(path.resolve()))
    if dataframe.rdd.isEmpty():
        return None
    if "index" in dataframe.columns:
        dataframe = dataframe.filter(functions.col("index").isNull()).drop("index")
    if dataframe.rdd.isEmpty():
        return None
    if "updated_at" not in dataframe.columns:
        raise RuntimeError(f"audit records in {path} do not contain updated_at")
    return dataframe.withColumn(
        "ad",
        functions.substring(functions.col("updated_at"), 1, 10),
    )


def write_parquet(
    spark: SparkSession, sources: list[Path], output_dir: Path
) -> tuple[list[Path], list[Path]]:
    dataframes: DataFrame | None = None
    processed_sources: list[Path] = []
    failed_sources: list[Path] = []

    for source in sources:
        try:
            dataframe = dataframe_for_file(spark, source)
        except Exception as exc:
            print(f"Error processing file {source}: {exc}")
            failed_sources.append(source)
            continue
        processed_sources.append(source)
        if dataframe is None:
            continue
        dataframes = dataframe if dataframes is None else dataframes.unionByName(
            dataframe, allowMissingColumns=True
        )
        if len(processed_sources) % 100 == 0:
            print(
                f"Read {len(processed_sources)} of {len(sources)} audit files",
                flush=True,
            )

    if failed_sources and not processed_sources:
        raise RuntimeError(
            f"Failed to process all {len(failed_sources)} audit files; "
            "leaving claimed files for retry"
        )

    if dataframes is None or dataframes.rdd.isEmpty():
        return processed_sources, failed_sources

    dataframe = flatten_structs(dataframes)
    dataframe.coalesce(COALESCE).write.mode("append").partitionBy("ad").parquet(
        str(output_dir.resolve())
    )
    return processed_sources, failed_sources


def upload_parquet(output_dir: Path) -> int:
    if os.environ.get("ENV", "prod") != "prod":
        return 0

    client = boto3.Session().client("s3")
    uploaded = 0
    for path in output_dir.glob("**/*.parquet"):
        relative = path.relative_to(output_dir)
        key = f"{PREFIX}/{relative.as_posix()}"
        while True:
            try:
                client.upload_file(
                    Filename=str(path.resolve()),
                    Bucket=BUCKET,
                    Key=key,
                    ExtraArgs={"ACL": "bucket-owner-full-control"},
                )
                break
            except Exception as exc:
                print(f"Error uploading {path} to s3://{BUCKET}/{key}: {exc}")
                sleep(UPLOAD_RETRY_SECONDS)
        uploaded += 1
    return uploaded


def cleanup_processed_archives(retention_seconds: int) -> int:
    processed_dir = WORK_DIR / "processed"
    if retention_seconds < 0 or not processed_dir.exists():
        return 0

    cutoff = datetime.now(UTC).timestamp() - retention_seconds
    deleted = 0
    for path in processed_dir.glob("**/*.jsonl"):
        if path.is_file() and path.stat().st_mtime <= cutoff:
            path.unlink()
            deleted += 1

    directories = sorted(
        (path for path in processed_dir.glob("**") if path.is_dir()),
        key=lambda path: len(path.parts),
        reverse=True,
    )
    for path in directories:
        try:
            path.rmdir()
        except OSError:
            continue
    return deleted


def finalize_sources(sources: list[Path]) -> None:
    if PROCESSED_RETENTION_SECONDS == 0:
        for path in sources:
            path.unlink(missing_ok=True)
        print(f"Deleted {len(sources)} processed audit source files", flush=True)
        return

    archive_dir = WORK_DIR / "processed" / datetime.now(UTC).strftime("%Y-%m-%d")
    archive_dir.mkdir(parents=True, exist_ok=True)
    for path in sources:
        target = archive_dir / path.name.replace(".jsonl.processing", ".jsonl")
        shutil.move(str(path), target)
    deleted = cleanup_processed_archives(PROCESSED_RETENTION_SECONDS)
    print(
        f"Archived {len(sources)} processed audit source files; "
        f"deleted {deleted} expired archive files",
        flush=True,
    )


def main() -> int:
    if COALESCE <= 0:
        raise SystemExit("DWH_PARQUET_COALESCE must be positive")
    if PROCESSING_STALE_SECONDS < 0:
        raise SystemExit("DWH_PROCESSING_STALE_SECONDS must be non-negative")
    if MAX_FILES_PER_RUN < 0:
        raise SystemExit("DWH_EXPORT_MAX_FILES_PER_RUN must be non-negative")
    if PROCESSED_RETENTION_SECONDS < -1:
        raise SystemExit("DWH_PROCESSED_RETENTION_SECONDS must be -1 or greater")
    validate_java_runtime()
    WORK_DIR.mkdir(parents=True, exist_ok=True)
    sources = claim_ready_files()
    if not sources:
        print("No closed audit files to export")
        return 0
    print(f"Claimed {len(sources)} audit files for export", flush=True)

    with TemporaryDirectory(dir=WORK_DIR) as tmpdir:
        tmp_root = Path(tmpdir)
        output_dir = tmp_root / "parquet"
        spark = build_spark(tmp_root)
        try:
            processed_sources, failed_sources = write_parquet(spark, sources, output_dir)
        finally:
            spark.stop()
        uploaded = upload_parquet(output_dir)

    finalize_sources(processed_sources)
    print(f"Uploaded {uploaded} parquet files from {len(processed_sources)} audit files")
    if failed_sources:
        print(
            f"Left {len(failed_sources)} failed audit files in processing state for retry"
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
