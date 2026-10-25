---
phase: 02-chtenie-soderzhimogo-docx
plan: 04
subsystem: documents
tags: [docx, import-service, headers, footers, comments, text-limit, markers, qt-test, phase-gate]

# Dependency graph
requires:
  - phase: 02-03
    provides: readDocxContent model with tables, inline content, skipped-content warnings and kMaxNestingDepth
  - phase: 02-02
    provides: heading levels, list fields and ListFormatApproximated
  - phase: 02-01
    provides: DocxModel.h, readDocxContent entry point, DTD and encoding refusal, kMaxTextChars and kMaxModelNodes
provides:
  - DocumentImport::readDocxFile returns ImportedDocx::document (the parsed body model) and warnings built from it
  - Header, footer and comments parts counted by entry name (HeaderFooterSkipped per part, OtherSkipped for word/comments.xml), never read
  - Russian import text for HeaderFooterSkipped (new), and the existing Limit text reached through the service for an over-long text (D-03)
  - ZipArchive::entryNames(), the entry names kept at open
  - Phase 2 gate evidence: build, full ctest, clang-format, clang-tidy and two mutation checks
affects: [phase 3 UI (ImportedDocx::document, owner review of the new texts), TEST-02 (a Word-saved file check is still open)]

# Actuals (#2632): same estimateTokens scale (chars/4 over the realized diff)
actuals:
  tokens: 15279    # 61118 bytes of `git diff 012d735 HEAD` / 4; byte-based, so Cyrillic is slightly overstated; plan estimate was 84000
  tasks: 3
  commits: 5       # MEASURED: git rev-list --count 012d735..HEAD before this SUMMARY (3 task commits + 2 docs commits)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Parts counted by entry name: no readEntry call is made for them, so a damaged payload gives a warning, not an error (D-10, SAFE-04)"
    - "Mutation check on a layer: break one rule in a copy-restorable way, run that layer's tests, restore from a backup and compare sha256 before and after"

key-files:
  created: []
  modified:
    - src/application/DocumentImport.h
    - src/application/DocumentImport.cpp
    - src/documents/ZipArchive.h
    - src/documents/ZipArchive.cpp
    - src/documents/DocxContainer.h
    - src/documents/DocxContainer.cpp
    - src/documents/DocxTypes.h
    - tests/DocumentImportTest.cpp
    - src/documents/README.md
    - docs/memory.md

key-decisions:
  - "OQ2 (default): a header or footer part is word/header<digits>.xml or word/footer<digits>.xml, exact and case-sensitive; zero digits also match, as the plan literal says; one HeaderFooterSkipped per entry, in central-directory order"
  - "OQ10 (default): word/comments.xml gives one OtherSkipped and is never read; the owner checks the text in phase 3"
  - "OQ6 (default): ImportedDocx gets the field document; documentXml, stylesXml and numberingXml stay until phase 3"
  - "Limit for text length uses the existing Limit text (one text for unpacking, text length and nesting), per the plan; no separate text was added"
  - "warnings of ImportedDocx are built from the model's warnings, which already carry the missing-part warnings from readDocxParts"

patterns-established:
  - "Service-level tests use literal expected texts copied from the plan, not calls into DocumentImport.cpp"
  - "Message capture: a scoped qInstallMessageHandler guard restores the previous handler on every exit path"

requirements-completed: [DOCX-06, SAFE-03, SAFE-04, SAFE-05, TEST-01]
# Partial coverage, stated per requirement:
#   DOCX-06: Cyrillic text and a Cyrillic directory and file name through readDocxFile (readsDocumentContentFromCyrillicDirectory); the Word-saved file check is phase 3 (TEST-02).
#   SAFE-03: DOCTYPE in document.xml refused at service level with the Russian text; the layer tests cover styles.xml and numbering.xml.
#   SAFE-04: header, footer and comments counted at service level; images, text boxes and other skipped items counted at the layer (docx_content).
#   SAFE-05: marker test over five hostile inputs at service level; the layer test messagesDoNotCarryMarker covers the layer.
#   TEST-01: the phase test set passes on synthetic fixtures; the Word-saved file is still open (TEST-02).

# Metrics
duration: not measured (plan start time was not recorded in this session)
completed: 2026-10-25
status: complete
---

# Phase 2 Plan 04: Ворота фазы 2 (сервис импорта, колонтитулы, тексты, сквозные тесты) Summary

**readDocxFile now returns the parsed document model; headers, footers and comments are counted by name without being read, DOCTYPE and text-limit failures give Russian texts that name only the file, and the phase gate passes on the real build and the full ctest run.**

