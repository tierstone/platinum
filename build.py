#!/usr/bin/env python3
import os
import sys
import shutil
import subprocess
import argparse

BUILD_DIR = "build"
SRC_DIR = "src"
TARGET_EFI = os.path.join(BUILD_DIR, "kernel.efi")

C_SOURCES = [
    os.path.join(SRC_DIR, "kernel", "core.c"),
    os.path.join(SRC_DIR, "kernel", "gdt.c"),
    os.path.join(SRC_DIR, "kernel", "idt.c"),
    os.path.join(SRC_DIR, "kernel", "paging.c"),
    os.path.join(SRC_DIR, "drivers", "serial.c"),
]

ASM_SOURCES = [
    os.path.join(SRC_DIR, "arch", "x86_64", "boot.S"),
]

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
    "-Isrc",
]

CFLAGS = COMMON_FLAGS + [
    "-std=c11",
]

ASFLAGS = COMMON_FLAGS + [
    "-x",
    "assembler",
]

def execute(command):
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        if result.stdout:
            sys.stderr.write(result.stdout)
        if result.stderr:
            sys.stderr.write(result.stderr)
        sys.exit(result.returncode)

def object_path_for(source):
    basename = os.path.splitext(os.path.basename(source))[0]
    return os.path.join(BUILD_DIR, basename + ".o")

def compile_c_objects():
    objects = []
    for source in C_SOURCES:
        output = object_path_for(source)
        execute(["clang"] + CFLAGS + ["-c", source, "-o", output])
        objects.append(output)
    return objects

def compile_asm_objects():
    objects = []
    for source in ASM_SOURCES:
        output = object_path_for(source)
        execute(["clang"] + ASFLAGS + ["-c", source, "-o", output])
        objects.append(output)
    return objects

def link_efi(objects):
    execute([
        "lld-link",
        "/subsystem:efi_application",
        "/entry:efi_main",
        "/machine:x64",
        "/nodefaultlib",
        "/out:" + TARGET_EFI,
    ] + objects)

def clean():
    if os.path.isdir(BUILD_DIR):
        shutil.rmtree(BUILD_DIR)

def build():
    os.makedirs(BUILD_DIR, exist_ok=True)
    objects = []
    objects.extend(compile_c_objects())
    objects.extend(compile_asm_objects())
    link_efi(objects)
    print("Output:", TARGET_EFI)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("target", choices=["all", "efi", "clean"])
    args = parser.parse_args()

    if args.target == "clean":
        clean()
        return

    build()

if __name__ == "__main__":
    main()
