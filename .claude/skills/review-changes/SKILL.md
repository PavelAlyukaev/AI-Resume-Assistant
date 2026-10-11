---
name: review-changes
description: Review the current git diff for bugs, security risks, C++23/Qt conventions and missing tests.
disable-model-invocation: true
---
# Review changes
1. Inspect `git status --short` and `git diff` without modifying files.
2. Identify bugs first, ordered by severity, with file/line references.
3. Check SQL parameterization, Qt ownership, error handling, privacy and `.env` protections.
4. Check test coverage and build/lint status; never invent a successful run.
5. Provide findings and remaining risks. Do not auto-commit or edit unless asked.
