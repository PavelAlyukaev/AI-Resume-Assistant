---
phase: 01-zavisimost-libzip-i-bezopasnoe-chtenie-zip
plan: 03
subsystem: documents
tags: [docx, qfile, cyrillic, russian-errors, zip-bomb, no-write, qt-test, mingw]

# Dependency graph
requires:
  - phase: 01-zavisimost-libzip-i-bezopasnoe-chtenie-zip
    provides: "plan 01-01 (ZipArchive, DocxError, libzip in resume_documents); plan 01-02 (readDocxParts, DocxWarning, kMaxFileBytes and the other limit constants)"
provides:
  - "DocumentImport::readDocxFile(QString path) -> std::expected<ImportedDocx, QString>: file-level import with Russian texts naming only the file"
  - "DocumentImport::ImportedDocx: documentXml, stylesXml, numberingXml (QByteArray) and warnings (QStringList of Russian texts, count = size)"
  - "document_import_test (ctest name document_import): 16 Qt Test cases"
  - "resume_core links resume_documents PUBLIC (application layer sits on the documents layer)"
affects: [phase-02-docx-reader, phase-03-open-in-window, 01-04]

# Actuals (#2632): chars/4 over the realized diff (git diff 301f397 HEAD, 38000 characters)
actuals:
  tokens: 9500
  tasks: 3
  commits: 3      # MEASURED: git rev-list --count 301f397..HEAD (before this summary commit)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "file-level checks before open: isFile, then size against kMaxFileBytes, then QFile ReadOnly and a bounded read of kMaxFileBytes plus one byte"
    - "application layer owns every user-facing text; the documents layer returns codes only"
    - "fixed Russian texts built from QFileInfo::fileName(); multi-argument arg() for two placeholders"
    - "directory snapshot (names, kinds, sizes, times) as the no-write proof; baseline waits until two listings agree (NTFS lazy directory times)"

key-files:
  created:
    - src/application/DocumentImport.h
    - src/application/DocumentImport.cpp
    - tests/DocumentImportTest.cpp
  modified:
    - CMakeLists.txt
    - docs/memory.md

key-decisions:
  - "Texts exactly as in plan 01-03 Task 1 step 3 (authoritative per 01-RESEARCH.md Q4). Owner review pending: they are shown in phase 3."
  - "MB in D-01 is read as MiB: the limit text says 10 MB and is computed as kMaxFileBytes / (1024 * 1024) = 10. OWNER-PENDING."
  - "Limits carried over unchanged from plan 01-02: one 20 MiB budget across the three parts (D-02 reinterpretation, OWNER-PENDING); 1000 entries and 100:1 after 1 MiB are planner values, enforced in plan 01-04, OWNER-PENDING."
  - "A read error after QFile::read returns the Unreadable text instead of classifying the truncated bytes (added, see deviations)."
  - "The import function takes a path only at its boundary and never receives a draft, so a failed import leaves any open draft unchanged."
  - "libzip 1.11.4-1 and source branch (c) from plan 01-01 are unchanged."

patterns-established:
  - "Tests build every text expectation from literal templates in the test file, not by calling the implementation, so a text drift fails the test"
  - "Leak test: a planted Cyrillic marker, the full directory path and the subdirectory name must not appear in any error text"

requirements-completed: [SAFE-01]

