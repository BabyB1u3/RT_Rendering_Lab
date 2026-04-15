#!/usr/bin/env python3
from __future__ import annotations

import argparse
import subprocess
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Iterable


SOURCE_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".m", ".mm"}
IGNORE_FILE = ".clang-format-ignore"
DEFAULT_SCAN_ROOTS = ("Src", "Tests")
CONTROL_KEYWORDS = {
    "if",
    "for",
    "while",
    "switch",
    "catch",
    "return",
    "sizeof",
    "alignof",
    "decltype",
    "static_assert",
}
BOOLEAN_PREFIXES = ("is", "has", "can", "should", "was", "needs")


@dataclass
class Diagnostic:
    path: str
    line: int
    rule: str
    symbol: str
    expected: str
    message: str


@dataclass
class Scope:
    name: str
    kind: str
    brace_depth: int


@dataclass
class StripState:
    in_block_comment: bool = False
    in_string: bool = False
    string_delimiter: str = ""
    escape: bool = False


@dataclass
class VariableDeclaration:
    name: str
    prefix: str


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


def is_supported_source(path: str) -> bool:
    return PurePosixPath(path).suffix.lower() in SOURCE_EXTENSIONS


def is_ignored(path: str, ignore_patterns: list[str]) -> bool:
    posix_path = path.replace("\\", "/")
    pure_path = PurePosixPath(posix_path)
    return any(pure_path.match(pattern) for pattern in ignore_patterns)


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
    files: set[str] = set()

    for raw_path in paths:
        path = Path(raw_path)
        if path.is_dir():
            for child in path.rglob("*"):
                if not child.is_file():
                    continue
                normalized = child.as_posix()
                if not is_supported_source(normalized):
                    continue
                if is_ignored(normalized, ignore_patterns):
                    continue
                files.add(normalized)
            continue

        normalized = path.as_posix()
        if not is_supported_source(normalized):
            continue
        if is_ignored(normalized, ignore_patterns):
            continue
        files.add(normalized)

    return sorted(files)


def sanitize_line(line: str, state: StripState) -> str:
    result: list[str] = []
    index = 0

    while index < len(line):
        char = line[index]
        next_char = line[index + 1] if index + 1 < len(line) else ""

        if state.in_block_comment:
            result.append(" ")
            if char == "*" and next_char == "/":
                result.append(" ")
                state.in_block_comment = False
                index += 2
            else:
                index += 1
            continue

        if state.in_string:
            result.append(" ")
            if state.escape:
                state.escape = False
            elif char == "\\":
                state.escape = True
            elif char == state.string_delimiter:
                state.in_string = False
                state.string_delimiter = ""
            index += 1
            continue

        if char == "/" and next_char == "*":
            result.extend([" ", " "])
            state.in_block_comment = True
            index += 2
            continue

        if char == "/" and next_char == "/":
            result.extend(" " for _ in line[index:])
            break

        if char in {"'", '"'}:
            result.append(" ")
            state.in_string = True
            state.string_delimiter = char
            state.escape = False
            index += 1
            continue

        result.append(char)
        index += 1

    return "".join(result)


def is_pascal_case(name: str) -> bool:
    if not name:
        return False
    if "_" in name:
        return False
    if not name[0].isalpha() or not name[0].isupper():
        return False
    return all(char.isalnum() for char in name)


def is_lower_camel_case(name: str) -> bool:
    if not name:
        return False
    if "_" in name:
        return False
    if not name[0].isalpha() or not name[0].islower():
        return False
    return all(char.isalnum() for char in name)


def is_prefixed_pascal_case(name: str, prefix: str) -> bool:
    if not name.startswith(prefix):
        return False
    suffix = name[len(prefix):]
    return is_pascal_case(suffix)


def is_boolean_predicate(name: str) -> bool:
    return name.startswith(BOOLEAN_PREFIXES)


def count_char_outside_templates(text: str, target: str) -> int:
    depth = 0
    count = 0

    for char in text:
        if char == "<":
            depth += 1
        elif char == ">":
            depth = max(0, depth - 1)
        elif char == target and depth == 0:
            count += 1

    return count


