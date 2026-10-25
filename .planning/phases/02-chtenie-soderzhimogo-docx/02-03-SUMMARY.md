---
phase: 02-chtenie-soderzhimogo-docx
plan: 03
subsystem: documents
tags: [docx, tables, gridSpan, vMerge, inline-content, skipped-content, mc-alternate-content, nesting-limit, qt-test]

# Dependency graph
requires:
  - phase: 02-01
    provides: DocxModel.h (Paragraph, Table, TableRow, TableCell, Block), PartReader, ParseBudget, kMaxModelNodes, kMaxTextChars
  - phase: 02-02
    provides: headingLevel and list fields, readStyles, readNumbering, ListFormatApproximated
provides:
  - w:tbl, w:tr, w:tc read into Table and TableCell with gridSpan, vMerge, padding and trimmed spans
  - inline content of paragraphs: runs, hyperlinks, insertions, fields, smart tags, content controls
  - skipped-content warnings ImageSkipped, TextBoxSkipped, OtherSkipped (one per instance, in document order)
  - kMaxNestingDepth = 64 with DepthScope over counted elements (T-02-08)
affects: [02-04 import API and header/footer and comments warnings, phase 3 UI]

# Actuals (#2632): same estimateTokens scale (chars/4 over the realized diff)
actuals:
  tokens: 16045    # 64183 bytes of `git diff 4a45917..HEAD -- src tests docs` / 4; byte-based, so Cyrillic is slightly overstated
  tasks: 3
  commits: 4       # MEASURED: git rev-list --count 4a45917..HEAD before this SUMMARY (3 task commits + 1 docs commit)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Sink (paragraph runs or block list) passed through counted wrappers, so one reader serves paragraphs, cells and content controls"
    - "DepthScope: RAII counter over counted elements; exceeded() checked on entry, depth restored on every exit"
    - "Two-phase table build: cells stored with 64-bit columns and merge indices; the grid is applied once the table is complete"
    - "skipUnknown: one rule per skipped element kind, with a subtree walk that reports w:t and txbxContent facts"

key-files:
  created: []
  modified:
    - src/documents/DocxContent.cpp
    - src/documents/DocxTypes.h
    - src/application/DocumentImport.cpp
    - tests/DocxContentTest.cpp
    - docs/memory.md

key-decisions:
  - "OQ1 (default, owner confirmation pending): w:sdt, w:smartTag, w:fldSimple and the result of a complex field give text; w:txbxContent gives TextBoxSkipped. w:sdt with w:showingPlcHdr is skipped without a warning (A11)"
  - "OQ7 (default, owner confirmation pending): kMaxNestingDepth = 64 over w:tbl, w:sdt, w:customXml, w:hyperlink, w:ins, w:moveTo, w:smartTag, w:fldSimple and the chosen mc:AlternateContent branch; 64 accepted, 65 gives DocxError::Limit (A9 not checked beyond the synthetic tests)"
  - "OQ10 and footnotes: each footnoteReference and endnoteReference in the body gives one OtherSkipped; the text of notes is not read. word/comments.xml is not read here (02-04 adds its single OtherSkipped); owner confirmation pending"
  - "OQ11: noBreakHyphen gives U+2011, softHyphen gives U+00AD, ptab gives a tab (A14), sym gives no text and one OtherSkipped (A15)"
  - "A2: only the first mc:Choice is read; mc:Fallback is read only when no branch came before it. Duplication of text in Word files is not verified (synthetic fixture only)"
  - "A6: gridSpan is the number of grid columns the cell covers; a value below 1 or non-numeric gives 1; a value above kMaxModelNodes is clamped (synthetic fixture only)"
  - "A7: w:del and w:moveFrom are skipped with their subtree, without a warning; delText and instrText give no text (synthetic fixture only)"
  - "A9: the depth limit 64 is a planner value; its safety was not measured on a real file (synthetic fixture only)"
  - "A11: a w:sdt whose w:sdtPr has w:showingPlcHdr has its content skipped without a warning; the flag must come before w:sdtContent, as the schema orders it (synthetic fixture only)"
  - "A12: w:tab gives a tab, w:br and w:cr give a line break (synthetic fixture only)"

requirements-completed: [DOCX-05, DOCX-07, DOCX-08, SAFE-04]
# Partial: DOCX-05 covers gridSpan, vMerge and padding on synthetic fixtures only; DOCX-07 and DOCX-08 likewise (A6, A7, A12 not checked on a Word-saved file, phase 3 TEST-02); SAFE-04 covers pictures, text boxes, footnotes, symbols and unknown elements; header and footer parts and comments.xml are counted in plan 02-04.

# Duration and timeline
duration: not measured (plan start time was not recorded in this session)
completed: 2026-10-25
commits: 4
plan_head_before: 4a4591743df64d7a2e647067dd53efbe1b206233
plan_head_after: 8534db6253f22d57e3592728712845a78947f276
status: complete
---

