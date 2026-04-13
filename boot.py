#!/usr/bin/env python3
import os
import sys
import shutil
import subprocess

OVMF_CODE = "/usr/share/edk2-ovmf/OVMF_CODE.fd"
OVMF_VARS_SRC = "/usr/share/edk2-ovmf/OVMF_VARS.fd"
OVMF_VARS_DST = "./OVMF_VARS.fd"
QEMU_BIN = "qemu-system-x86_64"
FAT_ROOT = "root"
EFI_BIN = "build/kernel.efi"

def check_prerequisites():
    if not os.path.isfile(OVMF_CODE):
        sys.exit("Missing firmware: " + OVMF_CODE)
    if not os.path.isfile(EFI_BIN):
        sys.exit("Missing kernel: " + EFI_BIN + ". Run python3 build.py all")
    if not os.path.isfile(OVMF_VARS_SRC):
        sys.exit("Missing NVRAM template: " + OVMF_VARS_SRC)

def prepare_vars():
    if not os.path.exists(OVMF_VARS_DST):
        shutil.copy2(OVMF_VARS_SRC, OVMF_VARS_DST)

def prepare_fat_drive():
    if os.path.exists(FAT_ROOT):
        shutil.rmtree(FAT_ROOT)

    boot_dir = os.path.join(FAT_ROOT, "EFI", "BOOT")
    os.makedirs(boot_dir, exist_ok=True)

    shutil.copy2(EFI_BIN, os.path.join(boot_dir, "BOOTX64.EFI"))

def launch_qemu():
    check_prerequisites()
    prepare_vars()
    prepare_fat_drive()

    command = [
        QEMU_BIN,
        "-machine", "q35",
        "-m", "512M",
        "-drive", "if=pflash,format=raw,readonly=on,file=" + OVMF_CODE,
        "-drive", "if=pflash,format=raw,file=" + OVMF_VARS_DST,
        "-drive", "format=raw,file=fat:rw:" + FAT_ROOT,
        "-nographic",
        "-serial", "mon:stdio"
    ]

    try:
        subprocess.run(command, check=False)
    except KeyboardInterrupt:
        sys.exit(0)

if __name__ == "__main__":
    launch_qemu()
