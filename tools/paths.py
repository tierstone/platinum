from __future__ import annotations

from pathlib import Path


ROOT_DIR = Path(__file__).resolve().parent.parent
SRC_DIR = ROOT_DIR / "src"
BUILD_DIR = ROOT_DIR / "build"
TARGET_EFI = BUILD_DIR / "kernel.efi"
USER_BUILD_DIR = BUILD_DIR / "user"
USER_PROGRAM_ELF = USER_BUILD_DIR / "init.elf"
USER_BLOB_C = BUILD_DIR / "generated" / "user_program_blob.c"
TEST_LOG_DIR = BUILD_DIR / "test-logs"

# OVMF_CODE = Path("/usr/share/edk2-ovmf/x64/OVMF_CODE.4m.fd")
# OVMF_VARS_SRC = Path("/usr/share/edk2-ovmf/x64/OVMF_VARS.4m.fd")
# OVMF_VARS_DST = ROOT_DIR / "OVMF_VARS.4m.fd"
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
