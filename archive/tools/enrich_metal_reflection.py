#!/usr/bin/env python3
"""Enrich Slang Metal reflection JSON with logical {set, binding} metadata."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
from typing import Dict


_VK_BINDING_PATTERN = re.compile(
    r"\[vk::binding\(\s*(?P<binding>\d+)\s*,\s*(?P<set>\d+)\s*\)\]\s*"
    r"(?:(?:uniform\s+[A-Za-z_][A-Za-z0-9_<>]*\s+(?P<uniform_name>[A-Za-z_][A-Za-z0-9_]*)\s*;)"
    r"|(?:cbuffer\s+(?P<cbuffer_name>[A-Za-z_][A-Za-z0-9_]*)))",
    re.MULTILINE,
)


def _parse_logical_bindings(source: str) -> Dict[str, dict]:
    logical_bindings: Dict[str, dict] = {}
    for match in _VK_BINDING_PATTERN.finditer(source):
        name = match.group("uniform_name") or match.group("cbuffer_name")
        logical_bindings[name] = {
            "index": int(match.group("binding")),
            "space": int(match.group("set")),
        }
    return logical_bindings


def enrich_reflection(source_path: pathlib.Path, reflection_path: pathlib.Path) -> None:
    source_text = source_path.read_text(encoding="utf-8")
    logical_bindings = _parse_logical_bindings(source_text)

    reflection = json.loads(reflection_path.read_text(encoding="utf-8"))
    for parameter in reflection.get("parameters", []):
        name = parameter.get("name")
        if not isinstance(name, str):
            continue

        logical_binding = logical_bindings.get(name)
        if logical_binding is None:
            continue

        parameter["logicalBinding"] = logical_binding

    reflection_path.write_text(
        json.dumps(reflection, indent=4) + "\n",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True)
    parser.add_argument("--reflection", required=True)
    args = parser.parse_args()

    enrich_reflection(pathlib.Path(args.source), pathlib.Path(args.reflection))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