def split_top_level(text: str, delimiter: str) -> list[str]:
    parts: list[str] = []
    current: list[str] = []
    paren_depth = 0
    angle_depth = 0
    square_depth = 0
    brace_depth = 0

    for char in text:
        if char == "(":
            paren_depth += 1
        elif char == ")":
            paren_depth = max(0, paren_depth - 1)
        elif char == "<":
            angle_depth += 1
        elif char == ">":
            angle_depth = max(0, angle_depth - 1)
        elif char == "[":
            square_depth += 1
        elif char == "]":
            square_depth = max(0, square_depth - 1)
        elif char == "{":
            brace_depth += 1
        elif char == "}":
            brace_depth = max(0, brace_depth - 1)

        if (
            char == delimiter
            and paren_depth == 0
            and angle_depth == 0
            and square_depth == 0
            and brace_depth == 0
        ):
            parts.append("".join(current))
            current = []
            continue

        current.append(char)

    parts.append("".join(current))
    return parts


def find_top_level_first(text: str, candidates: str) -> int:
    paren_depth = 0
    angle_depth = 0
    square_depth = 0
    brace_depth = 0

    for index, char in enumerate(text):
        if char == "(":
            paren_depth += 1
        elif char == ")":
            paren_depth = max(0, paren_depth - 1)
        elif char == "<":
            angle_depth += 1
        elif char == ">":
            angle_depth = max(0, angle_depth - 1)
        elif char == "[":
            square_depth += 1
        elif char == "]":
            square_depth = max(0, square_depth - 1)
        elif char == "{":
            brace_depth += 1
        elif char == "}":
            brace_depth = max(0, brace_depth - 1)

        if (
            char in candidates
            and paren_depth == 0
            and angle_depth == 0
            and square_depth == 0
            and brace_depth == 0
        ):
            return index

    return -1


def extract_identifier_from_declaration(declaration: str) -> str | None:
    match = None

    function_pointer = None
    if "(*" in declaration or "(&" in declaration:
        import re

        function_pointer = re.search(r"\(\s*[*&]\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)", declaration)
    if function_pointer:
        return function_pointer.group(1)

    import re

    match = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*$", declaration)
    if match:
        return match.group(1)

    return None


def build_case_message(symbol: str, expected: str) -> str:
    return f"'{symbol}' should match {expected}"


def append_case_diagnostic(
    diagnostics: list[Diagnostic],
    path: str,
    line: int,
    rule: str,
    symbol: str,
    expected: str,
) -> None:
    diagnostics.append(
        Diagnostic(
            path=path,
            line=line,
            rule=rule,
            symbol=symbol,
            expected=expected,
            message=build_case_message(symbol, expected),
        )
    )


def check_pascal(
    diagnostics: list[Diagnostic],
    path: str,
    line: int,
    rule: str,
    symbol: str,
) -> None:
    if not is_pascal_case(symbol):
        append_case_diagnostic(diagnostics, path, line, rule, symbol, "PascalCase")


def check_lower_camel(
    diagnostics: list[Diagnostic],
    path: str,
    line: int,
    rule: str,
    symbol: str,
) -> None:
    if not is_lower_camel_case(symbol):
        append_case_diagnostic(diagnostics, path, line, rule, symbol, "lowerCamelCase")


def check_prefixed_pascal(
    diagnostics: list[Diagnostic],
    path: str,
    line: int,
    rule: str,
    symbol: str,
    prefix: str,
) -> None:
    if not is_prefixed_pascal_case(symbol, prefix):
        append_case_diagnostic(diagnostics, path, line, rule, symbol, f"'{prefix}' + PascalCase")


def maybe_check_boolean_name(
    diagnostics: list[Diagnostic],
    path: str,
    line: int,
    rule: str,
    symbol: str,
) -> None:
    if not is_boolean_predicate(symbol):
        diagnostics.append(
            Diagnostic(
                path=path,
                line=line,
                rule=rule,
                symbol=symbol,
                expected="predicate-style boolean name",
                message=f"'{symbol}' should start with one of: {', '.join(BOOLEAN_PREFIXES)}",
            )
        )


def extract_parameters(parameter_text: str) -> list[str]:
    parameters: list[str] = []
    for raw_param in split_top_level(parameter_text, ","):
        parameter = raw_param.strip()
        if not parameter or parameter in {"void", "..."}:
            continue
        parameters.append(parameter)
    return parameters


def normalize_statement(statement: str) -> str:
    return " ".join(statement.split())


