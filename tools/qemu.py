from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

from .paths import (
    EFI_BOOT_BIN,
    EFI_BOOT_DIR,
    FAT_ROOT,
    OVMF_CODE,
    OVMF_VARS_DST,
    OVMF_VARS_SRC,
    QEMU_BIN,
    TARGET_EFI,
)
from .proc import copy_file, remove_tree, require_file


def qemu_command(serial_log: Path | None = None) -> list[str]:
    command = [
        QEMU_BIN,
        "-machine", "q35",
        "-m", "512M",
        "-drive", f"if=pflash,format=raw,readonly=on,file={OVMF_CODE}",
        "-drive", f"if=pflash,format=raw,file={OVMF_VARS_DST}",
        "-drive", f"format=raw,file=fat:rw:{FAT_ROOT}",
        "-nographic",
    ]

    if serial_log is None:
        command.extend([
            "-serial", "mon:stdio",
        ])
    else:
        command.extend([
            "-monitor", "none",
            "-serial", f"file:{serial_log}",
        ])

    return command


def check_prerequisites() -> None:
    require_file(OVMF_CODE, "firmware")
    require_file(OVMF_VARS_SRC, "NVRAM template")
    require_file(TARGET_EFI, "kernel")


def prepare_vars(reset: bool = False) -> None:
    if reset and OVMF_VARS_DST.exists():
        OVMF_VARS_DST.unlink()
    if not OVMF_VARS_DST.exists():
        copy_file(OVMF_VARS_SRC, OVMF_VARS_DST)


def prepare_fat_drive() -> None:
    remove_tree(FAT_ROOT)
    EFI_BOOT_DIR.mkdir(parents=True, exist_ok=True)
    copy_file(TARGET_EFI, EFI_BOOT_BIN)


def launch_qemu(serial_log: Path | None = None, reset_vars: bool = False) -> None:
    check_prerequisites()
    prepare_vars(reset_vars)
    prepare_fat_drive()

    try:
        subprocess.run(qemu_command(serial_log), check=False)
    except KeyboardInterrupt:
        raise SystemExit(0)


def boot_main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial-log", type=Path)
    parser.add_argument("--reset-vars", action="store_true")
    args = parser.parse_args(argv)
    launch_qemu(args.serial_log, args.reset_vars)
