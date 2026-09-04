"""Configure, build, and verify the real Xenia-backed x360port runtime."""

from __future__ import annotations

import argparse
import pathlib

from build_support import (
    DEFAULT_BUILD_DIR,
    ROOT,
    compiler_names,
    python_executable,
    require_build_dir,
    require_machine,
    require_program,
    require_xenia_source,
    run,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xenia-source", required=True, type=pathlib.Path)
    parser.add_argument("--build-dir", default=DEFAULT_BUILD_DIR, type=pathlib.Path)
    parser.add_argument("--expected-machine", choices=("x86_64", "arm64"))
    parser.add_argument("--parallel", default=2, type=int)
    return parser.parse_args()


def verify_python() -> None:
    ruff = require_program("ruff")
    run((ruff, "check", "tools"))
    run((ruff, "format", "--check", "tools"))


def configure(build_dir: pathlib.Path, xenia_source: pathlib.Path) -> None:
    c_name, cxx_name = compiler_names()
    c_compiler = require_program(c_name)
    cxx_compiler = require_program(cxx_name)
    require_program("ninja")
    run(
        (
            require_program("cmake"),
            "-S",
            str(ROOT),
            "-B",
            str(build_dir),
            "-G",
            "Ninja",
            "-DCMAKE_BUILD_TYPE=Debug",
            f"-DCMAKE_C_COMPILER={c_compiler}",
            f"-DCMAKE_CXX_COMPILER={cxx_compiler}",
            f"-DPython3_EXECUTABLE={python_executable()}",
            f"-DX360PORT_XENIA_SOURCE_DIR={xenia_source}",
        )
    )


def verify_runtime(build_dir: pathlib.Path, parallel: int) -> None:
    if parallel < 1:
        raise ValueError("--parallel must be positive")
    cmake = require_program("cmake")
    run((cmake, "--build", str(build_dir), "--parallel", str(parallel)))
    run((require_program("ctest"), "--test-dir", str(build_dir), "--output-on-failure"))


def main() -> int:
    args = parse_args()
    require_machine(args.expected_machine)
    build_dir = require_build_dir(args.build_dir)
    xenia_source = require_xenia_source(args.xenia_source)
    verify_python()
    configure(build_dir, xenia_source)
    verify_runtime(build_dir, args.parallel)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
