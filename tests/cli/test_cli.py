#!/usr/bin/env python3
"""Contract tests for the AirGlass command line.

The receiver is a GUI subsystem binary, so before this test existed a rejected
argument vanished without a word: no message, no non-zero exit, and the app just
started with defaults. These cases pin the behaviour that replaced it.

Run:  python tests/cli/test_cli.py [path-to-airglass.exe]
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_EXE = ROOT / "build" / "Release" / "airglass.exe"

BAD_INPUTS = [
    (["--preset", "nonsense"], "--preset"),
    (["--preset"], "--preset needs a value"),
    (["--mirror-res", "9999x9999"], "--mirror-res out of range"),
    (["--mirror-res", "abc"], "--mirror-res expects WxH"),
    (["--mirror-res"], "--mirror-res needs a value"),
    (["--bogus-flag"], "Unknown option"),
    (["--detach", "--nope"], "Unknown option"),
]

GOOD_INPUTS = [
    ["--probe"],
    ["--probe", "--preset", "1080p30"],
    ["--probe", "--mirror-res", "640x480"],
]


def run(exe: Path, args: list[str], timeout: int = 20) -> subprocess.CompletedProcess:
    return subprocess.run(
        [str(exe), *args],
        capture_output=True,
        text=True,
        timeout=timeout,
        cwd=str(ROOT),
    )


def main() -> int:
    exe = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_EXE
    if not exe.exists():
        print(f"FAIL: executable not found: {exe}")
        return 1

    failures = 0

    for args, needle in BAD_INPUTS:
        result = run(exe, args)
        ok = result.returncode == 2 and needle.lower() in (result.stdout + result.stderr).lower()
        print(
            f"{'PASS' if ok else 'FAIL'}: {' '.join(args)!r} -> exit {result.returncode}, "
            f"message {'found' if needle in (result.stdout + result.stderr) else 'missing'}"
        )
        if not ok:
            failures += 1

    for args in GOOD_INPUTS:
        result = run(exe, args)
        ok = result.returncode == 0 and "gpu" in result.stdout.lower()
        print(f"{'PASS' if ok else 'FAIL'}: {' '.join(args)!r} -> exit {result.returncode}")
        if not ok:
            failures += 1

    # --help must stay a pure, exit-0 path.
    help_run = run(exe, ["--help"])
    ok = help_run.returncode == 0 and "--preset" in help_run.stdout
    print(f"{'PASS' if ok else 'FAIL'}: --help -> exit {help_run.returncode}")
    if not ok:
        failures += 1

    print(f"\n{'PASS' if failures == 0 else 'FAIL'}: {len(BAD_INPUTS) + len(GOOD_INPUTS) + 1} cases, {failures} failed")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