## Performance

- **Duration:** not measured (plan start time was not recorded in this session)
- **Started / Completed:** not recorded; completed 2026-10-25
- **Tasks:** 3 of 3 completed
- **Files modified:** 10 (8 code and test, 1 README, 1 journal)

## Accomplishments

- `DocumentImport::readDocxFile` calls `documents::readDocxContent` after `readDocxParts`; a parse error becomes a Russian text through `errorText`; `ImportedDocx::document` holds the model (SC1 paragraphs and headings, SC3 Cyrillic, SC4 DOCTYPE text, D-03 text limit at service level).
- `ZipArchive::entryNames()` returns the names collected by the structural pass at open, in central-directory order, with no content read.
- `readDocxParts` counts `word/header<digits>.xml` and `word/footer<digits>.xml` as one `HeaderFooterSkipped` each, and `word/comments.xml` as one `OtherSkipped`. `readPart` is still called only for the three body parts.
- The text `В файле «%1» есть колонтитул: его текст не показан.` is added for `HeaderFooterSkipped`; `DocumentImport::warningText` has no default branch.
- Behavioral proof of D-10: the bytes of the header, footer and comments payloads are damaged in place (CRC no longer matches); a direct `ZipArchive::readEntry` on the header returns `Corrupt`, and the import still succeeds with three warnings.
- The layer README (`src/documents/README.md`) now describes the actual state: the codes, the four sources of `Limit`, the statuses of `kMaxTextChars`, `kMaxModelNodes` and `kMaxNestingDepth`, and the reading rules.

## Task Commits

Each task was committed atomically:

1. **Task 1: Tracer, readDocxContent wired into readDocxFile** - `ea4df08` (feat)
2. **Task 2: Header, footer and comments counted by name, with the text and the no-read test** - `106326e` (feat)
3. **Task 3: Service-level tests for headers, text limit, texts and markers** - `faa22f6` (test)
4. **Task 3: Layer README update** - `624e187` (docs)
5. **Plan journal entry** - `a5f1d76` (docs)

**Plan metadata:** this SUMMARY commit follows (docs: complete plan 02-04). STATE.md and ROADMAP.md were not changed (the orchestrator owns them).

_Note: none of the tasks was TDD-tagged in the plan. Each test was run against the code of the same task; the new service tests were not run red first._

## Files Created/Modified

- `src/application/DocumentImport.h` - `ImportedDocx::document` added; comment on the fields updated.
- `src/application/DocumentImport.cpp` - `readDocxContent` call, warnings built from the model, `HeaderFooterSkipped` text.
- `src/documents/ZipArchive.h/.cpp` - `entryNames()`; `checkEntryStructure` collects names; `Impl::names`.
- `src/documents/DocxContainer.h/.cpp` - header, footer and comments counting by name (`isNumberedXmlPart`, `isHeaderOrFooterEntry`); comment on `DocxParts::warnings`.
- `src/documents/DocxTypes.h` - `DocxWarning::HeaderFooterSkipped` appended.
- `tests/DocumentImportTest.cpp` - two phase-1 fixtures made well-formed (see Deviations); tests `readsDocumentContentFromCyrillicDirectory`, `doctypeFileGivesUnsafeXmlTextNamingFileOnly`, `headerFooterAndCommentsBytesNeverRead`, `headerAndFooterPartsCountedAsWarnings`, `overTextLimitGivesLimitText`, `warningTextsArePairwiseDistinct`, `messagesAndLogsCarryNoMarker`.
- `src/documents/README.md` - layer README (see Accomplishments).
- `docs/memory.md` - journal entry for plan 02-04 with the real results.

## Decisions Made

- Plan decisions applied as written (defaults, owner confirmation pending): OQ2, OQ6, OQ8, OQ10, and the single `Limit` text for text length.
- `word/header.xml` (no digits) counts as a header, because the plan literal says "zero or more digits". This follows the plan; it is not a separate choice.
- Warning order in `ImportedDocx::warnings` is the order of `document.warnings`: the parts' warnings first (missing parts, then header and footer, then comments), then body warnings in document order.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Phase-1 import test fixtures were not well-formed WordprocessingML**
- **Found during:** Task 1
- **Issue:** `readDocxContent` correctly refuses a `word/document.xml` with no `w:body` (`Corrupt`). Three phase-1 tests used a bare `<w:document/>` or `<w:document>Тело резюме</w:document>` and would have failed once the service parsed the body.
- **Fix:** The fixtures were wrapped in `w:document` / `w:body` / `w:styles` / `w:numbering` elements with the WordprocessingML namespace (helpers `wmlElement` and `wordDocument` in `tests/DocumentImportTest.cpp`). The expectations (byte equality of the parts, the warning counts, the empty warning list) were not changed.
- **Files modified:** `tests/DocumentImportTest.cpp`
- **Verification:** `document_import_test` Totals 19 passed after Task 1, 0 failed.
- **Committed in:** `ea4df08`