def should_start_signature(line: str, has_function_scope: bool) -> bool:
    if has_function_scope:
        return False

    stripped = line.strip()
    if not stripped or stripped.startswith("#"):
        return False
    if "(" not in stripped:
        return False
    if stripped.endswith(":"):
        return False

    prefix = stripped.split("(", 1)[0].strip()
    if not prefix:
        return False

    first_word = prefix.split()[0]
    if first_word in CONTROL_KEYWORDS:
        return False

    if "[" in prefix and "=" in prefix:
        return False

    return True


def parse_signature(
    statement: str,
    path: str,
    line: int,
    diagnostics: list[Diagnostic],
    check_bool_predicates: bool,
) -> tuple[bool, bool]:
    import re

    normalized = normalize_statement(statement)
    if not normalized:
        return False, False

    if normalized.startswith("#"):
        return False, False

    paren_index = normalized.find("(")
    if paren_index < 0:
        return False, False

    prefix = normalized[:paren_index].strip()
    if not prefix:
        return False, False

    first_word = prefix.split()[0]
    if first_word in CONTROL_KEYWORDS:
        return False, False

    if "[" in prefix and "=" in prefix:
        return False, False

    if "=" in prefix and "::" not in prefix and not prefix.startswith("operator"):
        return False, False

    name_match = re.search(r"([~A-Za-z_][A-Za-z0-9_:~]*)\s*$", prefix)
    if not name_match:
        return False, False

    qualified_name = name_match.group(1)
    if "operator" in qualified_name:
        return False, False

    name = qualified_name.split("::")[-1]
    bare_name = name[1:] if name.startswith("~") else name
    if not bare_name:
        return False, False

    if not is_pascal_case(bare_name):
        append_case_diagnostic(diagnostics, path, line, "function-name", name, "PascalCase")

    return_type = prefix[:name_match.start()].strip()
    is_bool_return = "bool" in return_type.split()
    if (
        check_bool_predicates
        and is_bool_return
        and not (bare_name.startswith("Get") or bare_name.startswith("Has") or bare_name.startswith("Is"))
    ):
        diagnostics.append(
            Diagnostic(
                path=path,
                line=line,
                rule="bool-function-name",
                symbol=name,
                expected="GetXxx / HasXxx / IsXxx",
                message=f"boolean-returning function '{name}' should usually start with Get, Has, or Is",
            )
        )

    parameter_text = normalized[paren_index + 1 : normalized.rfind(")")]
    for parameter in extract_parameters(parameter_text):
        param_without_default = split_top_level(parameter, "=")[0].strip()
        if not param_without_default:
            continue

        function_pointer_match = re.search(r"\(\s*[*&]\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)", param_without_default)
        if function_pointer_match:
            param_name = function_pointer_match.group(1)
        else:
            param_name = extract_identifier_from_declaration(param_without_default)

        if not param_name:
            continue

        check_lower_camel(diagnostics, path, line, "parameter-name", param_name)
        if check_bool_predicates and "bool" in param_without_default.split():
            maybe_check_boolean_name(diagnostics, path, line, "bool-parameter-name", param_name)

    opens_scope = "{" in normalized
    waits_for_scope = normalized.endswith(")") or normalized.endswith("const") or normalized.endswith("noexcept")
    return True, opens_scope or waits_for_scope


def detect_variable_declaration(line: str) -> VariableDeclaration | None:
    import re

    lambda_match = re.search(r"^(.*?)\b([A-Za-z_][A-Za-z0-9_]*)\s*=\s*\[[^\]]*\]", line.strip())
    if lambda_match:
        declaration = lambda_match.group(1).strip()
        if not declaration:
            return None
        return VariableDeclaration(name=lambda_match.group(2), prefix=declaration)

    init_index = find_top_level_first(line, "={;")
    declaration = line if init_index < 0 else line[:init_index]
    declaration = declaration.rstrip().rstrip(";").rstrip()
    if not declaration:
        return None

    if declaration.endswith(")") or declaration.endswith("}"):
        return None

    if "," in split_top_level(declaration, ",")[0] and len(split_top_level(declaration, ",")) > 1:
        return None

    name = extract_identifier_from_declaration(declaration)
    if not name:
        return None

    name_start = declaration.rfind(name)
    prefix = declaration[:name_start].strip()
    if not prefix:
        return None

    if "->" in prefix or "." in prefix:
        return None

    if prefix.endswith(("=", "+", "-", "*", "/", "%", "&", "|", "^", "!", "~", "?", ":")):
        return None

    first_token = prefix.split()[0]
    if first_token in CONTROL_KEYWORDS:
        return None

    return VariableDeclaration(name=name, prefix=prefix)


