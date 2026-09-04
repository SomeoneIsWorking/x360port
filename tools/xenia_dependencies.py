"""Prepare host-specific inputs required by the pinned Xenia checkout."""

from __future__ import annotations

import pathlib
import sys
from collections.abc import Callable, Sequence

from build_support import require_program, run

CommandRunner = Callable[..., None]


def _require_file(path: pathlib.Path, description: str) -> pathlib.Path:
    if not path.is_file():
        raise RuntimeError(f"{description} is missing: {path}")
    return path


def _macos_sdl_framework(
    xenia_source: pathlib.Path,
    build_dir: pathlib.Path,
    *,
    runner: CommandRunner,
) -> pathlib.Path:
    """Build the exact SDL2 submodule as the framework Xenia consumes on macOS."""
    sdl_source = xenia_source / "third_party" / "SDL2"
    project = _require_file(
        sdl_source / "Xcode" / "SDL" / "SDL.xcodeproj" / "project.pbxproj",
        "pinned SDL2 Xcode project",
    ).parent
    output_dir = build_dir / "deps" / "sdl2-framework"
    runner(
        (
            require_program("xcodebuild"),
            "-project",
            str(project),
            "-target",
            "Framework",
            "-configuration",
            "Release",
            f"CONFIGURATION_BUILD_DIR={output_dir}",
            f"OBJROOT={build_dir / 'deps' / 'sdl2-objects'}",
            "ONLY_ACTIVE_ARCH=YES",
            "CODE_SIGNING_ALLOWED=NO",
        )
    )
    framework = output_dir / "SDL2.framework"
    _require_file(framework / "Headers" / "SDL.h", "built SDL2 framework header")
    _require_file(framework / "SDL2", "built SDL2 framework binary")
    return output_dir


def prepare_xenia_dependencies(
    xenia_source: pathlib.Path,
    build_dir: pathlib.Path,
    *,
    host_platform: str = sys.platform,
    runner: CommandRunner = run,
) -> Sequence[str]:
    """Return parent-CMake options for exact, prepared Xenia dependencies."""
    if host_platform != "darwin":
        return ()
    framework_dir = _macos_sdl_framework(xenia_source, build_dir, runner=runner)
    return (f"-DCMAKE_FRAMEWORK_PATH={framework_dir}",)