**2. [Rule 2 - Missing critical functionality] Direct check that the damage is real**
- **Found during:** Task 2
- **Issue:** A test that only checks the import succeeds would also pass if the damage had not been applied.
- **Fix:** `headerFooterAndCommentsBytesNeverRead` opens the damaged archive with `ZipArchive::open` and asserts that `readEntry("word/header1.xml", ...)` returns `Corrupt`.
- **Files modified:** `tests/DocumentImportTest.cpp`
- **Committed in:** `106326e`

**3. [Formatting] clang-format on the new lines**
- **Found during:** Task 2 and the phase gate
- **Issue:** `clang-format --dry-run --Werror` flagged lines written in this plan (in `DocxTypes.h`, `DocumentImportTest.cpp`).
- **Fix:** `clang-format -i` on the seven files of Task 2 and on `tests/DocumentImportTest.cpp` before the Task 3 commit. Every file was clean at the base (012d735), so the change touches only lines written in this plan.
- **Committed in:** `106326e`, `faa22f6`

**Total deviations:** 3 (1 Rule 3, 1 Rule 2, 1 formatting). **Impact:** no production behavior changed beyond the plan; the expected texts and warning counts are the plan's.

## Issues Encountered

- The worktree sandbox refused some compound git commands that name the worktree path in a complex form; those calls were split into plain commands. No command ran outside the worktree.
- The shell does not keep `PATH` between calls; `export PATH="/c/msys64/mingw64/bin:$PATH"` was set in each build or test call.
- The first Task 1 commit message had a wording error; it was corrected with `git commit --amend` before any push (the commit was local and unpublished).

## Mutation checks (real runs, file restored after each run)

Both mutations were applied to `src/documents/DocxContent.cpp`, built, and tested with `docx_content_test`. The file was restored from a copy taken before the first mutation, and its hash was compared before and after (`sha256sum`).

| Mutation | Result | Hash before / after restore |
|---|---|---|
| DTD refusal disabled: `if (false && token == QXmlStreamReader::DTD)` | Totals: 91 passed, 6 failed. Failed: `doctypeRejected` (4 data rows), `doctypeResultHasNoContent`, `messagesDoNotCarryMarker` | `877a4bcd…` / `877a4bcd…` (same) |
| `w:del` skip removed from `skipUnknown` | Totals: 96 passed, 1 failed. Failed: `inlineWrappersAndTracking` ("deletion gives no text": the deletion gave a warning instead of a silent skip) | `877a4bcd…` / `877a4bcd…` (same) |

`DocxTypes.h` was checked too (`c11ccfb4…`, unchanged). `git status` was clean after the restore.

## Phase 2 gate: commands and real results

| Command | Result |
|---|---|
| `cmake --preset mingw-debug` (fresh worktree) | configure and generate OK; `web_e2e` skipped (no `tests/e2e/node_modules`) |
| `cmake --build --preset debug` before any change | exit 0 |
| `ctest --preset debug` before any change | 100% passed, 6 of 6 |
| `cmake --build --preset debug` on the final code | exit 0; 0 lines matching `error:` |
| `ctest --preset debug` on the final code | 100% passed, 6 of 6 (resume_e2e, resume_json, zip_archive, docx_content, document_import, web_api) |
| `docx_content_test -o …,txt` (final) | Totals 97 passed, 0 failed |
| `document_import_test -o …,txt` (final) | Totals 24 passed, 0 failed |
| `zip_archive_test -o …,txt` (final) | Totals 26 passed, 0 failed; `tests/ZipArchiveTest.cpp` unchanged in this plan |
| `web_api_test -o …,txt` (final) | Totals 11 passed, 0 failed |
| `resume_e2e_test -o …,txt` (final) | Totals 3 passed, 0 failed |
| `resume_json_test -o …,txt` (final) | Totals 15 passed, 0 failed |
| `clang-format --dry-run --Werror` on the 12 C++ files changed in phase 2 | exit 0 |
| `clang-tidy src/documents/DocxContent.cpp src/application/DocumentImport.cpp -p build/mingw-debug` | exit 0; 80 `warning:` lines: 69 `modernize-use-trailing-return-type`, 7 `modernize-use-designated-initializers`, 4 `performance-enum-size`. In `DocxContent.cpp` 69, in `DocumentImport.cpp` 5, in headers 6. Style only; not fixed (no new functions were added in this plan; the 5 in `DocumentImport.cpp` are phase-1 functions) |

