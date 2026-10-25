---
phase: 02-chtenie-soderzhimogo-docx
plan: 02
subsystem: documents
tags: [docx, styles, numbering, headings, lists, basedOn, qt-test]

# Dependency graph
requires:
  - phase: 02-01
    provides: DocxModel.h (Paragraph, Block, DocxDocument), PartReader, readDocxContent, ParseBudget
provides:
  - headingLevel of a paragraph from word/styles.xml (styleId, English and Russian names, outlineLvl, basedOn chain, direct outlineLvl)
  - listLevel, listKind and listMarker from word/numbering.xml (numId, abstractNum, numPr in the paragraph and in its style)
  - DocxWarning::ListFormatApproximated, one per numId
affects: [02-03 tables and skipped content, 02-04 import API and warnings, phase 3 UI]

# Actuals (#2632): same estimateTokens scale (chars/4 over the realized diff)
actuals:
  tokens: 14794    # 59174 bytes of `git diff cfd29fd HEAD -- src tests docs` / 4; byte-based, so Cyrillic is slightly overstated
  tasks: 3
  commits: 4       # MEASURED: git rev-list --count cfd29fd..HEAD before this SUMMARY (3 task commits + 1 docs commit)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "forEachChild: one walk over the direct children of an element; each handler consumes its child whole"
    - "Style chain walk with a depth bound and a cycle stop (T-02-06), used by heading and numbering lookups"
    - "Per-numId list counters with restart on a higher level; approximation warning once per numId"

key-files:
  created: []
  modified:
    - src/documents/DocxContent.cpp
    - src/documents/DocxTypes.h
    - src/application/DocumentImport.cpp
    - tests/DocxContentTest.cpp
    - docs/memory.md

key-decisions:
  - "OQ4 (default, owner confirmation pending): heading by styleId Heading1-9, by name 'heading N' (case-insensitive) or 'Заголовок N', by outlineLvl 0-8 (+1); outlineLvl 9 on the paragraph or on a style stops the search with 0; basedOn chain at most 16 styles; a cycle ends the walk without error"
  - "OQ5 (default, owner confirmation pending): bullet is U+2022; any other format is a decimal 'N.' with restart on a higher level; a format that is not reproduced (numFmt other than bullet or decimal, or decimal with lvlText other than '%1.') gives one ListFormatApproximated per numId"
  - "A4: numId 0 means no list. A5: lvlText is never copied as a marker. A13: a style's numPr gives the list, and the level is the w:lvl whose w:pStyle matches the paragraph style (the style's own ilvl is ignored)"
  - "start: a negative or non-numeric value becomes 1, a value above 1000000 becomes 1000000 (T-02-07)"

requirements-completed: [DOCX-03, DOCX-04]  # partial: the parser is checked on synthetic fixtures only (A1, A3, A4, A5, A13), not on a Word-saved file (phase 3, TEST-02); DOCX-04 covers bullets and decimal numbers with nesting, not roman, letters or custom lvlText (OQ5)

# Duration and timeline
duration: not measured (plan start time was not recorded in this session)
completed: 2026-10-25
commits: 4
plan_head_before: cfd29fdf498a5f3af03696c51d7490028e0645dd
plan_head_after: c1fa33bab9380475c85b7298ad89662f39f35173
status: complete
---

# Phase 2 Plan 02: Заголовки и списки DOCX Summary

**Headings come from word/styles.xml (styleId, English and Russian names, outline levels, basedOn chain) and lists from word/numbering.xml (bullet and decimal markers, nested levels 0-8, counters that restart on a higher level); a format that is not reproduced is reported once per list.**

## Performance

- **Duration:** not measured
- **Started / Completed:** not recorded; completed 2026-10-25
- **Tasks:** 3 of 3 completed
- **Files modified:** 4 code and test files, 1 docs file

## Accomplishments

