---
name: write-tests
description: Write or update Qt Test tests for a requested C++ component; use for new behavior and bug fixes.
disable-model-invocation: false
---
# Write tests
1. Read the target component and existing tests; identify behavior and edge cases.
2. Add focused Qt Test cases using deterministic, synthetic data; no external APIs or secrets.
3. Include happy path, invalid input, and persistence/restart cases when applicable.
4. Run `cmake --build --preset debug` and `ctest --preset debug` if toolchain exists.
5. Report exact commands and results; if unavailable, mark tests as NOT RUN.
6. Do not silently change production behavior to make tests pass.
