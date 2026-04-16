#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

from tools import (
    ASFLAGS,
    BUILD_DIR,
    CFLAGS,
    CLANG_BIN,
    LD_LLD_BIN,
    LLD_LINK_BIN,
    TARGET_EFI,
    USER_BLOB_C,
    USER_BUILD_DIR,
    USER_PROGRAM_ELF,
    ensure_dir,
    object_path_for,
    remove_tree,
    run,
    source_files,
)


USER_PROGRAM_SOURCES = {
    "init": Path("src/user/init.c"),
    "pulse": Path("src/user/pulse.c"),
}


def compile_sources(sources: list[Path], flags: list[str], extra_flags: list[str]) -> list[Path]:
    objects: list[Path] = []

    for source in sources:
        if source.is_relative_to(Path("src").resolve()):
            output = object_path_for(source)
        else:
            output = BUILD_DIR / "generated" / (source.stem + ".o")
        ensure_dir(output.parent)
        run([CLANG_BIN] + flags + extra_flags + ["-c", str(source), "-o", str(output)])
        objects.append(output)

    return objects


def build_user_program(user_program: str, extra_flags: list[str]) -> Path:
    user_source = USER_PROGRAM_SOURCES[user_program]
    user_object = USER_BUILD_DIR / (user_source.stem + ".o")
    user_linker_script = Path("src/user/link.ld")

    ensure_dir(USER_BUILD_DIR)
    run([
        CLANG_BIN,
        "--target=x86_64-unknown-none-elf",
        "-ffreestanding",
        "-fno-builtin",
        "-fno-stack-protector",
        "-fno-pic",
        "-mno-red-zone",
        "-mno-mmx",
        "-mno-sse",
        "-mno-avx",
        "-fno-asynchronous-unwind-tables",
        "-fno-unwind-tables",
        "-O2",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        f"-I{Path('src').resolve()}",
        "-std=c11",
    ] + extra_flags + [
        "-c",
        str(user_source),
        "-o",
        str(user_object),
    ])
    run([
        LD_LLD_BIN,
        "-nostdlib",
        "-static",
        "-T",
        str(user_linker_script),
        "-o",
        str(USER_PROGRAM_ELF),
        str(user_object),
    ])

    return USER_PROGRAM_ELF


def generate_user_blob_source(user_elf: Path) -> Path:
    data = user_elf.read_bytes()
    ensure_dir(USER_BLOB_C.parent)

    lines = [
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
        "const uint8_t embedded_user_program_elf[] = {",
    ]

    index = 0
    while index < len(data):
        chunk = data[index:index + 12]
        lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
        index += len(chunk)

    lines.extend([
        "};",
        "",
        f"const size_t embedded_user_program_elf_size = {len(data)}u;",
        "",
    ])

    USER_BLOB_C.write_text("\n".join(lines), encoding="ascii")
    return USER_BLOB_C


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


def user_init_flags(mode: str) -> list[str]:
    if mode == "off":
        return [
            "-DUSER_INIT_ENABLED=0",
            "-DUSER_INIT_USE_ELF=0",
        ]

    if mode == "c":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
        ]

    if mode == "elf":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=1",
        ]

    if mode == "bad-syscall":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_BAD_SYSCALL=1",
        ]

    if mode == "bad-elf":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=1",
            "-DUSER_TEST_BAD_ELF=1",
        ]

    if mode == "yield-stress":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_YIELD_STRESS=1",
        ]

    if mode == "bad-bootstrap":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_BAD_BOOTSTRAP=1",
        ]

    raise ValueError(f"unknown user init mode: {mode}")


def build(user_init_mode: str, user_program: str) -> None:
    c_sources, asm_sources = source_files()
    c_sources = [
        source for source in c_sources
        if not (
            len(source.parts) >= 2 and
            source.parts[-2] == "user" and
            source.name != "init.c"
        )
    ]
    extra_cflags = user_init_flags(user_init_mode)
    user_elf = build_user_program(user_program, extra_cflags)
    generated_blob = generate_user_blob_source(user_elf)

    ensure_dir(BUILD_DIR)

    objects: list[Path] = []
    objects.extend(compile_sources(c_sources, CFLAGS, extra_cflags))
    objects.extend(compile_sources(asm_sources, ASFLAGS, []))
    objects.extend(compile_sources([generated_blob], CFLAGS, extra_cflags))
    link_efi(objects)

    print(f"Output: {TARGET_EFI}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("target", choices=["all", "efi", "clean"])
    parser.add_argument(
        "--user-init",
        choices=["off", "c", "elf", "bad-syscall", "bad-elf", "yield-stress", "bad-bootstrap"],
        default="off",
        help="select first scheduled user task path for test builds",
    )
    parser.add_argument(
        "--user-program",
        choices=sorted(USER_PROGRAM_SOURCES.keys()),
        default="init",
        help="select embedded user ELF source program",
    )
    args = parser.parse_args()

    if args.target == "clean":
        clean()
        return

    build(args.user_init, args.user_program)


if __name__ == "__main__":
    main()
