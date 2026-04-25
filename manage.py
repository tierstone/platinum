#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shutil
import sys

from tools import build, build_main, boot_main, clean, testing_main
from tools.buildsys import USER_PROGRAM_SOURCES
from tools.paths import OVMF_CODE, OVMF_VARS_SRC, TARGET_EFI
from tools.testing import MODE_RULES, verify


def doctor() -> int:
    missing: list[str] = []

    for command in ("clang", "lld-link", "ld.lld", "qemu-system-x86_64"):
        if shutil.which(command) is None:
            missing.append(command)

    for label, path in (
        ("firmware", OVMF_CODE),
        ("NVRAM template", OVMF_VARS_SRC),
    ):
        if not path.is_file():
            missing.append(f"{label}: {path}")

    if missing:
        for item in missing:
            print(f"missing {item}", file=sys.stderr)
        return 1

    if not TARGET_EFI.exists():
        print(f"kernel not built: {TARGET_EFI}")
    else:
        print(f"kernel ok: {TARGET_EFI}")

    print("doctor ok")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    build_parser = subparsers.add_parser("build")
    build_parser.add_argument(
        "--user-init",
        choices=["off", "c", "elf", "bad-syscall", "bad-elf", "yield-stress", "bad-bootstrap", "fd-write", "fd-read", "open-read", "open-flags", "exec-elf", "dup-full", "bad-pointers", "exec-loop", "exec-bad-loop", "exec-transfer-fail", "exec-registry", "exec-paths", "exec-root", "exec-noent", "exec-nonexec"],
        default="off",
    )
    build_parser.add_argument(
        "--user-program",
        choices=sorted(USER_PROGRAM_SOURCES.keys()),
        default="init",
    )

    boot_parser = subparsers.add_parser("boot")
    boot_parser.add_argument("--serial-log")
    boot_parser.add_argument("--reset-vars", action="store_true")

    verify_parser = subparsers.add_parser("verify")
    verify_parser.add_argument("--mode", choices=["all"] + list(MODE_RULES.keys()), default="all")
    verify_parser.add_argument("--timeout", type=float, default=5.0)

    stress_parser = subparsers.add_parser("stress")
    stress_parser.add_argument("--mode", choices=["all"] + list(MODE_RULES.keys()), default="all")
    stress_parser.add_argument("--loops", type=int, default=25)
    stress_parser.add_argument("--timeout", type=float, default=5.0)

    subparsers.add_parser("clean")
    subparsers.add_parser("doctor")

    args = parser.parse_args(argv)

    if args.command == "build":
        build(args.user_init, args.user_program)
        return 0
    if args.command == "boot":
        boot_args: list[str] = []
        if args.serial_log is not None:
            boot_args.extend(["--serial-log", args.serial_log])
        if args.reset_vars:
            boot_args.append("--reset-vars")
        boot_main(boot_args)
        return 0
    if args.command == "verify":
        return verify(args.mode, 1, args.timeout)
    if args.command == "stress":
        return verify(args.mode, args.loops, args.timeout)
    if args.command == "clean":
        clean()
        return 0
    if args.command == "doctor":
        return doctor()

    return 1


if __name__ == "__main__":
    raise SystemExit(main())