- `readStyles` reads word/styles.xml once, before the body, into a table keyed by styleId (name, basedOn, outlineLvl, numId of the style's numPr). The same token checks as the body apply, so a DOCTYPE in styles.xml is still refused with `UnsafeXml`.
- A paragraph's heading level follows the order of the plan: its own w:outlineLvl first, then its style chain (styleId, then name, then outlineLvl, one style at a time). An outlineLvl of 9 stops the search with 0. The chain is cut at 16 styles and a repeated styleId ends it without an error.
- `readNumbering` reads word/numbering.xml once: w:num maps numId to abstractNumId, w:abstractNum holds up to nine levels with numFmt, start, pStyle. A missing or invalid numId gives no list; numId 0 gives no list.
- A paragraph's list comes from its own numPr, or else from the numPr of its style chain. For a style reference, the level is the lowest w:lvl whose w:pStyle equals the paragraph's style.
- Bullets get U+2022. Other formats get a decimal number with a dot, counted per numId; a level restarts the deeper levels. `ListFormatApproximated` is added once per numId, in document order.

## Task Commits

Each task was committed atomically:

1. **Task 1: Tracer, Heading1 from styles.xml to headingLevel** - `a814d06` (feat)
2. **Task 2: Heading rules, basedOn chain, name and outlineLvl, cycle guard** - `0352e47` (feat)
3. **Task 3: Lists from numbering.xml, levels 0-8, restart, style numPr, start clamp, approximation warning** - `40f8a3e` (feat)

**Plan metadata:** `c1fa33b` (docs: memory journal entry for plan 02-02). The SUMMARY commit follows this file.

_Note: the plan's tasks are marked `tdd="true"`. Each task's tests were written in the same commit as its code and run afterwards on that commit. No separate failing run was recorded before the implementation, so this plan does not claim a red-first sequence._

## Files Created/Modified

- `src/documents/DocxContent.cpp` (modified) - `readStyles`, `readStyle`, `readParagraphProps`, `readNumPr`, `readNumbering`, `readNum`, `readAbstractNum`, `readLevel`, `styleChain`, `headingLevel` (with `headingFromStyleId`, `headingFromName`, `headingFromOutline`), `listReference`, `levelForStyle`, `advanceCounter`, `applyList`. The 02-01 protective pass `checkPartIsSafe` is kept for no part; both optional parts are now read to the end by their readers.
- `src/documents/DocxTypes.h` (modified) - `DocxWarning::ListFormatApproximated`, appended to the enum.
- `src/application/DocumentImport.cpp` (modified) - one `warningText` branch for `ListFormatApproximated`; the text uses only the file name.
- `tests/DocxContentTest.cpp` (modified) - nine new test functions: `headingByStyleIdIsHeadingLevelOne`, `headingByStyleIdNameOrOutline` (14 data rows), `cyclicBasedOnTerminates`, `styleChainDepthIsBounded`, `listsKeepKindAndLevel` (8 data rows), `numberedCountersRestartOnHigherLevel`, `styleNumPrLevelFromPStyle`, `numberingStartClamped`, `unsupportedNumberingFormatCountedOncePerList` (5 data rows).
- `docs/memory.md` (modified) - one journal entry, in its own commit (see Deviations).

## Decisions Made

- OQ4 and OQ5 are applied as the plan states them (defaults, owner confirmation pending).
- Choices where the plan is silent (implementation decisions, listed for review):
  - A style's numPr is searched along the whole basedOn chain; the first style that has a numId decides.
  - A paragraph's numPr without a numId falls through to the style's numPr. A paragraph's numPr with numId 0 overrides the style and gives no list.
  - A level that the w:abstractNum does not define is treated as decimal, start 1, not approximated.
  - When a styleId, abstractNumId or numId is defined twice, the first definition is kept.
- `readStyles` and `readNumbering` replace the 02-01 protective pass for the two parts: they check every token and read to EndDocument, and they also produce the tables.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical functionality] Memory journal entry**
- **Found during:** wrap-up after Task 3
- **Issue:** CLAUDE.md and AGENTS.md require `docs/memory.md` to be updated after a meaningful task; the file is not in this plan's `files_modified`.
- **Fix:** One journal entry in its own commit (`c1fa33b`), as plan 02-01 did. `STATE.md` and `ROADMAP.md` were not touched.
- **Files modified:** `docs/memory.md`
- **Committed in:** `c1fa33b`

**2. [Scope addition] Test for the chain depth bound**
- **Found during:** Task 2
- **Issue:** The plan's Task 2 lists only `cyclicBasedOnTerminates`. The depth limit `kMaxStyleChainDepth` (16, T-02-06) was not pinned by any test.
- **Fix:** Added `styleChainDepthIsBounded`: a chain of 10 styles reaches the heading (level 2); a chain of 20 does not (level 0).
- **Committed in:** `0352e47`

**3. [Formatting] clang-format applied to the changed files**
- **Found during:** Tasks 1, 2 and 3
- **Issue:** `clang-format --dry-run --Werror` flagged lines written in this plan.
- **Fix:** `clang-format -i` on `DocxContent.cpp`, `DocxContentTest.cpp`, `DocxTypes.h`, `DocumentImport.cpp`. The diffs of the two header and application files were checked: only the added lines changed.
- **Committed in:** `a814d06`, `0352e47`, `40f8a3e`

**Total deviations:** 3 (1 Rule 2, 1 test addition, 1 formatting). **Impact:** no change to production behavior beyond the plan's rules.

