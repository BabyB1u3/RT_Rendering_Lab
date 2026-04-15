#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import PurePosixPath


SOURCE_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".m", ".mm"}
IGNORE_FILE = ".clang-format-ignore"


def load_ignore_patterns() -> list[str]:
    patterns: list[str] = []

    try:
        with open(IGNORE_FILE, "r", encoding="utf-8") as handle:
            for raw_line in handle:
                line = raw_line.strip()
                if not line or line.startswith("#"):
                    continue
                patterns.append(line.replace("\\", "/"))
    except FileNotFoundError:
        pass

    return patterns


def is_ignored(path: str, ignore_patterns: list[str]) -> bool:
    posix_path = path.replace("\\", "/")
    pure_path = PurePosixPath(posix_path)
    return any(pure_path.match(pattern) for pattern in ignore_patterns)


def is_supported_source(path: str) -> bool:
    return PurePosixPath(path).suffix.lower() in SOURCE_EXTENSIONS


def collect_tracked_files(ignore_patterns: list[str]) -> list[str]:
    result = subprocess.run(
        ["git", "ls-files"],
        check=True,
        capture_output=True,
        text=True,
    )

    files: list[str] = []
    for line in result.stdout.splitlines():
        normalized = line.strip().replace("\\", "/")
        if not normalized:
            continue
        if not is_supported_source(normalized):
            continue
        if is_ignored(normalized, ignore_patterns):
            continue
        files.append(normalized)

    return files


def collect_explicit_files(paths: list[str], ignore_patterns: list[str]) -> list[str]:
    files: list[str] = []

    for raw_path in paths:
        normalized = raw_path.strip().replace("\\", "/")
        if not normalized:
            continue
        if not is_supported_source(normalized):
            continue
        if is_ignored(normalized, ignore_patterns):
            continue
        files.append(normalized)

    return files


def run_clang_format(files: list[str], check_only: bool) -> int:
    clang_format = shutil.which("clang-format")
    if clang_format is None:
        print("clang-format was not found on PATH.", file=sys.stderr)
        return 1

    if not files:
        return 0

    command = [clang_format]
    if check_only:
        command.extend(["--dry-run", "--Werror"])
    else:
        command.append("-i")

    command.extend(files)
    completed = subprocess.run(command)
    return completed.returncode


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run clang-format for pre-commit hooks.")
    parser.add_argument("--check", action="store_true", help="Verify formatting without modifying files.")
    parser.add_argument("--all-files", action="store_true", help="Run against all tracked source files.")
    parser.add_argument("files", nargs="*", help="Files passed from pre-commit.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    ignore_patterns = load_ignore_patterns()

    if args.all_files:
        files = collect_tracked_files(ignore_patterns)
    else:
        files = collect_explicit_files(args.files, ignore_patterns)

    return run_clang_format(files, check_only=args.check)


if __name__ == "__main__":
    raise SystemExit(main())