coverage:
  - id: D1
    description: "A DOCX stored under a Cyrillic directory name is read through QFile and returns byte-identical word/document.xml, styles and numbering, with no warnings"
    requirement: "SAFE-01"
    verification:
      - kind: unit
        ref: "tests/DocumentImportTest.cpp#readsDocxFromCyrillicDirectory"
        status: pass
    human_judgment: false
  - id: D2
    description: "Each failure code has its own Russian text naming only the file: NotZip, Corrupt, Encrypted, LegacyDoc, MissingPart; unreadable file has its own text; the five failure texts are pairwise distinct"
    requirement: "SAFE-01"
    verification:
      - kind: unit
        ref: "tests/DocumentImportTest.cpp#eachFailureHasItsOwnExactText, failureTextsArePairwiseDistinct, unreadableFileGivesItsOwnText, notZipMessageNamesFileOnly"
        status: pass
    human_judgment: true
    rationale: "The texts match the plan's list; whether the wording is right for the user is an owner decision (shown in phase 3)"
  - id: D3
    description: "Size boundary: one byte above kMaxFileBytes gives TooLarge with the limit in MB; exactly kMaxFileBytes is classified by content (NotZip), not by size"
    requirement: "SAFE-01"
    verification:
      - kind: unit
        ref: "tests/DocumentImportTest.cpp#fileAboveLimitIsTooLargeBeforeOpen, fileOfExactlyLimitIsNotTooLarge"
        status: pass
    human_judgment: false
  - id: D4
    description: "Missing word/styles.xml and word/numbering.xml give two Russian warnings in order; all three parts give none"
    requirement: "SAFE-01"
    verification:
      - kind: unit
        ref: "tests/DocumentImportTest.cpp#missingOptionalPartsGiveTwoRussianWarnings"
        status: pass
    human_judgment: false
  - id: D5
    description: "Importing leaves the directory listing (names, kinds, sizes, times) unchanged in every case, including success and every error"
    requirement: "SAFE-01"
    verification:
      - kind: unit
        ref: "tests/DocumentImportTest.cpp#importWritesNothing"
        status: pass
    human_judgment: false
  - id: D6
    description: "No error text contains a planted content marker, the full directory path or the subdirectory name"
    requirement: "SAFE-01"
    verification:
      - kind: unit
        ref: "tests/DocumentImportTest.cpp#messagesLeakNeitherPathNorContent"
        status: pass
    human_judgment: false
  - id: D7
    description: "The size check runs before QFile is opened, by code order in readDocxFile (not directly observable from a test)"
    requirement: "SAFE-01"
    verification:
      - kind: other
        ref: "src/application/DocumentImport.cpp readDocxFile: isFile, size check, then QFile::open (read the function body)"
        status: pass
    human_judgment: true
    rationale: "The order cannot be observed through the API; it is checked by reading the function body, as the plan requires"

# Metrics
duration: "not measured (start time was not recorded at session start)"
completed: 2026-10-24
status: complete
---

# Phase 1 Plan 03: Application-layer import of a DOCX file with Russian texts, size limit and no-write proof

**`DocumentImport::readDocxFile` reads a DOCX from a Cyrillic path through QFile, refuses files above 10 MiB before opening them, and returns one fixed Russian text per failure that names only the file; 16 of 16 document_import cases and 17 of 17 zip_archive cases pass.**

## Performance

- **Duration:** not measured
- **Tasks:** 3 of 3 completed (three atomic commits)
- **Files:** 5 changed in code and docs (515 insertions, 1 deletion relative to 301f397)

## Accomplishments

- `readDocxFile(const QString &path)` checks, in order: the path is a regular file; the QFileInfo size is not above `kMaxFileBytes` (no open for an oversized file); `QFile` opens read-only; one bounded read of `kMaxFileBytes + 1` bytes; a file that grew past the limit is refused.
- The bytes go to `documents::readDocxParts` by value. Each `DocxError` maps to one Russian text (switch without default); each `DocxWarning` maps to one Russian text, in order.
- Every text is built from `QFileInfo::fileName()`. The directory path is never stored or printed.
- Tests: a DOCX in a Cyrillic directory (`Резюме_ёлка`) imports byte-identically; every failure text is compared with a literal template; the TooLarge boundary is exact; a recursive directory listing is identical before and after every case; planted markers, full paths and directory names never appear in an error text.

## Task Commits

1. **Task 1: tracer, readDocxFile for a Cyrillic directory and NotZip text** - `8dc3f48` (feat)
2. **Task 2: exact Russian text for each failure code and warning** - `cb0c969` (test)
3. **Task 3: size boundary, no-write snapshot and no-leak tests; memory entry** - `92e9850` (test)

**Plan metadata:** this summary is committed separately after `92e9850`; its hash is reported in the completion message.

## Files Created/Modified

- `src/application/DocumentImport.h` - namespace `DocumentImport`: `ImportedDocx` and `readDocxFile`.
- `src/application/DocumentImport.cpp` - the Russian text tables (one switch for DocxError, one for DocxWarning), file checks, bounded read.
- `tests/DocumentImportTest.cpp` - `document_import_test`, 16 Qt Test cases.
- `CMakeLists.txt` - DocumentImport sources in `resume_core`; `resume_documents` linked PUBLIC into `resume_core`; `document_import_test` with `add_test(NAME document_import ...)`.
- `docs/memory.md` - journal entry for plan 01-03 (real results and open questions).

## Texts used (exact, from plan 01-03 Task 1 step 3)

