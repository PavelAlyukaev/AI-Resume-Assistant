---
phase: 01-zavisimost-libzip-i-bezopasnoe-chtenie-zip
plan: 02
subsystem: documents
status: complete
tags: [libzip, zip, cfb, ole2, docx, error-codes, qt-test, mingw]

# Dependency graph
requires:
  - phase: 01-zavisimost-libzip-i-bezopasnoe-chtenie-zip
    provides: "plan 01-01: ZipArchive (read-only in-memory ZIP), DocxError (seven codes), libzip linked into resume_documents"
provides:
  - "readDocxParts(QByteArray): classification by signature, then word/document.xml (required) and word/styles.xml, word/numbering.xml (optional, with warnings)"
  - "DocxWarning: StylesPartMissing, NumberingPartMissing"
  - "Limit constants kMaxFileBytes, kMaxUnpackedBytes, kMaxEntries, kMaxCompressionRatio, kCompressionCheckAfterBytes, kMaxTextChars (values pinned, enforcement per plan)"
  - "ZipArchive::readEntry maps libzip password failures to DocxError::Encrypted"
  - "tests/DocxFixtures.h: shared builders (CRC-32, stored ZIP, CFB, libzip writer with AES-256 entry)"
  - "src/documents/README.md: layer description, codes, limits, rules"
affects: [01-03, 01-04, phase-02-docx-reader, phase-03-open-in-window]

# Actuals (#2632): chars/4 over the realized diff (git diff 70f5be0 HEAD, 42416 characters)
actuals:
  tokens: 10604
  tasks: 3
  commits: 3      # MEASURED: git rev-list --count 70f5be0..HEAD at write time (summary commit follows)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "classify by magic bytes before any libzip call: CFB and ZIP signatures decide the branch"
    - "CFB EncryptedPackage search: UTF-16LE built byte by byte, bounds-checked QByteArray::contains"
    - "one shared unpacked budget across the three parts in one open; the budget is consumed by bytes actually returned"
    - "test fixtures built in code in a shared header; libzip writer uses a QTemporaryDir (no binary fixtures committed)"

key-files:
  created:
    - src/documents/DocxContainer.h
    - src/documents/DocxContainer.cpp
    - tests/DocxFixtures.h
  modified:
    - src/documents/DocxTypes.h
    - src/documents/ZipArchive.cpp
    - src/documents/README.md
    - tests/ZipArchiveTest.cpp
    - CMakeLists.txt
    - docs/memory.md

key-decisions:
  - "Classification order: size check (TooLarge), CFB signature (Encrypted or LegacyDoc), ZIP signatures (otherwise NotZip). Only ZIP-prefixed bytes reach libzip."
  - "A ZIP-prefixed input that libzip rejects is Corrupt, not NotZip (research Pitfall 5)."
  - "libzip zip_fopen_index failures: ZIP_ER_NOPASSWD and ZIP_ER_WRONGPASSWD map to Encrypted; every other failure stays Corrupt. No libzip message text is stored or returned."
  - "The three parts share one budget of kMaxUnpackedBytes (20 MiB) in one open. This reinterprets D-02, which names only word/document.xml. OWNER-PENDING."
  - "MB in D-01 and D-02 is read as MiB (1024 * 1024 bytes). OWNER-PENDING."
  - "Source ownership after a failed zip_open_from_source stays as plan 01-01 decided (branch c: not freed). Open question, unchanged."
  - "kMaxEntries (1000), kMaxCompressionRatio (100) and kCompressionCheckAfterBytes (1 MiB) are planner values; they are enforced in plan 01-04 and still need owner confirmation."

patterns-established:
  - "Codes only at the documents layer: DocxError and DocxWarning carry no text; the application layer (plan 01-03) owns user-facing messages"
  - "No path API: readDocxParts takes bytes; the caller reads the file with QFile"

requirements-completed: [SAFE-01]  # documents-layer part of SAFE-01; the file-level size check is plan 01-03, the application-layer texts are plan 01-03/01-04

# Coverage metadata (#1602)
coverage:
  - id: D1
    description: "readDocxParts classifies NotZip (text, zero-byte), Encrypted and LegacyDoc (CFB), Corrupt (ZIP prefix rejected by libzip), MissingPart (no word/document.xml), Encrypted (libzip password entry)"
    requirement: "SAFE-01"
    verification:
      - kind: unit
        ref: "tests/ZipArchiveTest.cpp#plainTextIsNotZip, zeroByteInputIsNotZip, cfbWithEncryptedPackageIsEncrypted, cfbWithoutEncryptedPackageIsLegacyDoc, pkHeaderOnlyIsCorrupt, zipWithoutDocumentIsMissingPart, libzipEncryptedEntryIsEncrypted"
        status: pass
    human_judgment: false
  - id: D2
    description: "Damaged document entry returns Corrupt with no partial bytes; oversized input returns TooLarge; missing optional parts give warnings"
    requirement: "SAFE-01"
    verification:
      - kind: unit
        ref: "tests/ZipArchiveTest.cpp#damagedDocumentPartRefusesPartialResult, inputAboveFileLimitIsTooLarge, missingStylesAndNumberingGiveWarnings"
        status: pass
    human_judgment: false