Acceptance greps in this plan were run and match: `readDocxContent` 1 in `DocumentImport.cpp`; `documents::DocxDocument document` 1 in `DocumentImport.h`; `entryNames` 1 in `ZipArchive.h`; `readPart(` 3 and `readEntry(` 1 in `DocxContainer.cpp`; `qsizetype{7}` 2 and `qsizetype{5}` 0 in `DocumentImportTest.cpp`; `Не действует` 0 in the README; the README contains `UnsafeXml`, `HeaderFooterSkipped`, `kMaxNestingDepth`, `ListFormatApproximated` and `kMaxModelNodes`.

**Not run (stated explicitly):**
- Release build (`cmake --build --preset release`) and `ctest --preset release`.
- GitHub Actions CI and the Inno Setup installer.
- `web_e2e` (not registered in this environment).
- clang-tidy on files other than the two named in the plan.
- A manual check on a Word-saved DOCX (TEST-02, phase 3).

## Owner questions (open, not decided here)

- Texts of all new codes, to be checked in phase 3 (OQ8): `HeaderFooterSkipped`, `OtherSkipped` for comments, `ListFormatApproximated`, `ImageSkipped`, `TextBoxSkipped`, and the `Limit` text, which covers unpacking, text length and nesting.
- Limit values chosen by the planner, awaiting confirmation: `kMaxModelNodes` = 50000 (OQ9), `kMaxNestingDepth` = 64 (OQ7).
- OQ1 (insertions and text fields), OQ2 (headers and footers counted by name), OQ3 (encodings, A10), OQ10 (comments.xml), A1, A2, A10, A13.
- OQ5 false positive: a decimal list at level 1 or higher with `lvlText` `%2.` gives `ListFormatApproximated` although the number shown is the same. The plan's rule is implemented as written.
- Footnotes and endnotes: one `OtherSkipped` for each reference; the owner confirms this.

## Known Stubs and Known Gaps

- **No service-level test for lists, tables and hyperlinks** (SC1 list part and SC2). They are covered in `docx_content`, and the service passes the model through without change, but no test in `document_import_test` reads them.
- The text of `ListFormatApproximated` has no test that compares its content. The texts of `HeaderFooterSkipped` and of `OtherSkipped` for comments are compared in `headerFooterAndCommentsBytesNeverRead`; `warningTextsArePairwiseDistinct` compares only that the seven texts differ. All texts are owner-pending.
- `word/header.xml` with no digits counts as a header (plan literal "zero or more digits").
- TEST-02 (a DOCX saved by Word) is still open until phase 3.

## Threat Flags

None beyond the plan's threat model. T-02-12 (no content in messages) is covered by `messagesAndLogsCarryNoMarker` and `doctypeFileGivesUnsafeXmlTextNamingFileOnly`; T-02-13 (`word/_rels` not read, hyperlink addresses not output) is a README rule backed by the reader; T-02-14 (exact, case-sensitive part names) is covered by `headerAndFooterPartsCountedAsWarnings`. The change adds no network, auth or write path.

## Next Phase Readiness

- Phase 3 can use `ImportedDocx::document` (model contract in `src/documents/DocxModel.h`) and the warning list.
- Owner review of the new Russian texts is due in phase 3 together with the texts of phase 1 (01-VERIFICATION).
- TEST-02 (file saved by Word), the draft rule (SAFE-01) and the UI (UI-01…UI-03) are phase 3 work.

## Self-Check

Checked after writing this file and before its commit:

- Created or changed files exist (see the list in Files Created/Modified; checked with `ls`).
- Commits exist as ancestors of HEAD: `ea4df08`, `106326e`, `faa22f6`, `624e187`, `a5f1d76` (checked with `git merge-base --is-ancestor`).
- Ledger: `plan_head_before` 012d7356e433e42f5b17102c9ad21ef6363ea248; 5 commits measured with `git rev-list --count 012d735..HEAD` before this SUMMARY; `plan_head_after` a5f1d76be992afc832c9b049f9479a9eabc06a9a.
