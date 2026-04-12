#!/usr/bin/env python3
import os
import sys
import shutil
import subprocess
import argparse

BUILD_DIR = "build"
SRC_DIR = "src"
TARGET_EFI = "build/kernel.efi"

C_SOURCES = [
    os.path.join("src", "kernel.c"),
    os.path.join("src", "drivers", "serial.c"),
]

ASM_SOURCES = [
    os.path.join("src", "arch", "x86_64", "boot.S"),
]

CFLAGS = [
    "-ffreestanding", "-fno-builtin", "-mno-red-zone", "-mno-sse",
    "-mno-mmx", "-mno-avx", "-fno-stack-protector", "-fno-pic",
    "-O2", "-Isrc"
]

def execute(command):
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        sys.exit(1)

def compile_objects():
    objects = []
    for source in C_SOURCES + ASM_SOURCES:
        basename = os.path.splitext(os.path.basename(source))[0]
        obj_path = os.path.join(BUILD_DIR, f"{basename}.o")
        execute(["clang", "--target=x86_64-unknown-windows"] + CFLAGS + ["-c", source, "-o", obj_path])
        objects.append(obj_path)
    return objects

def link_efi(objects):
    execute([
        "lld-link", "/subsystem:efi_application", "/entry:efi_main",
        "/machine:x64", "/nodefaultlib", f"/out:{TARGET_EFI}"
    ] + objects)

def clean():
    if os.path.exists(BUILD_DIR):
        shutil.rmtree(BUILD_DIR)

def build():
    os.makedirs(BUILD_DIR, exist_ok=True)
    objects = compile_objects()
    link_efi(objects)
    print(f"Output: {TARGET_EFI}")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("target", choices=["all", "clean", "efi"])
    args = parser.parse_args()
    
    if args.target == "clean":
        clean()
    elif args.target == "all" or args.target == "efi":
        build()

if __name__ == "__main__":
    main()
