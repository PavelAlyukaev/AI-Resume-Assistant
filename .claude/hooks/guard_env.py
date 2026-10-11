#!/usr/bin/env python3
"""Best-effort Claude Code guard. Not an OS sandbox; test in real Claude Code."""
import json
import re
import sys

try:
    event = json.load(sys.stdin)
except (ValueError, OSError):
    sys.exit(0)
info = event.get("tool_input", {})
if not isinstance(info, dict):
    sys.exit(0)
# Match .env / .env.local, but not .env.example (safe template).
pattern = re.compile(r"(?<![A-Za-z0-9_])\.env(?:\.(?!example\b)[\w.-]+)?\b", re.I)
for key, value in info.items():
    if key in {"command", "file_path", "path", "pattern", "glob", "content", "old_string", "new_string"}:
        if isinstance(value, str) and pattern.search(value):
            print("BLOCKED: access to .env secrets is forbidden", file=sys.stderr)
            sys.exit(2)
sys.exit(0)
