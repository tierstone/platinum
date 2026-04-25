from __future__ import annotations

import argparse
import subprocess
import sys

from .paths import TEST_LOG_DIR


MODE_RULES = {
    "off": {
        "build_args": (),
        "required": ("sched ok", "worker"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail"),
    },
    "c": {
        "build_args": (),
        "required": ("sched ok", "user init", "U"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail"),
    },
    "elf": {
        "build_args": (),
        "required": ("sched ok", "user init", "U"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail"),
    },
    "elf-pulse": {
        "build_args": ("--user-program", "pulse"),
        "required": ("sched ok", "user init", "P"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail"),
    },
    "bad-syscall": {
        "build_args": (),
        "required": ("sched ok", "user init", "V"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail"),
    },
    "bad-elf": {
        "build_args": (),
        "required": ("user elf fail",),
        "forbidden": ("trap ", "exit fail", "user as fail", "tss stack fail"),
    },
    "yield-stress": {
        "build_args": (),
        "required": ("sched ok", "user init", "Y"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail"),
    },
    "bad-bootstrap": {
        "build_args": (),
        "required": ("sched bootstrap fail",),
        "forbidden": ("trap ", "exit fail", "user as fail", "user elf fail", "tss stack fail"),
    },
    "fd-write": {
        "build_args": (),
        "required": ("sched ok", "user init", "FD"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail", "!"),
    },
    "fd-read": {
        "build_args": (),
        "required": ("sched ok", "user init", "@"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail", "!"),
    },
    "open-read": {
        "build_args": (),
        "required": ("sched ok", "user init", "Op"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail", "!"),
    },
    "open-flags": {
        "build_args": (),
        "required": ("sched ok", "user init", "A"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail", "!"),
    },
    "exec-elf": {
        "build_args": ("--user-program", "pulse"),
        "required": ("sched ok", "user init", "P"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail", "!"),
    },
    "dup-full": {
        "build_args": (),
        "required": ("sched ok", "user init", "dupfull ok"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail", "!"),
    },
    "bad-pointers": {
        "build_args": (),
        "required": ("sched ok", "user init", "ptr ok"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail", "!"),
    },
    "exec-loop": {
        "build_args": ("--user-program", "pulse"),
        "required": ("sched ok", "user init", "execloop ok"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail", "!"),
    },
    "exec-bad-loop": {
        "build_args": (),
        "required": ("sched ok", "user init", "execbad ok"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail", "!"),
    },
    "exec-transfer-fail": {
        "build_args": (),
        "required": ("sched ok", "user init", "execxfer ok"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail", "!"),
    },
    "exec-registry": {
        "build_args": (),
        "required": ("sched ok", "user init", "execreg ok"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail", "!"),
    },
    "exec-paths": {
        "build_args": (),
        "required": ("sched ok", "user init", "execpath ok"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail", "!"),
    },
    "exec-root": {
        "build_args": (),
        "required": ("sched ok", "user init", "execroot ok"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail", "!"),
    },
    "exec-noent": {
        "build_args": (),
        "required": ("sched ok", "user init", "execnoent ok"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail", "!"),
    },
    "exec-nonexec": {
        "build_args": (),
        "required": ("sched ok", "user init", "execnonexec ok"),
        "forbidden": ("trap ", "fail", "exit fail", "user as fail", "user elf fail", "tss stack fail", "!"),
    },
}


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
    for marker in MODE_RULES[mode]["required"]:
        if marker not in output:
            raise RuntimeError(f"missing marker: {marker}")


def reject_markers(mode: str, output: str) -> None:
    lowered_lines = [line.lower().strip() for line in output.splitlines()]
    for marker in MODE_RULES[mode]["forbidden"]:
        lowered_marker = marker.lower()
        if lowered_marker.endswith(" "):
            for line in lowered_lines:
                if line.startswith(lowered_marker.strip()):
                    raise RuntimeError(f"unexpected marker: {marker}")
        else:
            for line in lowered_lines:
                if lowered_marker in line:
                    raise RuntimeError(f"unexpected marker: {marker}")


def run_mode(mode: str, timeout_seconds: float, iteration: int) -> None:
    TEST_LOG_DIR.mkdir(parents=True, exist_ok=True)
    log_path = TEST_LOG_DIR / f"{mode}-iteration-{iteration}.log"
    if log_path.exists():
        log_path.unlink()

    build_command = ["python3", "manage.py", "build", "--user-init", "elf" if mode == "elf-pulse" else mode]
    build_command.extend(MODE_RULES[mode]["build_args"])

    build_code, build_output = run_command(build_command, timeout_seconds)
    if build_code != 0:
        raise RuntimeError(
            f"build failed for mode={mode} iteration={iteration}\n{build_output}"
        )

    boot_code, boot_output = run_command(
        ["timeout", f"{timeout_seconds}s", "python3", "manage.py", "boot", "--reset-vars", "--serial-log", str(log_path)],
        timeout_seconds + 1.0,
    )
    if boot_code not in (0, 124):
        raise RuntimeError(
            f"boot failed for mode={mode} iteration={iteration} code={boot_code}\n{boot_output}"
        )

    serial_output = ""
    if log_path.exists():
        serial_output = log_path.read_text(encoding="utf-8", errors="replace")

    try:
        require_markers(mode, serial_output)
        reject_markers(mode, serial_output)
    except RuntimeError as exc:
        raise RuntimeError(
            f"mode={mode} iteration={iteration}\n{exc}\nlog={log_path}\n{serial_output}\n{boot_output}"
        ) from exc


def verify(mode: str, loops: int, timeout: float) -> int:
    if loops < 1:
        print("loops must be >= 1", file=sys.stderr)
        return 1
    if timeout <= 0:
        print("timeout must be > 0", file=sys.stderr)
        return 1

    if mode == "all":
        modes = ("off", "c", "elf", "elf-pulse", "yield-stress", "bad-syscall", "bad-elf", "bad-bootstrap", "fd-write", "fd-read", "open-read", "open-flags", "exec-elf", "dup-full", "bad-pointers", "exec-loop", "exec-bad-loop", "exec-transfer-fail", "exec-registry", "exec-paths", "exec-root", "exec-noent", "exec-nonexec")
    else:
        modes = (mode,)

    try:
        counts = {entry: 0 for entry in modes}
        for iteration in range(1, loops + 1):
            for entry in modes:
                run_mode(entry, timeout, iteration)
                counts[entry] += 1
            if loops > 1:
                print(f"iteration {iteration}/{loops} ok")
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    for entry in modes:
        print(f"{entry}: {counts[entry]} ok")

    return 0


def testing_main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--mode",
        choices=["all"] + list(MODE_RULES.keys()),
        default="all",
    )
    parser.add_argument("--loops", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=5.0)
    args = parser.parse_args(argv)
    return verify(args.mode, args.loops, args.timeout)
