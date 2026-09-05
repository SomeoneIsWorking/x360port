"""Exercise production CMake warning selection and the native compiler driver."""

from __future__ import annotations

import pathlib
import subprocess
import tempfile
import unittest

from build_support import ROOT, compiler_names, require_program, run


class WarningPolicyTests(unittest.TestCase):
    def test_driver_groups_preserve_modern_cpp_and_reject_real_warnings(self) -> None:
        build_root = ROOT / "build"
        build_root.mkdir(exist_ok=True)
        compiler = require_program(compiler_names()[1])
        with tempfile.TemporaryDirectory(prefix="warning-contract-", dir=build_root) as directory:
            build = pathlib.Path(directory)
            run(
                (
                    require_program("cmake"),
                    "-S",
                    str(ROOT / "tests/warnings"),
                    "-B",
                    str(build),
                    "-G",
                    "Ninja",
                    f"-DCMAKE_CXX_COMPILER={compiler}",
                ),
                capture_output=True,
            )
            flags = {
                name: (build / f"{name}.flags").read_text().splitlines()
                for name in ("gnu", "clang_cl", "msvc")
            }
            self.assertEqual(flags["gnu"], ["-Wall", "-Wextra", "-Wpedantic", "-Werror"])
            self.assertEqual(
                flags["clang_cl"], ["/clang:-Wall", "/clang:-Wextra", "/clang:-Wpedantic", "/WX"]
            )
            self.assertEqual(flags["msvc"], ["/W4", "/WX"])
            clang_cl = pathlib.Path(compiler).stem == "clang-cl"
            syntax = (
                ["/std:c++20", "/TP", "/Zs"]
                if clang_cl
                else ["-std=c++20", "-x", "c++", "-fsyntax-only"]
            )
            command = [compiler, *flags["clang_cl" if clang_cl else "gnu"], *syntax, "-"]
            positive = (ROOT / "tests/warnings/probe.cpp").read_text()
            for source, succeeds in (
                (positive, True),
                ("int warning_control(int unused) { return 0; }\n", False),
            ):
                if succeeds:
                    run(command, input_text=source, capture_output=True)
                else:
                    with self.assertRaises(subprocess.CalledProcessError) as failure:
                        run(command, input_text=source, capture_output=True)
                    self.assertIn("unused parameter", failure.exception.stderr)


if __name__ == "__main__":
    unittest.main()