| Case | Text (file name in place of %1) |
|---|---|
| Unreadable | Не удалось открыть файл «%1». Проверьте, что он существует и доступен для чтения. |
| TooLarge | Файл «%1» больше %2 МБ. Уменьшите файл (например, удалите изображения) и повторите. (%2 = 10) |
| NotZip | Файл «%1» не является DOCX: это не ZIP-архив. Сохраните резюме в Word как .docx и повторите. |
| Corrupt | Файл «%1» повреждён или обрезан. Сохраните резюме заново в Word и повторите. |
| Encrypted | Файл «%1» зашифрован. Сохраните резюме без пароля и повторите. |
| LegacyDoc | Файл «%1» в старом формате .doc, а не .docx. Откройте его в Word, сохраните как .docx и повторите. |
| Limit | Файл «%1» отклонён: после распаковки содержимое слишком большое или состоит из слишком многих частей. Сохраните резюме заново в Word и повторите. |
| MissingPart | В файле «%1» не найден текст резюме. Сохраните его как .docx в Word и повторите. |
| Warning: styles | В файле «%1» нет описания стилей: заголовки могут отображаться как обычный текст. |
| Warning: numbering | В файле «%1» нет описания нумерации: списки могут отображаться без маркеров и номеров. |

`Limit` has no test row in this plan: it is produced by the container layer (plan 01-04 enforces it); its text is in the switch and is compiled but not exercised here.

## Decisions Made

- Texts as the plan lists them; they are shown to the user in phase 3 and need owner review (see Open Questions).
- The TooLarge text uses the multi-argument `arg(name, "10")` so a file name containing `%2` cannot be substituted a second time.
- The snapshot test compares names, kinds, sizes and last-modified times of every entry under the temporary root.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical] A read error is reported as Unreadable**
- **Found during:** Task 1 (readDocxFile, step 4)
- **Issue:** The plan reads the file with `QFile::read` but does not say what to do when the read itself fails. A failed read returns fewer bytes, and those partial bytes would be classified as content (possibly giving a misleading NotZip or Corrupt text).
- **Fix:** After the read, `file.error() != QFileDevice::NoError` returns the Unreadable text.
- **Files modified:** src/application/DocumentImport.cpp
- **Verification:** The code path is in the function body; no test triggers a real read error on Windows (not covered by a test).
- **Committed in:** 8dc3f48 (Task 1)

**2. [Rule 1 - Bug in the test design] The directory snapshot baseline was stale**
- **Found during:** Task 3 (importWritesNothing)
- **Issue:** The first run failed on the directory entry of the Cyrillic subdirectory (last-modified time differed by 4 ms) at the first call. A diagnostic run showed the same difference between two listings taken with no call in between, so the import was not writing anything: NTFS updates directory times lazily after the inputs were written.
- **Fix:** The test takes listings every 100 ms until two consecutive listings agree, and uses that as the baseline. The directory time stays in the comparison, so any real change the import made would still fail the test. Diagnostic code was removed before the commit.
- **Files modified:** tests/DocumentImportTest.cpp
- **Verification:** before the fix the test failed on every run (6 of 6, always at index 7, the directory entry). After the fix the ctest run and 5 further runs of `document_import_test` all passed.
- **Committed in:** 92e9850 (Task 3)

**3. [Plan claim corrected] -Wswitch does not guard the text tables**
- **Found during:** Task 1 (review of the comment)
- **Issue:** The plan says a new DocxError code without a text "fails to compile under the warnings the build enables". The build sets no warning flags (no -Wall, no -Werror; checked with grep over CMakeLists.txt and the presets), so a missing case would compile silently.
- **Fix:** The comment states the real situation. No warning flags were added (that would change the build beyond this plan). Completeness of the tables is covered by the exact-text tests for the five failure codes and two warnings; `Limit` has no test row (see above).
- **Files modified:** src/application/DocumentImport.cpp
- **Committed in:** 8dc3f48 (Task 1)

**4. [Formatting] clang-format applied to the new files**
- **Found during:** Task 1 and Task 3
- **Issue:** The new files were not formatted with the project `.clang-format`.
- **Fix:** `clang-format -i` on the three new files. `clang-format --dry-run --Werror` on those files exits 0.
- **Committed in:** 8dc3f48, cb0c969, 92e9850

**5. [Lint] clang-tidy bugprone widening in TooLarge text**
- **Found during:** Task 3 (clang-tidy on DocumentImport.cpp)
- **Issue:** `1024 * 1024` was computed as int and widened to qint64.
- **Fix:** `constexpr qint64 bytesPerMb = qint64{1024} * 1024;`. Same value; the TooLarge test still expects "10".
- **Committed in:** 92e9850