# Phase 2 Plan 03: Таблицы, встроенное содержимое и пропуски DOCX Summary

**Tables come out as grid-aligned rows (gridSpan trimmed to the grid, vMerge continuations not repeated, short rows padded), hyperlinks, insertions, fields and content controls give their text, deleted revisions stay out, and every skipped picture, text box, note, symbol or unknown element with text gives one warning.**

## Performance

- **Duration:** not measured
- **Started / Completed:** not recorded; completed 2026-10-25
- **Tasks:** 3 of 3 completed
- **Files modified:** 4 code and test files, 1 docs file

## Accomplishments

- A `w:tbl` in the body or in a cell is read into `Table`: `w:tblGrid` gives `gridColumns` (or the widest row when there is no grid), each `w:tc` becomes a `TableCell` with `startColumn`, `columnSpan` and `rowSpan`. Paragraphs and nested tables inside cells use the same text and node budgets as the body.
- `gridSpan` is clamped to `kMaxModelNodes` and trimmed to the grid; a span beyond the grid gives a shorter cell, not an error. A cell that starts beyond the grid is dropped, and counted as `OtherSkipped` when it has content.
- `vMerge` restart opens a group; continue (or `vMerge` with no value) adds one to the `rowSpan` of the opener and produces no cell; its content is not shown. Any other cell closes the group of its column.
- Each row is padded with empty cells up to the grid; every kept and padding cell counts toward `kMaxModelNodes`, so a wide grid is refused by the node limit (test `paddingCellsCountedInNodeLimit`: 49998 accepted, 49999 refused).
- Paragraph content is read as runs: text, `w:tab`, `w:ptab`, `w:br`, `w:cr`, `w:noBreakHyphen`, `w:softHyphen`. `w:hyperlink`, `w:ins`, `w:moveTo`, `w:smartTag`, `w:fldSimple`, `w:customXml` and `w:sdt` pass their content on; a placeholder `w:sdt` is skipped without a warning.
- `w:del` and `w:moveFrom` are skipped with their subtree; `w:delText`, `w:instrText` and `w:delInstrText` give no text.
- `w:drawing`, `w:pict` and `w:object` give one `ImageSkipped`, or `TextBoxSkipped` when they contain a `txbxContent`. A `txbxContent` met elsewhere gives `TextBoxSkipped`. Footnote and endnote references, `w:sym`, and any unknown element with a `w:t` inside give `OtherSkipped`. Bookmarks and proofing marks are skipped without a warning.
- `mc:AlternateContent` reads only the first `mc:Choice`; `mc:Fallback` is skipped when a branch was read.
- Counted elements (`w:tbl`, `w:sdt`, `w:customXml`, `w:hyperlink`, `w:ins`, `w:moveTo`, `w:smartTag`, `w:fldSimple`, `mc:AlternateContent`) run under `DepthScope`; 64 levels are accepted and 65 give `DocxError::Limit` with no partial result.
- `DocxWarning` gains `ImageSkipped`, `TextBoxSkipped`, `OtherSkipped`; `DocumentImport::warningText` has three Russian texts that use only the file name.

## Task Commits

Each task was committed atomically:

1. **Task 1: Tracer, one table row with two cells from w:tbl to Block(Table)** - `7a88793` (feat)
2. **Task 2: Table geometry, gridSpan, vMerge, padding, nesting limit 64** - `62ec226` (feat)
3. **Task 3: Inline content, skipped-content warnings and wrapper depth** - `1aa9321` (feat)

**Plan metadata:** `8534db6` (docs: memory journal entry for plan 02-03). The SUMMARY commit follows this file.

_Note on TDD: Task 1 is a tracer and has its own tests. Task 2 followed the red-green order: tests were written first, a red run gave 8 failures, then the implementation made them pass. Task 3 was written in the opposite order: the implementation was written first and its tests after it, so no red run exists for Task 3. All 26 new Task 3 test rows passed on their first run, and each name was confirmed in the QTest PASS list._

## Files Created/Modified

- `src/documents/DocxContent.cpp` (modified) - `ReadState`, `DepthScope`, `Sink`, `readBlockChild`/`readChild`, `readContent`, `readParagraph`, `readBlockParagraph`, `readRun`, `readText`, `appendText`, `appendSymbol`, `readSdt`, `readAlternate`, `skipSubtree`, `skipUnknown`, `readGridColumns`, `readSpan`, `readCellProps`, `readCell`, `readRowChildren`, `readRow`, `readTableChildren`, `finishTable`, `readTable`; the 02-01 paragraph reader is replaced by `readParagraph`.
- `src/documents/DocxTypes.h` (modified) - `DocxWarning::ImageSkipped`, `TextBoxSkipped`, `OtherSkipped` appended; `kMaxNestingDepth = 64`.
- `src/application/DocumentImport.cpp` (modified) - three `warningText` branches, no `default`.
- `tests/DocxContentTest.cpp` (modified) - 15 new test functions (14 from the plan plus `rowsInsideWrapperKeepCells`); helpers `gridXml`, `tableXml`, `rowXml`, `cellXml`, `oneCellTable`, `columnCoverage`, `collectTexts`, `checkWarnings`; `wrapBody` now declares the prefixes r, mc, wp, a, pic, wps and v.
- `docs/memory.md` (modified) - one journal entry, in its own commit (see Deviations).

