---
phase: 02-chtenie-soderzhimogo-docx
plan: 01
subsystem: documents
tags: [docx, qxmlstreamreader, xxe, dtd, encoding, text-limit, qt-test]

# Dependency graph
requires:
  - phase: 01-zavisimost-libzip-i-bezopasnoe-chtenie-zip
    provides: readDocxParts (DocxParts: documentXml, stylesXml, numberingXml, warnings), DocxError/DocxWarning codes, kMaxTextChars, DocumentImport::readDocxFile
provides:
  - documents::DocxDocument model contract (Paragraph, Table, TableRow, TableCell, Block) in src/documents/DocxModel.h
  - documents::readDocxContent: body paragraphs in document order with exact text
  - DOCTYPE refusal (UnsafeXml) in all three parts; encoding policy (UnsupportedEncoding)
  - kMaxModelNodes = 50000 and enforcement of kMaxTextChars during accumulation
  - docx_content_test target (ctest name docx_content)
affects: [02-02 styles and numbering, 02-03 tables and skipped content, 02-04 import API and warnings, phase 3 UI]

# Actuals (#2632) — same estimateTokens scale (chars/4 over the realized diff)
actuals:
  tokens: 9636     # 38546 bytes of `git diff 25deb1d HEAD` / 4; byte-based, so Cyrillic is slightly overstated
  tasks: 3
  commits: 4       # MEASURED: git rev-list --count 25deb1d..HEAD before this SUMMARY (3 task commits + 1 docs commit)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Single token reader (PartReader::next) that maps parser errors and DTD tokens to codes"
    - "Element matching by namespaceUri() and local name(), never by prefix"
    - "Running budgets per readDocxContent call (text characters, model nodes), checked before the node or text is kept"

key-files:
  created:
    - src/documents/DocxModel.h
    - src/documents/DocxContent.h
    - src/documents/DocxContent.cpp
    - tests/DocxContentTest.cpp
  modified:
    - src/documents/DocxTypes.h
    - src/application/DocumentImport.cpp
    - tests/DocumentImportTest.cpp
    - CMakeLists.txt
    - docs/memory.md

key-decisions:
  - "OQ3 (default, owner confirmation pending): only UTF-8 and UTF-16 (any letter case) and a missing declaration are accepted; any other declared encoding returns UnsupportedEncoding, not Corrupt (A10 unconfirmed)"
  - "OQ9 (planner value, owner confirmation pending): kMaxModelNodes = 50000; a node is counted when its paragraph starts, so 50001 empty <w:p/> are refused"
  - "OQ8 (default): the Russian texts for UnsafeXml and UnsupportedEncoding are written now and built only from the file name; the owner checks them in phase 3"
  - "Protective pass over styles.xml and numbering.xml (DOCTYPE and encoding checks, read to EndDocument) runs before the body; their content is not used yet (02-02)"

patterns-established:
  - "Codes only at the documents layer; no QXmlStreamReader::errorString() and no Qt log output in src/documents/DocxContent.cpp"
  - "Qt Test data rows for refusal cases (doctypeRejected, declaredEncodingPolicy); message capture via a scoped qInstallMessageHandler guard that restores the previous handler on early return"

requirements-completed: [DOCX-02, DOCX-06, SAFE-03, SAFE-05, TEST-01]  # all five copied from this plan's `requirements`; coverage is partial for DOCX-02 (paragraphs only), SAFE-05 (XML errors only) and TEST-01 (first tests of the 02-02..02-04 set)

duration: not measured (plan start time was not recorded in this session)
completed: 2026-10-24
commits: 4
plan_head_before: 25deb1d93008276d8075320e88894b7aaaeae3c1
plan_head_after: b1df7c1af175e1f0353e412f4d2fd9aa6ef347a7
status: complete
---

# Phase 2 Plan 01: Ядро разбора содержимого DOCX Summary

**Body paragraphs parse in document order with exact Cyrillic text, while DOCTYPE, undeclared or truncated XML, and unsupported encodings are refused with codes only, and both text and node limits are enforced during accumulation.**

## Performance

- **Duration:** not measured
- **Started / Completed:** not recorded
- **Tasks:** 3 of 3 completed
- **Files modified:** 4 created, 5 modified

## Accomplishments

- `readDocxContent(const DocxParts&)` returns a `DocxDocument` whose `blocks` hold each `w:p` of `w:body` as a `Paragraph`, in document order, with text taken only from `w:t` (whitespace and CDATA kept, inter-element whitespace dropped).
- A DOCTYPE in `document.xml`, `styles.xml` or `numbering.xml` returns `UnsafeXml` at its DTD token, before any entity is expanded, with no partial result (SAFE-03, T-02-01).
- A declared encoding other than UTF-8 or UTF-16 returns `UnsupportedEncoding`; UTF-16 with BOM parses (OQ3, T-02-04).
- `kMaxTextChars` (200000) is enforced while text accumulates: 200000 is accepted, 200001 gives `Limit`. `kMaxModelNodes` (50000) is enforced per paragraph node: 50000 accepted, 50001 gives `Limit` (T-02-02).
- The two new codes have Russian texts in `DocumentImport::errorText` built only from the file name; `failureTextsArePairwiseDistinct` counts seven texts.

