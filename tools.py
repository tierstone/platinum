#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import sys


ROOT_DIR = Path(__file__).resolve().parent
SRC_DIR = ROOT_DIR / "src"
BUILD_DIR = ROOT_DIR / "build"
TARGET_EFI = BUILD_DIR / "kernel.efi"
USER_BUILD_DIR = BUILD_DIR / "user"
USER_PROGRAM_ELF = USER_BUILD_DIR / "init.elf"
USER_BLOB_C = BUILD_DIR / "generated" / "user_program_blob.c"

OVMF_CODE = Path("/usr/share/edk2-ovmf/OVMF_CODE.fd")
OVMF_VARS_SRC = Path("/usr/share/edk2-ovmf/OVMF_VARS.fd")
OVMF_VARS_DST = ROOT_DIR / "OVMF_VARS.fd"

FAT_ROOT = ROOT_DIR / "root"
EFI_BOOT_DIR = FAT_ROOT / "EFI" / "BOOT"
EFI_BOOT_BIN = EFI_BOOT_DIR / "BOOTX64.EFI"

QEMU_BIN = "qemu-system-x86_64"
CLANG_BIN = "clang"
LLD_LINK_BIN = "lld-link"
LD_LLD_BIN = "ld.lld"

COMMON_FLAGS = [
    "--target=x86_64-unknown-windows",
    "-ffreestanding",
    "-fno-builtin",
    "-fno-stack-protector",
    "-fno-pic",
    "-mno-red-zone",
    "-mno-mmx",
    "-mno-sse",
    "-mno-avx",
    "-O2",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    f"-I{SRC_DIR}",
]

CFLAGS = COMMON_FLAGS + [
    "-std=c11",
]

ASFLAGS = COMMON_FLAGS + [
    "-x",
    "assembler",
]


def fail(message: str, code: int = 1) -> None:
    sys.stderr.write(message + "\n")
    raise SystemExit(code)


def run(command: list[str]) -> None:
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        if result.stdout:
            sys.stderr.write(result.stdout)
        if result.stderr:
            sys.stderr.write(result.stderr)
        raise SystemExit(result.returncode)


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def remove_tree(path: Path) -> None:
    if path.exists():
        shutil.rmtree(path)


def copy_file(src: Path, dst: Path) -> None:
    ensure_dir(dst.parent)
    shutil.copy2(src, dst)


def require_file(path: Path, label: str) -> None:
    if not path.is_file():
        fail(f"Missing {label}: {path}")


def source_files() -> tuple[list[Path], list[Path]]:
    c_sources = sorted(SRC_DIR.rglob("*.c"))
    asm_sources = sorted(list(SRC_DIR.rglob("*.S")) + list(SRC_DIR.rglob("*.asm")))
    return c_sources, asm_sources


def object_path_for(source: Path) -> Path:
    rel = source.relative_to(SRC_DIR)
    parts = list(rel.parts)
    parts[-1] = rel.stem + ".o"
    return BUILD_DIR.joinpath(*parts)


def qemu_command() -> list[str]:
    return [
        QEMU_BIN,
        "-machine", "q35",
        "-m", "512M",
        "-drive", f"if=pflash,format=raw,readonly=on,file={OVMF_CODE}",
        "-drive", f"if=pflash,format=raw,file={OVMF_VARS_DST}",
        "-drive", f"format=raw,file=fat:rw:{FAT_ROOT}",
        "-nographic",
        "-serial", "mon:stdio",
    ]
