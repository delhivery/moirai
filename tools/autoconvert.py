#!/usr/bin/env python3
"""Compatibility wrapper for the Moirai DWH parquet exporter."""

from __future__ import annotations

import runpy
from pathlib import Path


if __name__ == "__main__":
    runpy.run_path(
        str(Path(__file__).with_name("export_audit_to_parquet.py")),
        run_name="__main__",
    )