def is_variable_candidate(line: str) -> bool:
    stripped = line.strip()
    if not stripped or not stripped.endswith(";"):
        return False
    if stripped.startswith("#"):
        return False
    if stripped.endswith(":"):
        return False
    if stripped.startswith(("return ", "break", "continue", "goto ", "using ", "typedef ", "friend ")):
        return False
    if stripped in {"public:", "private:", "protected:"}:
        return False
    if stripped.startswith("static_assert"):
        return False
    return True


def classify_variable_rule(
    line: str,
    declaration_prefix: str,
    has_type_scope: bool,
    has_function_scope: bool,
) -> tuple[str, str] | None:
    import re

    stripped = line.strip()
    tokens = stripped.replace(";", " ").replace("{", " ").replace("}", " ").split()
    is_static = "static" in tokens or "inline" in tokens and "static" in stripped
    is_const = "const" in tokens or "constexpr" in tokens or "consteval" in tokens
    is_constexpr = "constexpr" in tokens or "consteval" in tokens

    if has_type_scope and not has_function_scope:
        if is_static and is_const:
            return "class-constant-name", "k_"
        if is_static:
            return "static-member-name", "s_"
        return "member-field-name", "m_"

    if has_function_scope:
        if is_static and is_const:
            return "local-constant-name", "k_"
        if is_static:
            return "static-local-name", "s_"
        if is_constexpr:
            return "local-constant-name", "k_"
        return "local-variable-name", ""

    if re.search(r"[A-Za-z_][A-Za-z0-9_]*::\s*$", declaration_prefix):
        if is_const:
            return "class-constant-name", "k_"
        return "static-member-name", "s_"

    if is_const:
        return "global-constant-name", "k_"
    return "global-variable-name", "g_"