## Task Commits

Each task was committed atomically:

1. **Task 1: Tracer, model contract and body paragraphs** - `586c7b4` (feat)
2. **Task 2: DTD refusal, encoding policy, error texts, marker test** - `06f331b` (feat)
3. **Task 3: Exact text, text and node limits** - `25c9215` (feat)

**Plan metadata:** `b1df7c1` (docs: record phase 2 plan 02-01 results in memory). The SUMMARY commit follows this file.

_Note: this plan was not TDD-tagged; each task was implemented first and its tests were run afterward on the same commit._

## Files Created/Modified

- `src/documents/DocxModel.h` (new) - model contract: `ListKind`, `Paragraph`, `TableCell`, `TableRow`, `Table`, `Block` (variant), `DocxDocument`.
- `src/documents/DocxContent.h` (new) - declaration of `readDocxContent`.
- `src/documents/DocxContent.cpp` (new) - `PartReader` (single `next()` read point; DTD and encoding checks), `readParagraph`, `skipElement`, `checkPartIsSafe`, body walk, `ParseBudget` counters.
- `src/documents/DocxTypes.h` (modified) - `UnsafeXml`, `UnsupportedEncoding` appended to `DocxError`; `kMaxModelNodes`; comment on `kMaxTextChars` updated.
- `src/application/DocumentImport.cpp` (modified) - two `errorText` branches, no `default`.
- `tests/DocxContentTest.cpp` (new) - 14 test functions, 25 QTest passes including data rows.
- `tests/DocumentImportTest.cpp` (modified) - two literals added, count changed from 5 to 7.
- `CMakeLists.txt` (modified) - model, content header and source in `resume_documents`; `docx_content_test` target and `docx_content` test.
- `docs/memory.md` (modified) - journal entry for this plan (CLAUDE.md and AGENTS.md require it after a meaningful task; this file was not in the plan's `files_modified`, see Deviations).

## Decisions Made

- OQ3 default kept: only UTF-8 and UTF-16 (case-insensitive) and a missing declaration are accepted; other declarations are `UnsupportedEncoding`. Owner confirmation of A10 is pending.
- OQ9 default kept: `kMaxModelNodes` = 50000, planner value, owner confirmation pending.
- OQ8 default kept: texts written now, owner review in phase 3.
- Node counting: a paragraph node is counted at its start tag, before its content is read, so a flood of empty paragraphs stops at the limit.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical functionality] Added the memory journal entry**
- **Found during:** Task 3 wrap-up
- **Issue:** CLAUDE.md and AGENTS.md require `docs/memory.md` to be updated after a meaningful task, but `docs/memory.md` was not in this plan's `files_modified`.
- **Fix:** Added one journal entry in its own commit (`b1df7c1`), without touching `STATE.md` or `ROADMAP.md`.
- **Files modified:** `docs/memory.md`
- **Verification:** `git show --stat b1df7c1` lists only `docs/memory.md`.
- **Committed in:** `b1df7c1`

**2. [Formatting] clang-format applied to two test files**
- **Found during:** Tasks 2 and 3
- **Issue:** `clang-format --dry-run --Werror` flagged lines this plan wrote in `tests/DocxContentTest.cpp` and `tests/DocumentImportTest.cpp`.
- **Fix:** `clang-format -i` on both files; the `DocumentImportTest.cpp` diff was checked to touch only the added lines.
- **Committed in:** `06f331b` and `25c9215`

**Total deviations:** 2 (1 Rule 2, 1 formatting). **Impact:** none on scope; no production behavior changed.

## Issues Encountered

- The session was interrupted by a usage limit after Task 3's test functions were partly written. On resume, `git log` and `git status` were checked, the Task 3 edits were kept, and no completed commit was repeated.
- Build and test commands in a fresh shell need `export PATH="/c/msys64/mingw64/bin:$PATH"` again (shell state does not persist between calls); without it the test executables fail to load Qt6Test.dll. This was a shell setup issue, not a code issue.
- The worktree sandbox refused some compound git and ctest commands; the same steps were run as separate commands.

## Known Stubs and Known Gaps

- **Intermediate state (closed in 02-03):** body children other than `w:p` (tables, drawings, fields, text boxes) are skipped silently in this plan. No warning is emitted yet, so SAFE-04 is not met by this plan. This is recorded as a gap, not as a finished feature.
- Styles and numbering are checked for safety but not read (02-02).
- No `Known Stubs` in the UI sense: nothing in this plan renders or returns placeholder text.

