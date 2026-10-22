#!/usr/bin/env python3
"""Soak test: watch Glasscast's process while it runs.

Samples RSS, CPU% and thread count every interval and fails if the process
crosses the design budget (RSS > 250 MB, sustained CPU above the software-decode
ceiling). Written against the design doc numbers, not invented ones:

    RAM   < 250 MB RSS          (design §3)
    CPU   <= 60% of one core while software-decoding 1080p30, <= 15% total with
          hardware decode (design §3)

A/V skew and drop rate cannot be sampled from outside the process yet; the stats
overlay reports them and the audit document records them by hand.

Usage:
    python docs/qa/soak-test.py --exe build/Release/glasscast.exe --minutes 30
"""

from __future__ import annotations

import argparse
import csv
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

try:
    import psutil
except ImportError:
    print("needs psutil: pip install psutil")
    raise SystemExit(2)

RSS_BUDGET_MB = 250
CPU_BUDGET_PCT = 60.0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default="build/Release/glasscast.exe")
    ap.add_argument("--minutes", type=float, default=30)
    ap.add_argument("--interval", type=float, default=10)
    ap.add_argument("--preset", default="1080p30")
    ap.add_argument("--out", default="docs/qa/soak.csv")
    args = ap.parse_args()

    exe = Path(args.exe)
    if not exe.exists():
        print(f"FAIL: missing {exe}")
        return 1

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)

    proc = subprocess.Popen([str(exe), "--preset", args.preset])
    time.sleep(3)
    try:
        ps = psutil.Process(proc.pid)
    except psutil.NoSuchProcess:
        print("FAIL: process exited immediately")
        return 1

    cores = psutil.cpu_count(logical=True) or 1
    deadline = time.time() + args.minutes * 60
    failures: list[str] = []

    with out.open("w", newline="") as fh:
        writer = csv.writer(fh)
        writer.writerow(["timestamp", "rss_mb", "cpu_pct_of_one_core", "threads", "handles"])
        while time.time() < deadline and proc.poll() is None:
            try:
                info = ps.as_dict(attrs=["memory_info", "num_threads", "num_handles"])
                cpu = ps.cpu_percent(interval=1.0) * cores  # percent of one core
            except psutil.NoSuchProcess:
                break
            rss_mb = info["memory_info"].rss / (1024 * 1024)
            writer.writerow([datetime.now().isoformat(timespec="seconds"),
                             round(rss_mb, 1), round(cpu, 1),
                             info["num_threads"], info.get("num_handles", 0)])
            fh.flush()
            if rss_mb > RSS_BUDGET_MB:
                failures.append(f"RSS {rss_mb:.0f} MB > {RSS_BUDGET_MB} MB")
            if cpu > CPU_BUDGET_PCT:
                failures.append(f"CPU {cpu:.0f}% of one core > {CPU_BUDGET_PCT:.0f}%")
            time.sleep(max(0.0, args.interval - 1.0))

    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()

    print(f"wrote {out}")
    if failures:
        print("FAIL")
        for f in dict.fromkeys(failures):
            print(f"  - {f}")
        return 1
    print("PASS: stayed inside the RSS/CPU budget")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
