#!/usr/bin/env python3
"""Fail when a first-party source file grows beyond its ownership boundary."""

from __future__ import annotations

import argparse
import pathlib
import re
import tempfile

SOURCE_SUFFIXES = {".cpp", ".hpp", ".py"}
MAX_LINES = 500
FORBIDDEN_PRODUCT_PATTERNS = {
    "generated PPC configuration": re.compile(r"ppc_(?:config|runtime)\.h"),
    "per-title generated function": re.compile(r"\bsub_[0-9A-Fa-f]+\b"),
    "title/engine dependency": re.compile(r"\b(?:alchemy|gears)\b", re.IGNORECASE),
    "direct standard-error logging": re.compile(r"(?:fprintf\s*\(\s*stderr|std::(?:cerr|clog))"),
    "process-environment configuration": re.compile(r"(?:std::)?getenv\s*\("),
}


def violations(root: pathlib.Path) -> list[tuple[pathlib.Path, int]]:
    found: list[tuple[pathlib.Path, int]] = []
    for top in ("include", "src", "tests", "tools"):
        directory = root / top
        if not directory.exists():
            continue
        for path in sorted(directory.rglob("*")):
            if path.is_file() and path.suffix in SOURCE_SUFFIXES:
                count = len(path.read_text(encoding="utf-8").splitlines())
                if count > MAX_LINES:
                    found.append((path.relative_to(root), count))
    return found


def dependency_violations(root: pathlib.Path) -> list[tuple[pathlib.Path, int, str]]:
    found: list[tuple[pathlib.Path, int, str]] = []
    paths = [root / "CMakeLists.txt"] if (root / "CMakeLists.txt").exists() else []
    for top in ("include", "src"):
        directory = root / top
        if directory.exists():
            paths.extend(path for path in directory.rglob("*") if path.is_file())
    for path in sorted(paths):
        for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            for label, pattern in FORBIDDEN_PRODUCT_PATTERNS.items():
                if pattern.search(line):
                    found.append((path.relative_to(root), line_number, label))
    return found


def run_selftest() -> int:
    scratch = pathlib.Path(__file__).resolve().parents[1] / "scratch"
    scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="structure_", dir=scratch) as temporary:
        root = pathlib.Path(temporary)
        source = root / "src"
        source.mkdir()
        (source / "at_limit.cpp").write_text("line\n" * MAX_LINES, encoding="utf-8")
        if violations(root):
            print("structure self-test: rejected a file exactly at the limit")
            return 1
        (source / "too_large.cpp").write_text("line\n" * (MAX_LINES + 1), encoding="utf-8")
        observed = violations(root)
        expected = [(pathlib.Path("src/too_large.cpp"), MAX_LINES + 1)]
        if observed != expected:
            print(f"structure self-test: expected {expected}, observed {observed}")
            return 1
        include = root / "include"
        include.mkdir()
        (include / "leak.hpp").write_text('#include "ppc_config.h"\n', encoding="utf-8")
        (source / "policy_leaks.cpp").write_text(
            'fprintf(stderr, "bad");\nauto value = getenv("X360PORT_MODE");\n',
            encoding="utf-8",
        )
        dependencies = dependency_violations(root)
        expected_dependency = [
            (pathlib.Path("include/leak.hpp"), 1, "generated PPC configuration"),
            (pathlib.Path("src/policy_leaks.cpp"), 1, "direct standard-error logging"),
            (pathlib.Path("src/policy_leaks.cpp"), 2, "process-environment configuration"),
        ]
        if dependencies != expected_dependency:
            print(
                "structure self-test: expected dependency refusal "
                f"{expected_dependency}, observed {dependencies}"
            )
            return 1
    print("structure self-test: accepted boundary and detected oversized source")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        return run_selftest()

    root = pathlib.Path(__file__).resolve().parents[1]
    found = violations(root)
    dependencies = dependency_violations(root)
    if found or dependencies:
        for path, count in found:
            print(f"structure: {path} has {count} lines; limit is {MAX_LINES}")
        for path, line, label in dependencies:
            print(f"structure: {path}:{line} contains forbidden {label}")
        return 1
    print(
        f"structure: all first-party source files are at most {MAX_LINES} lines; "
        "shared product code has no title/generated dependency"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
