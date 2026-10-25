---
phase: 02-chtenie-soderzhimogo-docx
plan: 05
subsystem: documents
tags: [docx, skipped-content, altChunk, subDoc, contentPart, warnings, import-service, qt-test, gap-closure]

# Dependency graph
requires:
  - phase: 02-04
    provides: readDocxFile returns the model and warnings; header, footer and comments counted by name
  - phase: 02-03
    provides: skipUnknown, skipSubtree, OtherSkipped rule, mc:AlternateContent handling
provides:
  - w:altChunk, w:subDoc and w14:contentPart give one OtherSkipped per instance (body, paragraph, run, table cell, hidden alternate-content subtree)
  - Service-level tests through readDocxFile for lists (with OQ5 recorded on purpose), tables (gridSpan, vMerge restart and continue, empty cell) and hyperlinks, insertions and deletions
  - Exact-text check for all seven DocxWarning codes against the output of readDocxFile; pairwise distinctness over real output
affects: [phase 2 verification re-run, phase 3 (owner decisions on SAFE-04 and the warning texts, TEST-02)]

# Actuals (#2632): same estimateTokens scale (chars/4 over the realized diff)
actuals:
  tokens: 10274    # 41097 bytes of `git diff 3f9c2b7 HEAD -- src tests docs` / 4; byte-based, Cyrillic is overstated; plan estimate was 45000
  tasks: 3
  commits: 4       # MEASURED: git rev-list --count 3f9c2b7ca2159caea5fa696738c4e776a0efa402..HEAD before this SUMMARY (3 task commits + 1 journal commit)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Parts that hold content in another part (altChunk, subDoc, contentPart) are counted by element name; the part is never read and r:id is never resolved (D-10, T-02-15)"
    - "A skip flag set during the skipped subtree (SkipFacts.hasOtherPart) reaches elements hidden inside mc:AlternateContent"
    - "Warning texts are checked through the service (readDocxFile) on one archive per code, not against copies of the texts in the test"

key-files:
  created:
    - .planning/phases/02-chtenie-soderzhimogo-docx/02-05-SUMMARY.md
  modified:
    - src/documents/DocxContent.cpp
    - src/documents/README.md
    - tests/DocxContentTest.cpp
    - tests/DocumentImportTest.cpp
    - docs/memory.md

key-decisions:
  - "w:altChunk, w:subDoc and w14:contentPart: one OtherSkipped per instance, the same rule as footnote references (D-08). The content is not read and r:id is not resolved (D-10)."
  - "w14:contentPart inside mc:AlternateContent inside w:r is found through a flag set in skipSubtree; the rule on the element alone does not cover that path (row F of the test)."
  - "The list test records the OQ5 false positive on purpose: one ListFormatApproximated, for numId 2 (lvlText %2. on level 1). Remove the expectation together with the code change if OQ5 is fixed."
  - "The literal-copy test warningTextsArePairwiseDistinct is removed; the texts are checked by exact match against readDocxFile output, one archive per code."
  - "The vMerge continuation text is not shown and not counted (Отложено п. 1); no code or test was added for it. Owner decision."

patterns-established:
  - "Mutation check on one warning text: change one character in DocumentImport.cpp, rebuild, run the test, restore from a backup outside the repo, compare sha256, rebuild"

requirements-completed: [DOCX-04, DOCX-05, DOCX-08, SAFE-04, TEST-01]
# Coverage, stated per requirement (the plan's frontmatter lists these IDs):
#   SAFE-04: PARTIAL, not satisfied. Closed for w:altChunk, w:subDoc and w14:contentPart. Open: (1) vMerge continuation text is silently dropped (Отложено п. 1, owner decision); (2) the list of other elements that reference another part by r:id is not checked against ISO/IEC 29500 (question to owner). REQUIREMENTS.md is not changed; the SAFE-04 checkbox stays open.
#   DOCX-04: lists tested through readDocxFile; OQ5 false positive recorded on purpose; lvlOverride ignored (Отложено п. 2).
#   DOCX-05: tables tested through readDocxFile; vMerge continuation text open (Отложено п. 1).
#   DOCX-08: hyperlinks, insertions and deletions tested through readDocxFile.
#   TEST-01: synthetic fixtures built in code; the Word-saved file is still open (TEST-02, phase 3).

# Metrics
duration: not measured (plan start time was not recorded in this session)
completed: 2026-10-25
status: complete
---

# Phase 2 Plan 05: Закрытие разрывов верификации (altChunk, subDoc, contentPart; сервисные тесты; тексты предупреждений) Summary

**w:altChunk, w:subDoc and w14:contentPart now give one OtherSkipped each (no silent drop); lists, tables and hyperlinks are checked through readDocxFile; all seven warning texts are checked against real output, while SAFE-04 stays PARTIAL until the owner decides the vMerge continuation and the r:id element list.**

