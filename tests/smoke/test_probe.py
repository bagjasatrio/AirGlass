#!/usr/bin/env python3
"""Smoke test: `airglass --probe` must answer before any server starts.

Runs the release binary, parses the key/value report, and asserts the contract
the pre-flight UI depends on. No iPhone, no network, no admin rights needed.

Usage:  python tests/smoke/test_probe.py [path-to-airglass.exe]
Exit 0 = pass, 1 = fail with the offending line printed.
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

DEFAULT_EXE = Path("build/Release/airglass.exe")
REQUIRED_KEYS = ("gpu", "hw_decode", "cpu_cores", "ram_mb", "wifi", "diagnosis")
EXPECTED_PRESETS = {"720p60", "1080p30", "1080p60", "auto"}
VALID_VERDICTS = {"recommended", "possible", "risky"}


def main() -> int:
    exe = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_EXE
    if not exe.exists():
        print(f"FAIL: binary not found: {exe}")
        return 1

    proc = subprocess.run([str(exe), "--probe"], capture_output=True, text=True, timeout=60)
    out = proc.stdout or ""
    if proc.returncode != 0:
        print(f"FAIL: exit code {proc.returncode}\nstderr:\n{proc.stderr}")
        return 1

    failures: list[str] = []
    seen = {}
    for line in out.splitlines():
        if not line.strip():
            continue
        parts = line.split(None, 1)
        if len(parts) == 2 and parts[0] in REQUIRED_KEYS + ("preset",):
            seen.setdefault(parts[0], []).append(parts[1].strip())

    for key in REQUIRED_KEYS:
        if key not in seen:
            failures.append(f"missing key: {key}")

    presets = {}
    for entry in seen.get("preset", []):
        m = re.match(r"(?P<name>\S+)\s+(?P<verdict>\S+)", entry)
        if not m:
            failures.append(f"unparseable preset line: {entry!r}")
            continue
        presets[m["name"]] = m["verdict"]
        if m["verdict"] not in VALID_VERDICTS:
            failures.append(f"bad verdict for {m['name']}: {m['verdict']}")

    if set(presets) != EXPECTED_PRESETS:
        failures.append(f"preset set mismatch: got {sorted(presets)}, want {sorted(EXPECTED_PRESETS)}")

    if "hw_decode" in seen:
        hw = seen["hw_decode"][0]
        if not re.search(r"d3d11va=[01] nvdec=[01] qsv=[01]", hw):
            failures.append(f"malformed hw_decode line: {hw!r}")

    if failures:
        print("FAIL")
        for f in failures:
            print(f"  - {f}")
        print("\n--- raw output ---")
        print(out)
        return 1

    print(f"PASS: probe answered with {len(presets)} presets")
    print(out.rstrip())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