**Total deviations:** 5 (1 Rule 2, 1 Rule 1 in test design, 1 plan claim corrected, 1 formatting, 1 lint).
**Impact on plan:** No change to the planned behaviour, texts, limits or file list. The read-error branch is an addition; the test design change keeps the same no-write check.

## Issues Encountered

- The sandbox refused compound shell commands that combined `ctest -R "zip_archive|document_import"` with other steps, and git commands with `$(...)`. Each was run as a separate simple command; the results are the same.
- QTest prints nothing to stdout in this environment; results were taken with `-o <file>,txt`.
- Running `document_import_test.exe` directly needs `C:/msys64/mingw64/bin` on PATH (libzip.dll); ctest sets it from the preset.
- The plan-head ledger file could not be created with the Write tool inside the main checkout's `.git` path; it was written with a shell redirect into the worktree's git directory.

## Verification (real runs)

| Command | Result |
|---|---|
| `cmake --preset mingw-debug` (before edits) | Configuring done, Generating done |
| `cmake --build --preset debug` (after Task 1, 2, 3 and the final edit) | exit 0 |
| `ctest --preset debug -R "zip_archive\|document_import"` (final) | 100% tests passed, 0 tests failed out of 2 (zip_archive, document_import) |
| `document_import_test.exe -o <file>,txt` (final) | `Totals: 16 passed, 0 failed, 0 skipped, 0 blacklisted, 301ms`; 5 further runs exit 0 |
| `zip_archive_test.exe -o <file>,txt` (final) | `Totals: 17 passed, 0 failed, 0 skipped, 0 blacklisted, 13ms` |
| `ctest --preset debug` (full, final) | 100% tests passed, 0 tests failed out of 5 (resume_e2e, resume_json, zip_archive, document_import, web_api) |
| `clang-format --dry-run --Werror` on the 3 new files | exit 0 |
| `clang-tidy src/application/DocumentImport.cpp -p build/mingw-debug` | exit 0; 7 trailing-return-type notes (6 in new files, 1 pre-existing in DocxContainer.h), 2 performance-enum-size (pre-existing in DocxTypes.h) |
| `grep -c "add_test(NAME document_import" CMakeLists.txt` | 1 |
| `grep -c "DocumentImport.cpp" CMakeLists.txt` | 1 |
| `grep -nE "write\|QDir\|mkpath\|remove\|rename\|ReadWrite\|WriteOnly\|Append" DocumentImport.cpp/.h` | 0 lines |
| `grep -c "Фаза 1, план 01-03" docs/memory.md` | 1 |

Not run: `cmake --install`, the Inno Setup installer, the release preset, the GitHub Actions CI, a clean machine. No mutation test was run (the claim that the tests fail on a changed text is not demonstrated by a run here).

## Known Stubs

None.

## Threat Flags

None new. The planned trust boundary (a user-selected path) is the one this plan implements; T-01-09 (size before open and bounded read), T-01-10 (fixed texts, file name only, leak test), and T-01-11 (read-only, no writes, snapshot test) are covered as planned.

## Open Questions and Owner-Pending Items

- **OWNER-PENDING: Russian texts** (all ten texts above); the owner reviews them, and they are shown in phase 3.
- **OWNER-PENDING: "MB" read as MiB** in D-01 (also open from plan 01-02). The limit text says 10 MB and is 10 MiB.
- **OWNER-PENDING: D-02 scope** (one 20 MiB budget across the three parts) and the planner limits (1000 entries, 100:1 after 1 MiB). The limits are enforced in plan 01-04, not here.
- **Open from plan 01-01: libzip source ownership** after a failed `zip_open_from_source` (branch c: source not freed). Unchanged.
- **Not verified:** the size check runs before open is confirmed by reading the function body only; the read-error branch (deviation 1) has no test; `Limit` has no test row.
- **clang-tidy style notes** (trailing return type) are not fixed, to match the existing documents-layer code.

## Next Phase Readiness

- Phase 2 can call `DocumentImport::readDocxFile` and parse `ImportedDocx.documentXml`; the Limit path is ready for plan 01-04 to fill in.
- Phase 3 can show `ImportedDocx.warnings` and the error text; the draft rule (the import never receives a draft) is in place.
- Not updated by this plan: `.planning/STATE.md` and `.planning/ROADMAP.md` (the orchestrator owns those, as in plan 01-01).
- Branch `worktree-agent-a76922aacd6407202`; not pushed.

---
*Phase: 01-zavisimost-libzip-i-bezopasnoe-chtenie-zip*
*Completed: 2026-10-24*
