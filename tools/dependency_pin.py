#!/usr/bin/env python3
"""Report a pinned dependency field from the repository's single pin authority."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PINS = ROOT / "dependencies.json"


class DependencyPinError(RuntimeError):
    """Raised when the pin authority cannot answer the request."""


def load_pins(path: Path = PINS) -> dict[str, dict[str, str]]:
    if not path.is_file():
        raise DependencyPinError(f"pin authority is missing: {path}")
    try:
        pins = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        raise DependencyPinError(f"pin authority is not valid JSON: {path}: {error}") from error
    if not isinstance(pins, dict) or not pins:
        raise DependencyPinError(f"pin authority declares no dependencies: {path}")
    return pins


def pin_field(name: str, field: str, path: Path = PINS) -> str:
    pins = load_pins(path)
    if name not in pins:
        known = ", ".join(sorted(pins)) or "(none)"
        raise DependencyPinError(f"no pinned dependency named {name!r}; declared: {known}")
    entry = pins[name]
    if field not in entry:
        known = ", ".join(sorted(entry)) or "(none)"
        raise DependencyPinError(f"dependency {name!r} declares no {field!r}; has: {known}")
    value = entry[field]
    if not isinstance(value, str) or not value:
        raise DependencyPinError(f"dependency {name!r} field {field!r} is empty")
    return value


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--name", required=True, help="dependency to report, e.g. xenia")
    parser.add_argument("--field", default="revision", help="field to report (default: revision)")
    selected = parser.parse_args()
    print(pin_field(selected.name, selected.field))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
