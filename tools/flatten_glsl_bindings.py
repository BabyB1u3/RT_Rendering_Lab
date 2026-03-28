#!/usr/bin/env python3
"""Flatten Slang-emitted GLSL descriptor-set bindings for the OpenGL runtime."""

from __future__ import annotations

import argparse
import pathlib
import re


# Keep this in sync with graphics/ShaderBinding.h:kOpenGLBindingSetStride.
OPENGL_SET_STRIDE = 16

_BINDING_SET_PATTERN = re.compile(
    r"layout\s*\(\s*binding\s*=\s*(\d+)\s*,\s*set\s*=\s*(\d+)\s*\)"
)
_SET_BINDING_PATTERN = re.compile(
    r"layout\s*\(\s*set\s*=\s*(\d+)\s*,\s*binding\s*=\s*(\d+)\s*\)"
)


def _flatten_match(binding: int, set_index: int) -> str:
    return f"layout(binding = {set_index * OPENGL_SET_STRIDE + binding})"


def flatten_glsl_bindings(source: str) -> str:
    source = _BINDING_SET_PATTERN.sub(
        lambda m: _flatten_match(int(m.group(1)), int(m.group(2))),
        source,
    )
    source = _SET_BINDING_PATTERN.sub(
        lambda m: _flatten_match(int(m.group(2)), int(m.group(1))),
        source,
    )
    return source


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    input_path = pathlib.Path(args.input)
    output_path = pathlib.Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        flatten_glsl_bindings(input_path.read_text(encoding="utf-8")),
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
