"""Shared process, path, and host checks for x360port tooling."""

from __future__ import annotations

import os
import pathlib
import platform
import shutil
import subprocess
import sys
from collections.abc import Mapping, Sequence

ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_BUILD_DIR = ROOT / "build" / "verify"


def run(
    command: Sequence[str],
    *,
    cwd: pathlib.Path = ROOT,
    env: Mapping[str, str] | None = None,
) -> None:
    """Run one required command and preserve its exit status."""
    rendered = subprocess.list2cmdline(command)
    print(f"+ {rendered}", flush=True)
    subprocess.run(  # noqa: S603 - argv has no shell expansion.
        command,
        cwd=cwd,
        check=True,
        env=env,
    )


def require_program(name: str) -> str:
    """Resolve one required executable without fallback."""
    path = shutil.which(name)
    if path is None:
        raise RuntimeError(f"required program is missing from PATH: {name}")
    return path


def require_build_dir(path: pathlib.Path) -> pathlib.Path:
    """Keep generated build state under the repository build owner."""
    resolved = path.resolve()
    build_root = (ROOT / "build").resolve()
    if resolved == build_root or build_root not in resolved.parents:
        raise ValueError(f"build directory must be a child of {build_root}: {resolved}")
    return resolved


def require_xenia_source(path: pathlib.Path) -> pathlib.Path:
    """Require a complete git checkout rather than an implicit vendored fallback."""
    resolved = path.resolve()
    required = resolved / "src" / "xenia" / "cpu" / "processor.cc"
    if not required.is_file() or not (resolved / ".git").exists():
        raise ValueError(f"Xenia source is not a complete git checkout: {resolved}")
    return resolved


def normalized_machine() -> str:
    machine = platform.machine().lower()
    aliases = {
        "amd64": "x86_64",
        "x64": "x86_64",
        "aarch64": "arm64",
    }
    return aliases.get(machine, machine)


def require_machine(expected: str | None) -> None:
    if expected is None:
        return
    actual = normalized_machine()
    if actual != expected:
        raise RuntimeError(f"host architecture mismatch: expected {expected}, observed {actual}")


def compiler_names() -> tuple[str, str]:
    """Select Clang's native command-line driver for the current host."""
    if os.name == "nt":
        return "clang-cl", "clang-cl"
    return "clang", "clang++"


def cmake_child_environment(c_compiler: str, cxx_compiler: str) -> dict[str, str]:
    """Keep nested dependency configures on the parent's compiler and Ninja contract."""
    environment = os.environ.copy()
    environment.update(
        {
            "CC": c_compiler,
            "CXX": cxx_compiler,
            "CMAKE_GENERATOR": "Ninja",
        }
    )
    return environment


def python_executable() -> str:
    return str(pathlib.Path(sys.executable).resolve())
