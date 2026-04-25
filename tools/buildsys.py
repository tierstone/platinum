from __future__ import annotations

import argparse
from pathlib import Path

from .paths import (
    ASFLAGS,
    BUILD_DIR,
    CFLAGS,
    CLANG_BIN,
    LD_LLD_BIN,
    LLD_LINK_BIN,
    TARGET_EFI,
    USER_BLOB_C,
    USER_BUILD_DIR,
)
from .proc import ensure_dir, object_path_for, remove_tree, run, source_files


USER_PROGRAM_SPECS = (
    ("init", Path("src/user/init.c")),
    ("pulse", Path("src/user/pulse.c")),
    ("echo", Path("src/user/echo.c")),
    ("pulse2", Path("src/user/pulse2.c")),
)


def user_program_sources() -> dict[str, Path]:
    sources: dict[str, Path] = {}

    for program_name, source in USER_PROGRAM_SPECS:
        if "/" in program_name:
            raise ValueError(f"invalid user program name: {program_name}")
        if program_name in sources:
            raise ValueError(f"duplicate user program name: {program_name}")
        sources[program_name] = source

    return sources


USER_PROGRAM_SOURCES = user_program_sources()
USER_PROGRAM_NAMES = tuple(program_name for program_name, _ in USER_PROGRAM_SPECS)
EMBEDDED_USER_PROGRAM_NAMES = USER_PROGRAM_NAMES + ("badelf2",)


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
    user_object = USER_BUILD_DIR / (user_program + ".o")
    user_linker_script = Path("src/user/link.ld")
    user_elf = USER_BUILD_DIR / (user_program + ".elf")

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
        str(user_elf),
        str(user_object),
    ])

    return user_elf


def user_blob_prelude() -> list[str]:
    return [
        '#include "kernel/embedded_user_programs.h"',
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
    ]


def append_user_blob_program(lines: list[str], program_name: str, user_elf: Path) -> None:
    data = user_elf.read_bytes()

    lines.extend([
        f"const uint8_t embedded_user_program_{program_name}_elf[] = {{",
    ])

    index = 0
    while index < len(data):
        chunk = data[index:index + 12]
        lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
        index += len(chunk)

    lines.extend([
        "};",
        "",
        f"const size_t embedded_user_program_{program_name}_elf_size = {len(data)}u;",
        "",
    ])


def append_badelf2_program(lines: list[str], user_elf: Path) -> None:
    data = bytearray(user_elf.read_bytes())

    if len(data) < 4:
        raise ValueError("embedded program too small for badelf2")

    data[0] = 0x00

    lines.extend([
        "const uint8_t embedded_user_program_badelf2_elf[] = {",
    ])

    index = 0
    while index < len(data):
        chunk = data[index:index + 12]
        lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
        index += len(chunk)

    lines.extend([
        "};",
        "",
        f"const size_t embedded_user_program_badelf2_elf_size = {len(data)}u;",
        "",
    ])


def append_user_blob_registry(lines: list[str], user_programs: dict[str, Path], program_names: tuple[str, ...]) -> None:
    lines.extend([
        "const struct embedded_user_program embedded_user_program_registry[] = {",
    ])
    for program_name in program_names:
        lines.append(
            f'    {{ "{program_name}", embedded_user_program_{program_name}_elf, '
            f"embedded_user_program_{program_name}_elf_size }},"
        )
    lines.extend([
        "};",
        "",
        f"const size_t embedded_user_program_registry_count = {len(program_names)}u;",
        "",
    ])


def append_first_user_program_metadata(lines: list[str], first_user_program: str) -> None:
    lines.extend([
        f"const uint8_t *embedded_first_user_program_image = embedded_user_program_{first_user_program}_elf;",
        f"const size_t embedded_first_user_program_size = embedded_user_program_{first_user_program}_elf_size;",
        "",
    ])


