#!/usr/bin/env python3
"""Convert closed Moirai audit NDJSON files to parquet and upload them to S3."""

from __future__ import annotations

import os
import shutil
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


def claim_ready_files() -> list[Path]:
    AUDIT_DIR.mkdir(parents=True, exist_ok=True)
    files = sorted(path for path in AUDIT_DIR.glob("*.jsonl") if path.is_file())
    claimed: list[Path] = []
    for path in files:
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


def write_parquet(spark: SparkSession, sources: list[Path], output_dir: Path) -> None:
    dataframes: DataFrame | None = None

    for source in sources:
        try:
            dataframe = dataframe_for_file(spark, source)
        except Exception as exc:
            print(f"Error processing file {source}: {exc}")
            continue
        if dataframe is None:
            continue
        dataframes = dataframe if dataframes is None else dataframes.unionByName(
            dataframe, allowMissingColumns=True
        )

    if dataframes is None or dataframes.rdd.isEmpty():
        return

    dataframe = flatten_structs(dataframes)
    dataframe.coalesce(COALESCE).write.mode("append").partitionBy("ad").parquet(
        str(output_dir.resolve())
    )


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


def archive_sources(sources: list[Path]) -> None:
    archive_dir = WORK_DIR / "processed" / datetime.now(UTC).strftime("%Y-%m-%d")
    archive_dir.mkdir(parents=True, exist_ok=True)
    for path in sources:
        target = archive_dir / path.name.replace(".jsonl.processing", ".jsonl")
        shutil.move(str(path), target)


def main() -> int:
    if COALESCE <= 0:
        raise SystemExit("DWH_PARQUET_COALESCE must be positive")
    WORK_DIR.mkdir(parents=True, exist_ok=True)
    sources = claim_ready_files()
    if not sources:
        print("No closed audit files to export")
        return 0

    with TemporaryDirectory(dir=WORK_DIR) as tmpdir:
        tmp_root = Path(tmpdir)
        output_dir = tmp_root / "parquet"
        spark = build_spark(tmp_root)
        try:
            write_parquet(spark, sources, output_dir)
        finally:
            spark.stop()
        uploaded = upload_parquet(output_dir)

    archive_sources(sources)
    print(f"Uploaded {uploaded} parquet files from {len(sources)} audit files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
