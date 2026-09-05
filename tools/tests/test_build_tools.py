"""Falsifiers for x360port's portable dependency-build policy."""

from __future__ import annotations

import os
import pathlib
import tempfile
import unittest
from unittest import mock

from build_support import ROOT, cmake_child_environment, compiler_names, python_executable
from xenia_dependencies import prepare_xenia_dependencies


class CmakeChildEnvironmentTests(unittest.TestCase):
    def test_python_handoff_preserves_the_virtual_environment_entry_point(self) -> None:
        with (
            mock.patch("build_support.sys.executable", str(ROOT / ".venv/bin/python")),
            mock.patch("pathlib.Path.resolve", return_value=pathlib.Path("/base/python")),
        ):
            self.assertEqual(python_executable(), str(ROOT / ".venv/bin/python"))

    def test_macos_selects_xcode_compilers_independently_of_llvm_path(self) -> None:
        with (
            mock.patch("build_support.os.name", "posix"),
            mock.patch("build_support.platform.system", return_value="Darwin"),
        ):
            self.assertEqual(compiler_names(), ("/usr/bin/clang", "/usr/bin/clang++"))

    def test_windows_selects_native_clang_cl_driver(self) -> None:
        with mock.patch("build_support.os.name", "nt"):
            self.assertEqual(compiler_names(), ("clang-cl", "clang-cl"))

    def test_nested_configures_inherit_ninja_and_exact_compilers(self) -> None:
        original_generator = os.environ.get("CMAKE_GENERATOR")

        environment = cmake_child_environment("clang-c", "clang-cxx")

        self.assertEqual(environment["CMAKE_GENERATOR"], "Ninja")
        self.assertEqual(environment["CC"], "clang-c")
        self.assertEqual(environment["CXX"], "clang-cxx")
        self.assertEqual(os.environ.get("CMAKE_GENERATOR"), original_generator)


class XeniaDependencyTests(unittest.TestCase):
    def test_non_macos_hosts_do_not_prepare_a_substitute_sdl(self) -> None:
        with mock.patch("xenia_dependencies.require_program") as require_program:
            options = prepare_xenia_dependencies(
                pathlib.Path("unused-xenia"),
                pathlib.Path("unused-build"),
                host_platform="linux",
            )

        self.assertEqual(options, ())
        require_program.assert_not_called()

    def test_macos_builds_framework_from_pinned_xenia_submodule(self) -> None:
        (ROOT / "build").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(
            prefix="dependency-test-", dir=ROOT / "build"
        ) as temporary:
            root = pathlib.Path(temporary)
            xenia_source = root / "xenia"
            project_file = (
                xenia_source
                / "third_party"
                / "SDL2"
                / "Xcode"
                / "SDL"
                / "SDL.xcodeproj"
                / "project.pbxproj"
            )
            project_file.parent.mkdir(parents=True)
            project_file.write_text("fixture", encoding="utf-8")

            observed_command: tuple[str, ...] | None = None

            def fake_runner(command: tuple[str, ...]) -> None:
                nonlocal observed_command
                observed_command = command
                output_argument = next(
                    argument
                    for argument in command
                    if argument.startswith("CONFIGURATION_BUILD_DIR=")
                )
                output_dir = pathlib.Path(output_argument.partition("=")[2])
                framework = output_dir / "SDL2.framework"
                (framework / "Headers").mkdir(parents=True)
                (framework / "Headers" / "SDL.h").write_text("fixture", encoding="utf-8")
                (framework / "SDL2").write_text("fixture", encoding="utf-8")

            with mock.patch("xenia_dependencies.require_program", return_value="xcodebuild"):
                options = prepare_xenia_dependencies(
                    xenia_source,
                    root / "build",
                    host_platform="darwin",
                    runner=fake_runner,
                )

            self.assertIsNotNone(observed_command)
            self.assertIn("-target", observed_command)
            self.assertIn("Framework", observed_command)
            self.assertEqual(
                options,
                (f"-DCMAKE_FRAMEWORK_PATH={root / 'build/deps/sdl2-framework'}",),
            )


if __name__ == "__main__":
    unittest.main()