## Decisions Made

- OQ1, OQ7, OQ10 (footnotes), OQ11 applied as the plan states them (defaults, owner confirmation pending).
- Choices where the plan is silent (implementation decisions, listed for review):
  - A cell that starts beyond the grid gives no cell; if it holds content, one `OtherSkipped` is added (SAFE-04: counted, not lost).
  - An orphan continuation (`vMerge continue` with no restart above it) is output as a normal cell, with its content. The plan said "пустая ячейка"; the content is kept instead of dropped.
  - `w:sdt`, `w:sdtContent` and `w:customXml` at row or table level are entered as counted elements, so cells inside them are read. Without this the cells would be lost with no warning.
  - `mc:Fallback` is read when it comes before any `mc:Choice` (non-schema order). In schema order the first branch wins.
  - `w:tcPrChange` and other children of `w:tcPr` are skipped; only `w:gridSpan` and `w:vMerge` are read there.
- Warnings for skipped content are appended in the order the elements are met; a dropped cell's warning comes at the end of its table.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical functionality] Row- and table-level wrappers**
- **Found during:** Task 3 review
- **Issue:** The plan's skip rules did not cover a `w:sdt` or `w:customXml` around rows or cells. Such cells would have been dropped without a warning.
- **Fix:** `readTableChildren` and `readRowChildren` enter these wrappers as counted elements; test `rowsInsideWrapperKeepCells` added.
- **Files modified:** `src/documents/DocxContent.cpp`, `tests/DocxContentTest.cpp`
- **Commit:** `1aa9321`

**2. [Rule 2 - Missing critical functionality] Memory journal entry**
- **Found during:** wrap-up after Task 3
- **Issue:** CLAUDE.md and AGENTS.md require `docs/memory.md` to be updated after a meaningful task; the file is not in this plan's `files_modified`.
- **Fix:** One journal entry in its own commit (`8534db6`), as plans 02-01 and 02-02 did. `STATE.md` and `ROADMAP.md` were not touched.
- **Commit:** `8534db6`

**3. [Formatting] clang-format applied to the changed files**
- **Found during:** Tasks 1, 2 and 3
- **Issue:** `clang-format --dry-run --Werror` flagged lines written in this plan.
- **Fix:** `clang-format -i` on `DocxContent.cpp`, `DocxContentTest.cpp`, `DocxTypes.h` and `DocumentImport.cpp`. The four files passed the check at the base commit, so only lines of this plan changed.

**4. [Code quality] clang-tidy nodiscard**
- **Found during:** Task 3 clang-tidy run
- **Issue:** `DepthScope::exceeded` was not marked `[[nodiscard]]`.
- **Fix:** Marked; no behavior change.

**Total deviations:** 4 (2 Rule 2, 1 formatting, 1 code quality) plus the plan's own choices listed under Decisions Made. **Impact:** no change to the plan's contracts; the two Rule 2 items add behavior the plan did not list.

## Issues Encountered

- `docx_content_test.exe` prints nothing to stdout in this shell (QTest output is not captured); totals and per-test PASS and FAIL lines were read from `-o <file>,txt` output.
- The sandbox refused compound shell commands that mixed `$TMPDIR` with `docx_content_test` and with git; each command was split into plain commands run from the worktree.
- The `clang-format -i` run reported that files changed on disk; these were this session's own edits, not outside changes.
- The grep counts in the plan's acceptance criteria for `inlineNestingBoundedForEveryWrapper` and `inlineWrappersAndTracking` give 2 (the function name also appears in its `_data` companion), not 1; the functions themselves are present once each.

## Known Stubs and Known Gaps

- `DocumentImport::warningText` has three new texts with no direct test. `tests/DocumentImportTest.cpp` is not in this plan's `files_modified`; the owner checks the texts in phase 3 (OQ8).
- `w:sdt` placeholder detection needs `w:showingPlcHdr` before `w:sdtContent` (schema order). A file that puts it after the content would show the placeholder text.
- Footnote text is not read (D-10); each reference gives one `OtherSkipped`. Owner confirmation pending (footnote handling belongs to the OQ10 decision).
- `word/comments.xml`, header and footer parts are not counted here (plan 02-04).
- `gridSpan` above `kMaxModelNodes` is clamped, not refused; the synthetic test covers only values within the grid.
- The drawing and text-box fixtures are our reading of the format, not a Word-saved file (phase 3, TEST-02).