## Issues Encountered

- The shell refused compound git commands that name the worktree path in a complex form; each git call was split into a plain command.
- `document_import_test.exe` needs `C:/msys64/mingw64/bin` on PATH to load `libzip.dll`; without it the process exits with code 127. This is a shell setup issue, not a code issue.
- QTest output sent to stdout was not captured in this shell; totals were read from `-o <file>,txt` output.
- The research file `02-RESEARCH.md` was truncated in one read; the plan's own text (Tasks 1-3 and the OQ4, OQ5 notes) was used for the rules.

## Known Stubs and Known Gaps

- **OQ5 false positive (open for owner decision):** the plan's rule counts a decimal level whose `lvlText` is not `%1.` as approximated. Word's usual multi-level lists use `%2.` on level 1, and the number shown there is the same as this parser's output, so such a list produces a `ListFormatApproximated` warning without a real loss. The plan's rule was implemented as written; not changed here.
- Roman, letter and custom-`lvlText` markers are not reproduced; they show a decimal `N.` and produce one warning per numId.
- `w:lvlOverride` and `w:startOverride` in `w:num` are not read; a restart or start override there is ignored.
- A level that a w:abstractNum does not define is shown as decimal from 1 with no warning (the plan does not say what to do).
- Tables, drawings, text boxes and fields in the body are still skipped (02-03).
- The text of `ListFormatApproximated` in `DocumentImport::warningText` has no direct test; `tests/DocumentImportTest.cpp` is not in this plan's `files_modified`. The owner checks the text in phase 3 (OQ8).
- Russian style names and numbering rules are checked on synthetic fixtures only (A1, A3, A4, A5, A13); no Word-saved file was checked (phase 3, TEST-02).

## Threat Flags

None beyond T-02-06 and T-02-07 (chain depth at 16, cycle stop, start clamp, qint64 counters). The change adds no network, auth or file-access surface.

## Verification: commands and real results

| Command | Result |
|---|---|
| `git merge --ff-only cfd29fd`, then `git rev-parse HEAD` | fast-forward; HEAD = cfd29fd |
| `cmake --preset mingw-debug` (fresh worktree) | configure and generate exit 0 |
| Baseline `cmake --build --preset debug`, before any change | exit 0 |
| Baseline `ctest --preset debug` | 100% passed, 6 of 6 |
| After Task 1: `cmake --build --preset debug`; `ctest --preset debug -R docx_content --no-tests=error`; `docx_content_test -o ...,txt` | build exit 0; 1 of 1 passed; Totals 26 passed, 0 failed |
| After Task 2: `docx_content_test` (same run) | Totals 42 passed, 0 failed; `ctest -R docx_content` and `ctest -R document_import` both passed |
| After Task 3: build exit 0; `docx_content_test` | Totals 58 passed, 0 failed |
| Final `cmake --build --preset debug` | exit 0 |
| Final `ctest --preset debug` | 100% passed, 6 of 6 (resume_e2e, resume_json, zip_archive, docx_content, document_import, web_api; web_e2e not registered, no tests/e2e/node_modules) |
| `document_import_test` (with MSYS2 bin on PATH) | Totals 17 passed, 0 failed |
| `clang-format --dry-run --Werror` on the four changed files | exit 0 |
| `clang-tidy src/documents/DocxContent.cpp -p build/mingw-debug` | exit 0; style findings only: 32 modernize-use-trailing-return-type, 2 modernize-use-designated-initializers, 3 performance-enum-size (not changed) |
| Acceptance greps (test names, `basedOn`, `toLower`, `kMaxStyleChainDepth`, `abstractNumId`, `lvlText`, `ListFormatApproximated` in `DocxTypes.h` and `DocumentImport.cpp`, `0x2022`) | all present with the required counts |

**Not run (stated explicitly):**
- Release build (`cmake --build --preset release`) and `ctest --preset release`.
- GitHub Actions CI and the Inno Setup installer.
- Mutation check (breaking the DTD check, the depth bound or the start clamp and confirming failures).
- `web_e2e` (not registered in this environment).
- A check on a Word-saved file for the Russian style names and numbering (phase 3, TEST-02).

## Self-Check

- Commits exist as ancestors of HEAD: `a814d06`, `0352e47`, `40f8a3e`, `c1fa33b` (checked with `git merge-base --is-ancestor`).
- Files exist: `src/documents/DocxContent.cpp`, `src/documents/DocxTypes.h`, `src/application/DocumentImport.cpp`, `tests/DocxContentTest.cpp`, `docs/memory.md`.
- Ledger: `plan_head_before` cfd29fd; 4 commits measured before this SUMMARY was written.

## Self-Check: PASSED