## Performance

- **Duration:** not measured (plan start time was not recorded in this session)
- **Started / Completed:** not recorded; completed 2026-10-25
- **Tasks:** 3 of 3 completed
- **Files modified:** 5 (2 code, 2 test, 1 README, 1 journal; this SUMMARY is added)

## Accomplishments

- `w:altChunk`, `w:subDoc` and `w14:contentPart` give exactly one `OtherSkipped` each, in the body, in a paragraph, between runs, in a table cell, and inside a hidden `mc:AlternateContent` (SAFE-04, ROADMAP SC4, BLOCKER 1 of 02-VERIFICATION). The neighbouring text is read (checked in all seven rows).
- Red run before the fix: rows A to G of `altChunkAndSubDocCountedAsOtherSkipped` failed on the warning check; the text check passed in every row.
- Service-level tests through `DocumentImport::readDocxFile` for lists (SC1, DOCX-04), tables with gridSpan, vertical merge and an empty cell (SC2, DOCX-05), and hyperlinks, insertions and deletions (SC2, DOCX-08). Each test uses a synthetic archive built in code.
- The literal-copy test `warningTextsArePairwiseDistinct` is replaced by `warningTextsMatchEachCode` (seven rows, exact match against the output of `readDocxFile`) and `warningTextsOfRealRunsArePairwiseDistinct` (seven distinct texts from real runs). This closes WARNING 3 and WARNING 4 (ListFormatApproximated text was never checked; ImageSkipped and TextBoxSkipped texts were not checked either).
- Mutation check: one character in the ImageSkipped text changed; only `warningTextsMatchEachCode(ImageSkipped)` failed; file restored, sha256 matched.
- `src/documents/README.md`: the rule for the three elements and the OtherSkipped row of the DocxWarning table.

## Task Commits

Each task was committed atomically:

1. **Task 1: w:altChunk, w:subDoc and w14:contentPart counted as OtherSkipped** - `679dce8` (fix; includes the test, the source change and the README rule)
2. **Task 2: service-level tests for lists, tables and hyperlinks** - `d67296d` (test)
3. **Task 3: exact warning texts through readDocxFile; literal-copy test removed** - `83670b4` (test)
4. **Journal entry** - `6e3a887` (docs; `docs/memory.md`)

**Plan metadata:** this SUMMARY commit follows (docs: complete plan 02-05). STATE.md and ROADMAP.md were not changed (the orchestrator owns them).

