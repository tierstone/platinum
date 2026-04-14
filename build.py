#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

from tools import (
    ASFLAGS,
    BUILD_DIR,
    CFLAGS,
    CLANG_BIN,
    LLD_LINK_BIN,
    TARGET_EFI,
    ensure_dir,
    object_path_for,
    remove_tree,
    run,
    source_files,
)


def compile_sources(sources: list[Path], flags: list[str]) -> list[Path]:
    objects: list[Path] = []

    for source in sources:
        output = object_path_for(source)
        ensure_dir(output.parent)
        run([CLANG_BIN] + flags + ["-c", str(source), "-o", str(output)])
        objects.append(output)

    return objects


def link_efi(objects: list[Path]) -> None:
    run([
        LLD_LINK_BIN,
        "/subsystem:efi_application",
        "/entry:efi_main",
        "/machine:x64",
        "/nodefaultlib",
        f"/out:{TARGET_EFI}",
    ] + [str(obj) for obj in objects])


def clean() -> None:
    remove_tree(BUILD_DIR)


def build() -> None:
    c_sources, asm_sources = source_files()

    ensure_dir(BUILD_DIR)

    objects: list[Path] = []
    objects.extend(compile_sources(c_sources, CFLAGS))
    objects.extend(compile_sources(asm_sources, ASFLAGS))
    link_efi(objects)

    print(f"Output: {TARGET_EFI}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("target", choices=["all", "efi", "clean"])
    args = parser.parse_args()

    if args.target == "clean":
        clean()
        return

    build()


if __name__ == "__main__":
    main()
