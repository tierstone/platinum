#!/usr/bin/env python3
from __future__ import annotations

import argparse
import subprocess
import sys


MODES = {
    "off": ("sched ok", "worker"),
    "c": ("sched ok", "user init", "U"),
    "elf": ("sched ok", "user init", "U"),
}

NEGATIVE_MARKERS = ("trap", "fail")


def text_output(data: str | bytes | None) -> str:
    if data is None:
        return ""
    if isinstance(data, bytes):
        return data.decode("utf-8", errors="replace")
    return data


def run_command(command: list[str], timeout_seconds: float) -> tuple[int, str]:
    try:
        result = subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
            timeout=timeout_seconds,
        )
        return result.returncode, result.stdout + result.stderr
    except subprocess.TimeoutExpired as exc:
        output = text_output(exc.stdout) + text_output(exc.stderr)
        return 124, output


def require_markers(mode: str, output: str) -> None:
    for marker in MODES[mode]:
        if marker not in output:
            raise RuntimeError(f"missing marker: {marker}")


def reject_markers(output: str) -> None:
    lowered = output.lower()
    for marker in NEGATIVE_MARKERS:
        if marker in lowered:
            raise RuntimeError(f"unexpected marker: {marker}")


def run_mode(mode: str, timeout_seconds: float, iteration: int) -> None:
    build_code, build_output = run_command(
        ["python3", "build.py", "all", "--user-init", mode],
        timeout_seconds,
    )
    if build_code != 0:
        raise RuntimeError(
            f"build failed for mode={mode} iteration={iteration}\n{build_output}"
        )

    boot_code, boot_output = run_command(
        ["python3", "boot.py"],
        timeout_seconds,
    )
    if boot_code not in (0, 124):
        raise RuntimeError(
            f"boot failed for mode={mode} iteration={iteration} code={boot_code}\n{boot_output}"
        )

    try:
        require_markers(mode, boot_output)
        reject_markers(boot_output)
    except RuntimeError as exc:
        raise RuntimeError(
            f"mode={mode} iteration={iteration}\n{exc}\n{boot_output}"
        ) from exc


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--loops", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=5.0)
    args = parser.parse_args()

    if args.loops < 1:
        print("loops must be >= 1", file=sys.stderr)
        return 1
    if args.timeout <= 0:
        print("timeout must be > 0", file=sys.stderr)
        return 1

    try:
        for iteration in range(1, args.loops + 1):
            for mode in ("off", "c", "elf"):
                run_mode(mode, args.timeout, iteration)
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