---

# Phase 1 Plan 02: Container classification and three-part read Summary

**readDocxParts classifies input by magic bytes (ZIP, CFB, other), reads word/document.xml with styles and numbering as optional parts, and returns one code per failure, with 17 of 17 zip_archive cases passing.**

## Performance

- **Tasks:** 3 of 3 completed (three atomic commits)
- **Files:** 9 changed (563 insertions, 107 deletions relative to 70f5be0)
- **Result branch:** `worktree-agent-a89394bad9b409018`

## Accomplishments

- `readDocxParts` returns `DocxParts` (three QByteArrays plus a warnings list) or a `DocxError`, and never a partial result (D-07).
- CFB containers are split on the UTF-16LE name `EncryptedPackage`: present gives Encrypted, absent gives LegacyDoc (D-06). The check runs before any libzip call.
- libzip's password failures on an entry map to Encrypted. Verified with an entry written by libzip with AES-256 and read back.
- Missing styles or numbering produce `StylesPartMissing` or `NumberingPartMissing`; the list size is the count (D-08, D-09).
- One unpacked budget covers the three parts within one open. A part whose read fails fails the whole call.
- Shared fixtures in `tests/DocxFixtures.h`; no binary fixture is committed.
- `src/documents/README.md` replaces the stub with the layer rules, the code tables, and the limit table.

## Task Commits

1. **Task 1: tracer, NotZip and the three-part read** - `171ef9c` (feat)
2. **Task 2: CFB classification, libzip password mapping, shared fixtures** - `8d57c14` (feat)
3. **Task 3: defensive tests, README, memory entry** - `f6db0a2` (test)

Summary commit: written after these three; its hash is reported in the completion message.

## Must-haves check

| Truth (from plan) | Covered by test |
| --- | --- |
| NotZip for non-ZIP, non-CFB input, including zero bytes | `plainTextIsNotZip`, `zeroByteInputIsNotZip` |
| CFB with EncryptedPackage gives Encrypted; other CFB gives LegacyDoc | `cfbWithEncryptedPackageIsEncrypted`, `cfbWithoutEncryptedPackageIsLegacyDoc` |
| ZIP-prefixed input rejected by libzip gives Corrupt; no word/document.xml gives MissingPart; password entry gives Encrypted | `pkHeaderOnlyIsCorrupt`, `zipWithoutDocumentIsMissingPart`, `libzipEncryptedEntryIsEncrypted` |
| Damaged entry gives an error and no bytes (D-07), via one flipped byte that fails CRC | `damagedDocumentPartRefusesPartialResult` |
| Missing styles or numbering gives warnings with the count as list size | `missingStylesAndNumberingGiveWarnings` |
| Input above 10 MiB gives TooLarge at the container | `inputAboveFileLimitIsTooLarge` |

## Verification output (real runs, this worktree)

Build, `cmake --build --preset debug` (final run, after the last edit): exit 0, no warnings or errors in the log.

Full suite, `ctest --preset debug` (final run):
```
100% tests passed out of 4

Total Test time (real) =   0.70 sec
```
(Individual targets: resume_e2e, resume_json, zip_archive, web_api all Passed.)

zip_archive QTest output (binary run with `-o build/mingw-debug/zip_final.txt,txt`):
```
PASS   : ZipArchiveTest::openAndReadExactEntry()
PASS   : ZipArchiveTest::readEntryEnforcesByteBudget()
PASS   : ZipArchiveTest::exactNameMissingIsMissingPart()
PASS   : ZipArchiveTest::truncatedAndGarbageInputsAreCorrupt()
PASS   : ZipArchiveTest::docxPartsReadDocumentStylesAndNumbering()
PASS   : ZipArchiveTest::plainTextIsNotZip()
PASS   : ZipArchiveTest::missingStylesAndNumberingGiveWarnings()
PASS   : ZipArchiveTest::cfbWithEncryptedPackageIsEncrypted()
PASS   : ZipArchiveTest::cfbWithoutEncryptedPackageIsLegacyDoc()
PASS   : ZipArchiveTest::pkHeaderOnlyIsCorrupt()
PASS   : ZipArchiveTest::zipWithoutDocumentIsMissingPart()
PASS   : ZipArchiveTest::libzipEncryptedEntryIsEncrypted()
PASS   : ZipArchiveTest::zeroByteInputIsNotZip()
PASS   : ZipArchiveTest::inputAboveFileLimitIsTooLarge()
PASS   : ZipArchiveTest::damagedDocumentPartRefusesPartialResult()
Totals: 17 passed, 0 failed, 0 skipped, 0 blacklisted, 16ms
```
(initTestCase and cleanupTestCase also passed. This is the run after the last code edit: zip_final.txt.)