def scan_file(path: str, check_bool_predicates: bool) -> list[Diagnostic]:
    diagnostics: list[Diagnostic] = []

    try:
        raw_lines = Path(path).read_text(encoding="utf-8").splitlines()
    except UnicodeDecodeError:
        raw_lines = Path(path).read_text(encoding="utf-8-sig").splitlines()

    state = StripState()
    brace_depth = 0
    type_scopes: list[Scope] = []
    function_scopes: list[Scope] = []
    pending_type_scope: tuple[str, str] | None = None
    pending_function_scope: str | None = None
    signature_lines: list[str] = []
    signature_start_line = 0

    import re

    for index, raw_line in enumerate(raw_lines, start=1):
        line = sanitize_line(raw_line, state)
        stripped = line.strip()

        if signature_lines:
            signature_lines.append(line)
            combined = " ".join(signature_lines)
            if count_char_outside_templates(combined, "(") <= count_char_outside_templates(combined, ")"):
                is_signature, opens_scope = parse_signature(
                    combined,
                    path,
                    signature_start_line,
                    diagnostics,
                    check_bool_predicates,
                )
                if is_signature:
                    if "{" in combined:
                        pending_function_scope = "<function>"
                    elif opens_scope:
                        pending_function_scope = "<function>"
                signature_lines = []
            continue

        if should_start_signature(line, has_function_scope=bool(function_scopes)):
            signature_lines = [line]
            signature_start_line = index
            if count_char_outside_templates(line, "(") <= count_char_outside_templates(line, ")"):
                combined = " ".join(signature_lines)
                is_signature, opens_scope = parse_signature(
                    combined,
                    path,
                    signature_start_line,
                    diagnostics,
                    check_bool_predicates,
                )
                if is_signature:
                    if "{" in combined:
                        pending_function_scope = "<function>"
                    elif opens_scope:
                        pending_function_scope = "<function>"
                signature_lines = []
            continue

        namespace_match = re.match(r"^\s*namespace\s+([A-Za-z_][A-Za-z0-9_:]*)", line)
        if namespace_match:
            qualified_namespace = namespace_match.group(1)
            for namespace_segment in qualified_namespace.split("::"):
                check_pascal(diagnostics, path, index, "namespace-name", namespace_segment)

        type_match = re.match(r"^\s*(class|struct|enum(?:\s+class)?)\s+([A-Za-z_][A-Za-z0-9_]*)", line)
        if type_match:
            type_name = type_match.group(2)
            check_pascal(diagnostics, path, index, "type-name", type_name)
            if type_name.startswith("I") and len(type_name) > 1 and type_name[1].isupper():
                diagnostics.append(
                    Diagnostic(
                        path=path,
                        line=index,
                        rule="interface-prefix",
                        symbol=type_name,
                        expected="type names without an I prefix",
                        message=f"type '{type_name}' should not use an I prefix",
                    )
                )

            if "{" in line:
                pending_type_scope = (type_name, type_match.group(1))
            elif stripped and not stripped.endswith(";"):
                pending_type_scope = (type_name, type_match.group(1))

        using_match = re.match(r"^\s*using\s+([A-Za-z_][A-Za-z0-9_]*)\s*=", line)
        if using_match:
            check_pascal(diagnostics, path, index, "type-alias-name", using_match.group(1))

        if is_variable_candidate(line):
            declaration = detect_variable_declaration(line)
            if declaration:
                rule_info = classify_variable_rule(
                    line,
                    declaration.prefix,
                    has_type_scope=bool(type_scopes),
                    has_function_scope=bool(function_scopes),
                )
                if rule_info is not None:
                    rule, prefix = rule_info
                    if prefix:
                        check_prefixed_pascal(diagnostics, path, index, rule, declaration.name, prefix)
                    else:
                        check_lower_camel(diagnostics, path, index, rule, declaration.name)

                    tokens = stripped.replace(";", " ").replace("{", " ").replace("}", " ").split()
                    if check_bool_predicates and "bool" in tokens:
                        bare_name = declaration.name
                        if prefix and declaration.name.startswith(prefix):
                            bare_name = declaration.name[len(prefix):]
                            bare_name = bare_name[:1].lower() + bare_name[1:] if bare_name else bare_name
                        maybe_check_boolean_name(diagnostics, path, index, f"bool-{rule}", bare_name)

        opens_on_this_line = stripped == "{"

        brace_depth += line.count("{")
        brace_depth -= line.count("}")

        if pending_type_scope and ("{" in line or opens_on_this_line):
            type_scopes.append(Scope(name=pending_type_scope[0], kind=pending_type_scope[1], brace_depth=brace_depth))
            pending_type_scope = None

        if pending_function_scope and ("{" in line or opens_on_this_line):
            function_scopes.append(Scope(name=pending_function_scope, kind="function", brace_depth=brace_depth))
            pending_function_scope = None

        while function_scopes and brace_depth < function_scopes[-1].brace_depth:
            function_scopes.pop()

        while type_scopes and brace_depth < type_scopes[-1].brace_depth:
            type_scopes.pop()

    return diagnostics


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Best-effort naming convention audit for repository-owned code.")
    parser.add_argument("--all-files", action="store_true", help="Scan all tracked source files.")
    parser.add_argument(
        "--summary-only",
        action="store_true",
        help="Only print the final summary.",
    )
    parser.add_argument(
        "paths",
        nargs="*",
        help="Files or directories to scan. Defaults to Src/ and Tests/.",
    )
    parser.add_argument(
        "--check-bool-predicates",
        action="store_true",
        help="Also audit bool names for predicate-style prefixes.",
    )
    return parser.parse_args()


def print_diagnostics(diagnostics: Iterable[Diagnostic], summary_only: bool) -> None:
    diagnostics = list(diagnostics)
    if not summary_only:
        for diagnostic in diagnostics:
            print(f"{diagnostic.path}:{diagnostic.line}: {diagnostic.rule}: {diagnostic.message}")

    if not diagnostics:
        print("Naming audit passed with no diagnostics.")
        return

    counts = Counter(diagnostic.rule for diagnostic in diagnostics)
    print()
    print(f"Naming audit found {len(diagnostics)} diagnostic(s) across {len({d.path for d in diagnostics})} file(s).")
    for rule, count in sorted(counts.items()):
        print(f"  {rule}: {count}")


def main() -> int:
    args = parse_args()
    ignore_patterns = load_ignore_patterns()

    if args.all_files:
        files = collect_tracked_files(ignore_patterns)
    else:
        roots = list(args.paths) if args.paths else list(DEFAULT_SCAN_ROOTS)
        files = collect_explicit_files(roots, ignore_patterns)

    diagnostics: list[Diagnostic] = []
    for path in files:
        diagnostics.extend(scan_file(path, check_bool_predicates=args.check_bool_predicates))

    diagnostics.sort(key=lambda item: (item.path, item.line, item.rule, item.symbol))
    print_diagnostics(diagnostics, summary_only=args.summary_only)
    return 1 if diagnostics else 0


if __name__ == "__main__":
    raise SystemExit(main())