## Threat Flags

None beyond the plan's `<threat_model>` (T-02-08 to T-02-11). The change adds no network, auth or file-access surface; `src/documents/` takes bytes only.

## Verification: commands and real results

Run from the worktree `C:/Users/Apostol/CLionProjects/AI-Resume-Assistant/.claude/worktrees/agent-a5d0c2fa88c05fa0c`, with `export PATH="/c/msys64/mingw64/bin:$PATH"`.

| Command | Result |
|---|---|
| `git merge --ff-only 4a45917`, then `git rev-parse HEAD` | fast-forward; HEAD = 4a4591743df64d7a2e647067dd53efbe1b206233 |
| `cmake --preset mingw-debug` (fresh worktree) | configure and generate exit 0 |
| Baseline `cmake --build --preset debug` (base commit) | exit 0 |
| Baseline `ctest --preset debug` (base commit) | 100% passed, 6 of 6 |
| Task 1: `cmake --build --preset debug` | exit 0 |
| Task 1: `ctest --preset debug -R docx_content --no-tests=error` | 1/1 passed |
| Task 1: `docx_content_test -o ...,txt` | Totals 60 passed, 0 failed (`tableWithOneRowKeepsCells`, `cellTextCountsTowardTextLimit` PASS) |
| Task 2 red run (tests first) | Totals 62 passed, 8 failed: `tableRowsKeepGridWidth` (5 rows), `vMergeNotDuplicated`, `nestingDepthLimit`, `paddingCellsCountedInNodeLimit`; `emptyAndNestedTables` and `tableNodesCountedInLimit` already passed on the Task 1 code |
| Task 2 green | Totals 70 passed, 0 failed; `ctest --preset debug` 100% passed, 6 of 6 |
| Task 3 first run (after implementation and 26 new test rows) | Totals 96 passed, 0 failed |
| Task 3 final (after `rowsInsideWrapperKeepCells` and `[[nodiscard]]`) | docx_content_test Totals 97 passed, 0 failed; `ctest --preset debug` 100% passed, 6 of 6 |
| `document_import_test -o ...,txt` (final) | Totals 17 passed, 0 failed |
| `clang-format --dry-run --Werror` on `DocxContent.cpp`, `DocxContentTest.cpp`, `DocxTypes.h`, `DocumentImport.cpp` | exit 0 |
| `clang-tidy src/documents/DocxContent.cpp -p build/mingw-debug` | exit 0; style findings only: 63 `modernize-use-trailing-return-type`, 7 `modernize-use-designated-initializers`, 4 `performance-enum-size`; the one `modernize-use-nodiscard` was fixed |
| Acceptance greps (plan Task 1-3) | `kMaxTextChars`, `gridCol`, `kMaxNestingDepth`, `kMaxModelNodes`, `noBreakHyphen`, `softHyphen`, `ptab`, `sym`, `delText`, `instrText`, `rowSpan` in `DocxContent.cpp` each >= 1; `kMaxNestingDepth = 64` on line 64 of `DocxTypes.h`; the three new codes in `DocxTypes.h` and `DocumentImport.cpp` |

**Not run (stated explicitly):**
- Release build (`cmake --build --preset release`) and `ctest --preset release`.
- GitHub Actions CI and the Inno Setup installer.
- Mutation check (breaking the depth bound, the vMerge rule or the placeholder check and confirming failures).
- `web_e2e` (not registered: no `tests/e2e/node_modules`).
- A check on a Word-saved file for tables, drawings, text boxes and content controls (phase 3, TEST-02).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 02-04 adds `ZipArchive::entryNames()`, the header and footer count, the `comments.xml` count, and the import API field `document` in `ImportedDocx`; it uses `DocxWarning::OtherSkipped` for the comments part.
- Owner confirmation pending: OQ1 (inserts and text boxes), OQ7 (depth 64), OQ10 (footnotes and comments), OQ11 (symbols), A2, A6, A7, A9, A11, A12, A14, A15; the three new warning texts (OQ8).

## Self-Check

- Commits exist as ancestors of HEAD: `7a88793`, `62ec226`, `1aa9321`, `8534db6` (checked with `git merge-base --is-ancestor`, run from the worktree).
- Files exist: `src/documents/DocxContent.cpp`, `src/documents/DocxTypes.h`, `src/application/DocumentImport.cpp`, `tests/DocxContentTest.cpp`, `docs/memory.md`.
- Ledger: `plan_head_before` 4a45917; 4 commits measured before this SUMMARY was written.
- Not edited: `.planning/STATE.md`, `.planning/ROADMAP.md`, `state.json`, `milestone.lock`.

## Self-Check: PASSED
