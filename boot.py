#!/usr/bin/env python3
from __future__ import annotations

import sys
from pathlib import Path
import subprocess

from tools import (
    EFI_BOOT_BIN,
    EFI_BOOT_DIR,
    FAT_ROOT,
    OVMF_CODE,
    OVMF_VARS_DST,
    OVMF_VARS_SRC,
    TARGET_EFI,
    copy_file,
    qemu_command,
    remove_tree,
    require_file,
)


def check_prerequisites() -> None:
    require_file(OVMF_CODE, "firmware")
    require_file(OVMF_VARS_SRC, "NVRAM template")
    require_file(TARGET_EFI, "kernel")


def prepare_vars() -> None:
    if not OVMF_VARS_DST.exists():
        copy_file(OVMF_VARS_SRC, OVMF_VARS_DST)


def prepare_fat_drive() -> None:
    remove_tree(FAT_ROOT)
    EFI_BOOT_DIR.mkdir(parents=True, exist_ok=True)
    copy_file(TARGET_EFI, EFI_BOOT_BIN)


def launch_qemu() -> None:
    check_prerequisites()
    prepare_vars()
    prepare_fat_drive()

    try:
        subprocess.run(qemu_command(), check=False)
    except KeyboardInterrupt:
        raise SystemExit(0)


if __name__ == "__main__":
    launch_qemu()
