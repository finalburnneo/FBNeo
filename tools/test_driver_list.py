#!/usr/bin/env python3
"""Build each Meson Burn driver independently and report failures."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BUILD_ROOT = ROOT / "build-driver-tests"
DEFAULT_LOG_DIR = DEFAULT_BUILD_ROOT / "logs"


def parse_default_driver_list(options_file: Path) -> list[str]:
    text = options_file.read_text(encoding="utf-8")
    match = re.search(r"option\(\s*'driver_list'.*?value\s*:\s*\[(.*?)\]", text, re.S)
    if not match:
        raise RuntimeError(
            "Could not find driver_list default value in meson_options.txt"
        )
    return re.findall(r"'([^']+)'", match.group(1))


def run_command(
    command: list[str], cwd: Path, log_file: Path, timeout: int | None
) -> int:
    start = time.monotonic()
    with log_file.open("w", encoding="utf-8", errors="replace") as log:
        log.write("$ " + " ".join(command) + "\n\n")
        log.flush()
        try:
            proc = subprocess.run(
                command,
                cwd=cwd,
                stdout=log,
                stderr=subprocess.STDOUT,
                timeout=timeout,
                text=True,
            )
            code = proc.returncode
        except subprocess.TimeoutExpired:
            code = 124
            log.write(f"\nTIMEOUT after {timeout} seconds\n")
        log.write(f"\nExit code: {code}\nElapsed: {time.monotonic() - start:.1f}s\n")
    return code


def tail_log(path: Path, lines: int) -> str:
    if lines <= 0 or not path.exists():
        return ""
    content = path.read_text(encoding="utf-8", errors="replace").splitlines()
    return "\n".join(content[-lines:])


def test_driver(args: argparse.Namespace, driver: str) -> dict[str, object]:
    build_dir = args.build_root / driver
    log_dir = args.log_dir
    setup_log = log_dir / f"{driver}.setup.log"
    build_log = log_dir / f"{driver}.build.log"

    if args.clean and build_dir.exists():
        shutil.rmtree(build_dir)

    setup_command = [
        args.meson,
        "setup",
        str(build_dir),
        "--wipe",
        f"-Dfrontend={args.frontend}",
        f"-Dbuildtype={args.buildtype}",
        f"-Ddriver_list={driver}",
    ]
    setup_command.extend(args.setup_arg)

    setup_code = run_command(setup_command, ROOT, setup_log, args.timeout)
    result: dict[str, object] = {
        "driver": driver,
        "build_dir": str(build_dir),
        "setup_log": str(setup_log),
        "build_log": str(build_log),
        "setup_code": setup_code,
        "build_code": None,
        "ok": False,
    }
    if setup_code != 0:
        result["stage"] = "setup"
        return result

    if args.setup_only:
        result["ok"] = True
        result["stage"] = "setup"
        return result

    build_command = [
        args.meson,
        "compile",
        "-C",
        str(build_dir),
        "-j",
        str(args.jobs),
        args.target,
    ]
    build_code = run_command(build_command, ROOT, build_log, args.timeout)
    result["build_code"] = build_code
    result["stage"] = "build"
    result["ok"] = build_code == 0
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "drivers",
        nargs="*",
        help="Drivers to test; default tests every driver in driver_list",
    )
    parser.add_argument("--build-root", type=Path, default=DEFAULT_BUILD_ROOT)
    parser.add_argument("--log-dir", type=Path, default=DEFAULT_LOG_DIR)
    parser.add_argument("--frontend", default="auto")
    parser.add_argument("--buildtype", default="debug")
    parser.add_argument(
        "--target", default="burn_drv", help="Meson target to compile for each driver"
    )
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument(
        "--timeout", type=int, default=None, help="Per-command timeout in seconds"
    )
    parser.add_argument("--meson", default=os.environ.get("MESON", "meson"))
    parser.add_argument(
        "--setup-arg",
        action="append",
        default=[],
        help="Extra argument passed to meson setup",
    )
    parser.add_argument(
        "--setup-only", action="store_true", help="Only configure each driver"
    )
    parser.add_argument(
        "--no-clean",
        dest="clean",
        action="store_false",
        help="Do not remove per-driver build dirs first",
    )
    parser.add_argument(
        "--tail", type=int, default=40, help="Failure log lines to print"
    )
    args = parser.parse_args()

    all_drivers = parse_default_driver_list(ROOT / "meson_options.txt")
    drivers = args.drivers or all_drivers
    unknown = [driver for driver in drivers if driver not in all_drivers]
    if unknown:
        print("Unknown drivers: " + ", ".join(unknown), file=sys.stderr)
        return 2

    args.build_root.mkdir(parents=True, exist_ok=True)
    args.log_dir.mkdir(parents=True, exist_ok=True)

    results: list[dict[str, object]] = []
    start = time.monotonic()
    for index, driver in enumerate(drivers, 1):
        print(f"[{index}/{len(drivers)}] Testing {driver}...", flush=True)
        result = test_driver(args, driver)
        results.append(result)
        status = "PASS" if result["ok"] else "FAIL"
        print(f"[{index}/{len(drivers)}] {driver}: {status}", flush=True)
        if not result["ok"] and args.tail > 0:
            failed_log = Path(
                str(
                    result["build_log"]
                    if result.get("build_code") is not None
                    else result["setup_log"]
                )
            )
            print(f"--- tail {failed_log} ---")
            print(tail_log(failed_log, args.tail))
            print("--- end tail ---")

    summary = {
        "ok": all(bool(result["ok"]) for result in results),
        "elapsed_seconds": round(time.monotonic() - start, 1),
        "total": len(results),
        "passed": sum(1 for result in results if result["ok"]),
        "failed": [result for result in results if not result["ok"]],
        "results": results,
    }
    summary_path = args.log_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")

    print()
    print(f"Passed: {summary['passed']}/{summary['total']}")
    print(f"Summary: {summary_path}")
    if summary["failed"]:
        print(
            "Failed drivers: "
            + ", ".join(str(result["driver"]) for result in summary["failed"])
        )
    return 0 if summary["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
