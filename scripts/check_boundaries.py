#!/usr/bin/env python3
"""Lightweight domain import-boundary check."""

from __future__ import annotations

import ast
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src" / "bluos_knob"

FORBIDDEN_IMPORTS = {
    "device_input": ("bluos_knob.platform_adapter", "bluos_knob.cli", "hid"),
    "configuration": ("bluos_knob.cli", "bluos_knob.platform_adapter", "hid"),
    "platform_adapter": ("bluos_knob.cli", "bluos_knob.configuration"),
    "volume_policy": ("hid", "bluos_knob.platform_adapter", "bluos_knob.cli"),
    "amplifier_control": ("hid", "bluos_knob.platform_adapter", "bluos_knob.device_input"),
}


def main() -> int:
    failures: list[str] = []
    for path in SRC.rglob("*.py"):
        domain = _domain_for(path)
        if domain is None:
            continue
        forbidden = FORBIDDEN_IMPORTS.get(domain, ())
        for imported in _imports(path):
            for prefix in forbidden:
                if imported == prefix or imported.startswith(f"{prefix}."):
                    failures.append(f"{path.relative_to(ROOT)} imports forbidden {imported}")

    if failures:
        print("Domain boundary violations:")
        for failure in failures:
            print(f"- {failure}")
        return 1
    print("Domain boundary check passed.")
    return 0


def _domain_for(path: Path) -> str | None:
    relative = path.relative_to(SRC)
    if len(relative.parts) < 2:
        return None
    return relative.parts[0]


def _imports(path: Path) -> set[str]:
    tree = ast.parse(path.read_text(encoding="utf-8"))
    imports: set[str] = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            imports.update(alias.name for alias in node.names)
        elif isinstance(node, ast.ImportFrom) and node.module:
            imports.add(node.module)
    return imports


if __name__ == "__main__":
    raise SystemExit(main())

