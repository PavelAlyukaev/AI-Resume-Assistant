#!/usr/bin/env python3
"""Non-destructive formatting check after C++ edits."""
import json
import pathlib
import shutil
import subprocess
import sys
try:
    event = json.load(sys.stdin)
except (ValueError, OSError):
    sys.exit(0)
path = event.get("tool_input", {}).get("file_path", "")
if not isinstance(path, str) or pathlib.Path(path).suffix.lower() not in {".cpp", ".h", ".hpp", ".cc"}:
    sys.exit(0)
if not shutil.which("clang-format"):
    print("clang-format not installed; formatting check skipped", file=sys.stderr)
    sys.exit(0)
if not pathlib.Path(path).is_file():
    sys.exit(0)
result = subprocess.run(["clang-format", "--dry-run", "--Werror", path], capture_output=True, text=True)
if result.returncode:
    print("Formatting check failed:\n" + result.stderr, file=sys.stderr)
    sys.exit(2)