_Note: none of the tasks was TDD-tagged in the plan. Task 1 was run red before the source change, as the plan requires; Tasks 2 and 3 were run green on first build (the new test rows were written against the plan's expectations)._

## Files Created/Modified

- `src/documents/DocxContent.cpp` - constants `kAltChunk`, `kSubDoc`, `kW14Namespace`, `kContentPart`; `isW14`; `SkipFacts::hasOtherPart` set in `skipSubtree`; `skipUnknown` counts the three elements and uses `hasOtherPart`; comment updated.
- `tests/DocxContentTest.cpp` - `altChunkAndSubDocCountedAsOtherSkipped` (data-driven, seven rows A to G); clang-format applied to the new lines only.
- `tests/DocumentImportTest.cpp` - `documentRoot` (prefixes w, r, v); `warningCases` and `WarningCase`; `listsKeepNestingThroughReadDocxFile`, `tablesKeepRowsThroughReadDocxFile`, `hyperlinkAndInsertionGiveTextThroughReadDocxFile`, `warningTextsMatchEachCode` (data), `warningTextsOfRealRunsArePairwiseDistinct`; `warningTextsArePairwiseDistinct` removed; `#include <QSet>`; clang-format applied to the new lines only.
- `src/documents/README.md` - rule on the three elements; OtherSkipped row updated.
- `docs/memory.md` - one entry for plan 02-05.
- `src/application/DocumentImport.cpp` - not changed in any commit (the mutation was applied and restored; `git diff` is empty).

## Decisions Made

- The three elements follow the same counting rule as footnote references: one OtherSkipped per instance (D-08). Their content and `r:id` are not read (D-10, T-02-15).
- The hidden-subtree case (row F, `w14:contentPart` inside a run's `mc:AlternateContent`) is handled by the flag, because the element is visible only as a descendant there. Row G (directly in a paragraph) is handled by the element rule.
- The OQ5 false positive is kept in the test on purpose (one `ListFormatApproximated`, numId 2). Remove that expectation together with the code change if OQ5 is fixed.
- No text in `DocumentImport.cpp` was changed; the seven texts are checked, not decided (OQ8).

## Deviations from Plan

### Auto-fixed Issues

**1. [Formatting] clang-format on new lines of two test files**
- **Found during:** Task 1 (`DocxContentTest.cpp`, one line over the column limit) and Task 2 and 3 (`DocumentImportTest.cpp`, line breaks and string wrapping)
- **Issue:** `clang-format --dry-run --Werror` reported the new lines.
- **Fix:** `clang-format -i` on the two test files. The diff was checked with `git diff -U0`: only hunks at the new code changed; no existing line moved.
- **Files modified:** `tests/DocxContentTest.cpp`, `tests/DocumentImportTest.cpp`
- **Committed in:** `679dce8`, `d67296d`, `83670b4`

**Total deviations:** 1 (formatting only). **Impact:** no change in behaviour or in any expectation.

No Rule 1, 2, 3 or 4 deviations occurred. No expectation was changed to match output (the worktree rule and the plan's failure rule were not triggered: every expectation passed as written).

## Issues Encountered

- The first clang-tidy baseline run failed with `'DocxContentTest.moc' file not found` (and the same for `DocumentImportTest.moc`). AUTOMOC files are generated by the build, so the baseline was re-run after one build of the unmodified tree (no source edit). The recorded baseline below is the second run, and the baseline was still taken before the first edit.
- The Bash harness refused some compound commands that combined a runtime variable with a binary or a git call. Those were split into plain separate commands with literal paths. No command ran outside the worktree.
- The red-run output of `QVERIFY2` shows `returned FALSE` and does not print the observed warning count. The count 0 that the plan expects was not printed by the run; it is not claimed here as observed.

## Test runs (real commands and results)

| Command | Exit | Result |
|---|---|---|
| `cmake --preset mingw-debug` (fresh worktree, before any edit) | 0 | configure and generate OK; `web_e2e` skipped (no `tests/e2e/node_modules`) |
| `cmake --build --preset debug` (unmodified tree, before any edit) | 0 | 52 steps, 0 `error:` lines |
| `cmake --build --preset debug` (test added, before the fix) | 0 | 0 `error:` lines |
| `docx_content_test.exe -o <file>,txt` (red, before the fix) | 7 | Totals 97 passed, 7 failed; the failing rows are `altChunkAndSubDocCountedAsOtherSkipped(A…G)` |
| `cmake --build --preset debug` and `ctest --preset debug -R docx_content --no-tests=error` (after the fix) | 0 | 100% passed, 1 of 1 |
| `docx_content_test.exe -o <file>,txt` (after the fix) | 0 | Totals 104 passed, 0 failed; `textBoxAndOtherSkippedCounted` and `alternateContentUsesChoiceOnly` PASS |
| `ctest --preset debug -R document_import --no-tests=error` (after Task 2) | 0 | 100% passed; `document_import_test` Totals 27 passed, 0 failed (24 before the task, per 02-04 SUMMARY; not re-measured here before the edit) |
| `ctest --preset debug -R document_import --no-tests=error` (after Task 3) | 0 | 100% passed; `document_import_test` Totals 34 passed, 0 failed |
| `ctest --preset debug` (full, final code) | 0 | 100% passed, 6 of 6 (resume_e2e, resume_json, zip_archive, docx_content, document_import, web_api) |
| `docx_content_test.exe -o <file>,txt` (final) | 0 | Totals 104 passed, 0 failed |
| `document_import_test.exe -o <file>,txt` (final) | 0 | Totals 34 passed, 0 failed |
| `clang-format --dry-run --Werror` on the three target files (final) | 0 | no violations |

Per-case results of the new `document_import_test` rows: `listsKeepNestingThroughReadDocxFile`, `tablesKeepRowsThroughReadDocxFile`, `hyperlinkAndInsertionGiveTextThroughReadDocxFile` PASS (first run, no expectation changed). `warningTextsMatchEachCode(StylesPartMissing|NumberingPartMissing|ListFormatApproximated|ImageSkipped|TextBoxSkipped|OtherSkipped|HeaderFooterSkipped)` PASS (seven rows); `warningTextsOfRealRunsArePairwiseDistinct` PASS.

### Mutation check (Task 3, step 6)

| Step | Result |
|---|---|
| sha256 of `src/application/DocumentImport.cpp` before the mutation | `25f65ecd4ce4ef63de7ea6a0135d3b856bf6182d683a0a97c4811a21dde0b22b` (backup outside the repo) |
| Mutation: line 91, `показан, текст` changed to `показан; текст` (ImageSkipped text) | applied |
| `cmake --build --preset debug` after the mutation | 0 |
| `ctest --preset debug -R document_import --no-tests=error` after the mutation | exit 8: Totals 33 passed, 1 failed; `FAIL! : DocumentImportTest::warningTextsMatchEachCode(ImageSkipped) Compared lists differ at index 0.` |
| Restore from backup; sha256 after the restore | `25f65ecd4ce4ef63de7ea6a0135d3b856bf6182d683a0a97c4811a21dde0b22b` (same) |
| `cmake --build --preset debug` after the restore, then `ctest` | 0; 100% passed |
| `git diff -- src/application/DocumentImport.cpp` at the end | empty |

### clang-tidy (AGENTS.md section 2)

Command for each file: `clang-tidy <file> -p build/mingw-debug` with `C:/msys64/mingw64/bin` on PATH.

| File | Baseline before any edit (exit, `warning:` lines) | After all edits (exit, `warning:` lines) | New diagnostics in lines written by this plan |
|---|---|---|---|
| `src/documents/DocxContent.cpp` | 0, 74 | 0, 75 | +1 `modernize-use-trailing-return-type` on `isW14` (line 146). Its neighbours `isWml` and `isMc` already carry the same warning in the baseline; the file keeps the same signature style. Style only. |
| `tests/DocxContentTest.cpp` | 0, 51 | 0, 51 | none |
| `tests/DocumentImportTest.cpp` | 0, 63 | 0, 105 | +38 `modernize-use-designated-initializers` and +4 `modernize-use-trailing-return-type`, all in the new fixtures and helpers (`documentEntry`, `warningCases`, `documentRoot`, the list-paragraph lambda, `ZipEntry{...}` and table-row aggregates). The same idiom produces 44 designated-initializer and 13 trailing-return warnings in the file's existing fixtures. Style only; not fixed, to keep one fixture idiom in the file. |

Clang-tidy reports no errors (`error:` count 0) for all three files, before and after. The baseline check for DocxContent.cpp matched the planning figure of 74. These are style checks only; the statement "clang-tidy is clean" is not made.

## Open questions for the owner (not decided here)

- **SAFE-04 (PARTIAL):** (1) text of a vMerge continuation cell is not shown and gives no warning (Отложено п. 1; accept as an exception to SAFE-04, or count as OtherSkipped); (2) the list of other elements that reference another part by `r:id` is not checked against ISO/IEC 29500 (not done by this plan; recorded here as the open question from the plan's objective). REQUIREMENTS.md is not changed; the SAFE-04 checkbox stays open.
- **Warning texts (OQ8):** the seven texts are now checked by exact match, but not changed. The owner checks them in phase 3.
- **Deferred items from the plan:** `w:lvlOverride` and `w:startOverride` ignored (п. 2); undefined list level shown as decimal without warning (п. 3); OQ5 false positive (п. 4; recorded in the list test); soft and no-break hyphens (п. 5); literal copies in `failureTextsArePairwiseDistinct` (п. 7, not in this plan's scope).
- **TEST-02:** a DOCX saved by Word is still not tested; open until phase 3.

## Not run

- Release build (`cmake --build --preset release`) and `ctest --preset release`.
- GitHub Actions CI and the Inno Setup installer.
- `web_e2e` (not registered: no `tests/e2e/node_modules`).
- A manual check on a Word-saved DOCX (TEST-02, phase 3).
- A separate mutation that removes the `hasOtherPart` flag. The claim that row F alone would give zero warnings without the flag is not verified here and is not made.

## Threat Flags

None beyond the plan's threat model. T-02-15 (resolution of `r:id` in altChunk, subDoc and contentPart) is mitigated: the element is counted by name, the subtree is walked without reading other parts, and `word/_rels` is not opened; the fixtures use an `r:id` (`rId20`, `rId21`, `rId5`) that has no relationship part.

## Known Stubs

None introduced by this plan.

## Next Phase Readiness

- The 02-VERIFICATION BLOCKER 1 (altChunk, subDoc) and WARNINGs 3 and 4 are addressed by this plan's commits; the verifier should re-run the phase verification to confirm.
- Phase 3 should not rely on SAFE-04 as satisfied until the owner decides the vMerge continuation (Отложено п. 1) and the r:id element list (question above).
- Owner review of the seven warning texts is due in phase 3 together with the phase-1 texts.

## Self-Check

Checked after writing this file and before its commit:

- Files changed by the plan exist and are in the commits listed above.
- Commits `679dce8`, `d67296d`, `83670b4`, `6e3a887` exist as ancestors of HEAD (checked with `git merge-base --is-ancestor`).
- Ledger: `plan_head_before` 3f9c2b7ca2159caea5fa696738c4e776a0efa402; 4 commits measured with `git rev-list --count` before this SUMMARY; `plan_head_after` 6e3a887472a122d956577f3a4cf04c4e270349ce (the SUMMARY commit follows).
- `git status` before the SUMMARY commit: clean; `.planning/REQUIREMENTS.md`, `.planning/STATE.md` and `.planning/ROADMAP.md` not modified.