def generate_user_blob_source(user_programs: dict[str, Path], first_user_program: str) -> Path:
    ensure_dir(USER_BLOB_C.parent)

    lines = user_blob_prelude()

    for program_name in USER_PROGRAM_NAMES:
        append_user_blob_program(lines, program_name, user_programs[program_name])

    append_badelf2_program(lines, user_programs["pulse"])

    append_user_blob_registry(lines, user_programs, EMBEDDED_USER_PROGRAM_NAMES)
    append_first_user_program_metadata(lines, first_user_program)

    USER_BLOB_C.write_text("\n".join(lines), encoding="ascii")
    return USER_BLOB_C


def build_embedded_user_programs(extra_flags: list[str]) -> dict[str, Path]:
    return {
        program_name: build_user_program(program_name, extra_flags)
        for program_name in USER_PROGRAM_NAMES
    }


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

    if mode == "fd-write":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_FD_WRITE=1",
        ]

    if mode == "fd-read":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_FD_READ=1",
        ]

    if mode == "open-read":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_OPEN=1",
        ]

    if mode == "open-flags":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_OPEN_FLAGS=1",
        ]

    if mode == "open-invalid":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_OPEN_INVALID=1",
        ]

    if mode == "exec-elf":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_EXEC=1",
        ]

    if mode == "dup-full":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_DUP_FULL=1",
        ]

    if mode == "bad-pointers":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_BAD_POINTERS=1",
        ]

    if mode == "exec-loop":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_EXEC_LOOP=1",
        ]

    if mode == "exec-bad-loop":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_EXEC_BAD_LOOP=1",
            "-DUSER_TEST_BAD_EXEC_IMAGE=1",
        ]

    if mode == "exec-transfer-fail":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_EXEC_TRANSFER_FAIL=1",
        ]

    if mode == "exec-registry":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_EXEC_REGISTRY=1",
        ]

    if mode == "exec-paths":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_EXEC_PATHS=1",
        ]

    if mode == "exec-root":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_EXEC_DIR_ROOT=1",
        ]

    if mode == "exec-noent":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_EXEC_NOENT=1",
        ]

    if mode == "exec-nonexec":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_EXEC_NONEXEC=1",
        ]

    if mode == "exec-bad-elf2":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_EXEC_BAD_ELF2=1",
        ]

    if mode == "exec-bad-paths":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_EXEC_BAD_PATHS=1",
        ]

    if mode == "exec-second":
        return [
            "-DUSER_INIT_ENABLED=1",
            "-DUSER_INIT_USE_ELF=0",
            "-DUSER_TEST_EXEC_SECOND=1",
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
    user_programs = build_embedded_user_programs(extra_cflags)
    generated_blob = generate_user_blob_source(user_programs, user_program)

    ensure_dir(BUILD_DIR)

    objects: list[Path] = []
    objects.extend(compile_sources(c_sources, CFLAGS, extra_cflags))
    objects.extend(compile_sources(asm_sources, ASFLAGS, []))
    objects.extend(compile_sources([generated_blob], CFLAGS, extra_cflags))
    link_efi(objects)

    print(f"Output: {TARGET_EFI}")


def build_main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("target", choices=["all", "efi", "clean"])
    parser.add_argument(
        "--user-init",
        choices=["off", "c", "elf", "bad-syscall", "bad-elf", "yield-stress", "bad-bootstrap", "fd-write", "fd-read", "open-read", "open-flags", "open-invalid", "exec-elf", "dup-full", "bad-pointers", "exec-loop", "exec-bad-loop", "exec-transfer-fail", "exec-registry", "exec-paths", "exec-root", "exec-noent", "exec-nonexec", "exec-bad-elf2", "exec-bad-paths", "exec-second"],
        default="off",
        help="select first scheduled user task path for test builds",
    )
    parser.add_argument(
        "--user-program",
        choices=list(USER_PROGRAM_NAMES),
        default="init",
        help="select embedded user ELF source program",
    )
    args = parser.parse_args(argv)

    if args.target == "clean":
        clean()
        return

    build(args.user_init, args.user_program)
