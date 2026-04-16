from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import sys

from .paths import BUILD_DIR, SRC_DIR


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