## Threat Flags

None beyond the plan's `<threat_model>` (T-02-01 to T-02-05). The new files add no network, auth or file-access surface; `src/documents/` takes bytes only.

## Verification: commands and real results

| Command | Result |
|---|---|
| Baseline before any change (`25deb1d`): `cmake --preset mingw-debug`, `cmake --build --preset debug`, `ctest --preset debug` | configure OK; build exit 0; ctest 100% passed, 5/5 |
| After Task 1: `cmake --build --preset debug` | exit 0 |
| After Task 1: `ctest --preset debug -R docx_content --no-tests=error` | 1/1 passed |
| After Task 1: `docx_content_test -v2` | Totals 3 passed, 0 failed |
| After Task 2: `docx_content_test -v1` | Totals 16 passed, 0 failed |
| After Task 2: `ctest --preset debug -R document_import --no-tests=error` | 1/1 passed |
| After Task 2: full `ctest --preset debug` | 100% passed, 6/6 |
| After Task 3: `cmake --build --preset debug` | exit 0, no `error:` lines |
| After Task 3: `docx_content_test -v1` | Totals 25 passed, 0 failed |
| After Task 3 (final code): `ctest --preset debug` | 100% tests passed, 0 failed out of 6 (resume_e2e, resume_json, zip_archive, docx_content, document_import, web_api; total 1.52 sec) |
| Final `document_import_test -v1` | Totals 17 passed, 0 failed |
| `clang-format --dry-run --Werror` on `DocxModel.h`, `DocxContent.h`, `DocxContent.cpp`, `DocxContentTest.cpp`, `DocxTypes.h`, `DocumentImport.cpp`, `DocumentImportTest.cpp` | exit 0 |
| `clang-tidy src/documents/DocxContent.cpp -p build/mingw-debug` (compile database present) | 10 x `modernize-use-trailing-return-type`, 3 x `performance-enum-size`; style only, same classes as in phase 1, not changed |
| Gate: `grep -v '^\s*//' DocxContent.cpp \| grep -c readNext` | 1 |
| Gate: `errorString` / `qDebug\|qWarning\|qCritical\|qInfo` / `isWhitespace` / `trimmed` in `DocxContent.cpp` | 0 / 0 / 0 / 0 |
| Gate: `default:` in `DocumentImport.cpp` | 0 (unchanged from baseline) |
| Gate: `qsizetype{7}` in `DocumentImportTest.cpp` | 1 |
| Gate: `add_test(NAME docx_content COMMAND docx_content_test)` in `CMakeLists.txt` | 1 |

What the tests cover (names as in `tests/DocxContentTest.cpp`): `paragraphsKeepDocumentOrder`, `doctypeRejected` (4 rows), `doctypeResultHasNoContent`, `undeclaredEntityIsCorrupt`, `truncatedXmlIsCorrupt`, `declaredEncodingPolicy` (5 rows), `messagesDoNotCarryMarker`, `whitespaceAndCdataKept` (5 rows), `interElementWhitespaceIgnored`, `cyrillicTextExact`, `textLimitBoundary`, `nodeLimitBoundary`.

**Not run (stated explicitly):**
- Mutation check (temporarily break the DTD check or a limit and confirm the tests fail): not performed.
- Release build (`cmake --build --preset release`) and `ctest --preset release`: not run in this plan.
- `web_e2e`: not registered (no `tests/e2e/node_modules`), as in earlier phases.
- CI (GitHub Actions) and the Inno Setup installer: not run.
- Qt minimum 6.7 (CMake `find_package(Qt6 6.7)`): the DTD and encoding behavior was checked only on Qt 6.11.1 (A8).
- clang-tidy on files other than `DocxContent.cpp`: not run for this plan.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 02-02 can build on `DocxModel.h` and the `PartReader` in `DocxContent.cpp`; it will replace `checkPartIsSafe` for styles and numbering with real reads.
- Plan 02-03 extends the same `ParseBudget` to table rows and cells and replaces the silent skip of other body children with counted warnings.
- Owner confirmation pending: OQ3 (encoding policy and A10), OQ8 (Russian texts for the two new codes), OQ9 (node limit value).

## Self-Check: PASSED

- Files exist: `src/documents/DocxModel.h`, `src/documents/DocxContent.h`, `src/documents/DocxContent.cpp`, `tests/DocxContentTest.cpp` (checked with `ls -la`).
- Commits exist as ancestors of HEAD: `586c7b4`, `06f331b`, `25c9215`, `b1df7c1` (checked with `git merge-base --is-ancestor`).
- Ledger: `plan_head_before` 25deb1d; 4 commits measured before this SUMMARY was written.