Format: `clang-format --dry-run --Werror` on DocxTypes.h, DocxContainer.h/.cpp, ZipArchive.cpp, DocxFixtures.h, ZipArchiveTest.cpp: exit 0.

Lint: `clang-tidy src/documents/DocxContainer.cpp src/documents/ZipArchive.cpp -p build/mingw-debug` ran with the compile database. Findings are style checks from `.clang-tidy` (modernize-use-trailing-return-type, performance-enum-size, modernize-use-nodiscard), the same kind of finding as plan 01-01's code. Not fixed here: changing them means touching 01-01 code and the project style is not decided. The bugprone widening warnings in the new DocxTypes.h were fixed (`qint64{10} * 1024 * 1024`).

## Deviations from Plan

### Process deviations

**1. Worktree branch was missing; recreated from the 01-01 merge**
- **Found during:** base check at start, and again after the coordinator's resume message
- **Issue:** The assigned branch `worktree-agent-a89394bad9b409018` and its worktree directory did not exist. The main checkout was on `chore/map-codebase`, the owner's working branch, which the rules forbid committing to.
- **Fix:** `git worktree add .claude/worktrees/agent-a89394bad9b409018 -b worktree-agent-a89394bad9b409018 70f5be0`. The base check then passed (ZipArchive.h/.cpp, tests/ZipArchiveTest.cpp, 01-01-SUMMARY.md, commit 70f5be0 an ancestor). No 01-02 commits existed anywhere before this run, so no task was redone.
- **Other worktree:** `.claude/worktrees/agent-ae75ac1f74e7edc04` (another branch) was not touched.

### Auto-fixed Issues

**2. [Rule 2 - Missing critical] Test helpers and expected codes**
- The plan's Task 2 said to remove the moved helpers from ZipArchiveTest.cpp. Done; they now live in `tests/DocxFixtures.h` under `namespace fixtures`, with `using namespace fixtures;` in the test file. This is a test-only namespace choice.

**3. [Rule 1 - Bug] Formatting violations in new test lines**
- **Found during:** format check after Task 1. Three QCOMPARE lines and an include order were not clang-formatted.
- **Fix:** `clang-format -i` on the test file; only new lines changed. Committed in Task 1.

**4. [Rule 1 - Bug] Widening warnings in DocxTypes.h**
- **Found during:** clang-tidy run.
- **Fix:** `qint64{10} * 1024 * 1024` form for the three limit constants. No value changed. Committed in Task 3.

No other deviations. The plan's code direction (classification order, budget scope, codes, warnings) was implemented as written.

## Auth gates

None.

## Known Stubs

None. Every code the plan names is produced and tested.

## Threat Flags

None new. The CFB scan (T-01-08) is a bounds-checked `QByteArray::contains` over the input and takes no offset from the file. The libzip password mapping keeps no message text (T-01-04).

## Deferred Items and Open Questions

- **OWNER-PENDING: "MB" reading.** D-01 and D-02 are read as MiB (1024 * 1024). Recorded in DocxTypes.h, README, and the memory entry.
- **OWNER-PENDING: D-02 scope.** D-02 as written names only word/document.xml. This plan applies one 20 MiB budget shared by the three parts in one open. Recorded in DocxTypes.h, README, and the memory entry; to be recorded in plan 01-04.
- **Planner values, plan 01-04:** kMaxEntries = 1000, kMaxCompressionRatio = 100, kCompressionCheckAfterBytes = 1 MiB. Not enforced in this plan.
- **libzip source ownership (plan 01-01, branch c).** A failed `zip_open_from_source` does not free the source. Unchanged; open question.
- **Plan 01-03** must add the file-level size check before opening (D-01 at file level) and the application-layer texts.
- **QTest stdout (environment note).** The zip_archive binary prints nothing to stdout in this environment; results were captured with `-o <file>,txt`. ctest reports pass/fail only.
- **Not verified here:** CI, `cmake --install` and the Inno Setup installer, release build, web_e2e (skipped: no `tests/e2e/node_modules`).
- **Not pushed.** The branch lives only in this worktree.

## Self-Check

Checked after writing the code and before this file:
- FOUND: src/documents/DocxContainer.h, src/documents/DocxContainer.cpp, tests/DocxFixtures.h, src/documents/README.md, src/documents/DocxTypes.h, tests/ZipArchiveTest.cpp
- FOUND commits: 171ef9c, 8d57c14, f6db0a2 (all ancestors of HEAD)
- Working tree clean apart from ignored `build/`.

## Self-Check: PASSED
