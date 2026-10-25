---
phase: 02-chtenie-soderzhimogo-docx
verified: 2026-10-25T02:53:38Z
status: gaps_found
score: 4/5 roadmap truths verified; 10/11 requirements satisfied
covered_files:
  - ".planning/phases/02-chtenie-soderzhimogo-docx/02-01-PLAN.md"
  - ".planning/phases/02-chtenie-soderzhimogo-docx/02-01-SUMMARY.md"
  - ".planning/phases/02-chtenie-soderzhimogo-docx/02-02-PLAN.md"
  - ".planning/phases/02-chtenie-soderzhimogo-docx/02-02-SUMMARY.md"
  - ".planning/phases/02-chtenie-soderzhimogo-docx/02-03-PLAN.md"
  - ".planning/phases/02-chtenie-soderzhimogo-docx/02-03-SUMMARY.md"
  - ".planning/phases/02-chtenie-soderzhimogo-docx/02-04-PLAN.md"
  - ".planning/phases/02-chtenie-soderzhimogo-docx/02-04-SUMMARY.md"
  - "CMakeLists.txt"
  - "src/application/DocumentImport.cpp"
  - "src/application/DocumentImport.h"
  - "src/documents/DocxContainer.cpp"
  - "src/documents/DocxContainer.h"
  - "src/documents/DocxContent.cpp"
  - "src/documents/DocxContent.h"
  - "src/documents/DocxModel.h"
  - "src/documents/DocxTypes.h"
  - "tests/DocumentImportTest.cpp"
  - "tests/DocxContentTest.cpp"
  - "tests/DocxFixtures.h"
covered_digest: "v3:sha256:aebc0fe0f56162f75e99812a2f933949a1002604841058cff8310feac320d360"
behavior_unverified: 0
overrides_applied: 0
gaps:
  - truth: "SAFE-04 and ROADMAP SC4: skipped constructs are counted and returned as warnings, not lost silently"
    status: failed
    reason: "w:altChunk and w:subDoc (content that lives in another part) fall through skipUnknown. That function warns only for pictures, text boxes, footnote/endnote references, symbols, and elements that contain a w:t. An altChunk or subDoc element has no w:t, so it is dropped with no warning. No rule, code path or test mentions altChunk or subDoc."
    artifacts:
      - path: "src/documents/DocxContent.cpp"
        issue: "skipUnknown (lines 962-983) has no rule for w:altChunk or w:subDoc; a body-level or paragraph-level occurrence gives zero warnings"
    missing:
      - "Count w:altChunk and w:subDoc (and any element that references content in another part) as OtherSkipped in skipUnknown"
      - "Add a synthetic test in tests/DocxContentTest.cpp that expects exactly one OtherSkipped for a body-level w:altChunk and for one inside a paragraph"
deferred:
  - truth: "Word-saved DOCX is read correctly (A1, A6, A7, A12; Russian style names and numbering rules on real files)"
    addressed_in: "Phase 3"
    evidence: "Phase 3 requirement TEST-02 in .planning/REQUIREMENTS.md and the Phase 3 entry in .planning/ROADMAP.md; 02-02 and 02-03 SUMMARY known-gap sections defer the same items"
---

# Phase 2: Чтение содержимого DOCX — Verification Report

**Phase Goal:** Путь к `.docx` превращается в упорядоченную модель документа (абзацы, заголовки, списки, таблицы) с предупреждениями о пропущенном; текст читается точно, враждебный XML отклоняется, текст резюме не утекает в логи и ошибки
**Verified:** 2026-10-25T02:53:38Z
**Status:** gaps_found
**Re-verification:** No (initial verification; no prior VERIFICATION.md in the phase directory)
**Repo state:** branch chore/map-codebase, HEAD 74fbbff

## Summary

Phase 2 delivers what the goal describes at the model layer. Paragraphs, headings, lists, tables, inline content and skipped-content warnings are read from the DOCX body into `documents::DocxDocument`, with XML hostility refused and error texts that carry no document content. The test suite passes (ctest 6/6, exit 0).

The phase does not pass the gate. SAFE-04 requires that skipped content is counted and never lost silently. Two constructs are dropped silently: `w:altChunk` and `w:subDoc` are dropped with no warning and no test, which is a BLOCKER. A vMerge continuation cell that holds text is also dropped silently, by design, which conflicts with SAFE-04's wording; this is a WARNING that needs an owner decision. Several warning texts are checked only against literal copies in the test, not against the implementation. These are WARNINGs.

## Test Run (executed by the verifier)

| Command | Exit | Result |
| ------- | ---- | ------ |
| `export PATH="/c/msys64/mingw64/bin:$PATH"; cmake --build --preset debug` | 0 | `ninja: no work to do`. Outputs were already up to date with the sources. This was not a clean rebuild. |
| `ctest --preset debug` | 0 | 100% tests passed, 6 of 6: resume_e2e, resume_json, zip_archive, docx_content, document_import, web_api. The web_e2e test is not registered in this configuration (CMake registers it only when node and tests/e2e/node_modules exist), so it did not run. |

Per-binary counts come from QTest XML output (`<binary> -o <file>,xml`), because direct runs and ctest -V produced no per-case stdout in this environment:

| Binary | Test functions (XML entries) | Passing incidents | Failing incidents |
| ------ | ---------------------------- | ----------------- | ----------------- |
| docx_content_test | 38 | 97 | 0 |
| document_import_test | 20 | 24 | 0 |
| zip_archive_test | 26 | 26 | 0 |

Other checks run by the verifier:
- Logging calls in `src/documents/` and `src/application/DocumentImport.cpp` (qDebug, qWarning, qInfo, qCritical, qFatal, std::cerr, printf): none.
- libzip error text used anywhere in `src/documents/` (zip_strerror, zip_error_strerror, errorString): none. Errors are codes only.
- Debt markers (TBD, FIXME, XXX, TODO, HACK, PLACEHOLDER) in the phase files: none.
- Invisible characters: the hook flagged `src/documents/DocxContent.cpp` (line 1041, U+2011; line 1043, U+00AD) and `tests/DocxContentTest.cpp` (lines 1218, 1221). These are the intended mappings of `w:noBreakHyphen` and `w:softHyphen`. The test expects the same code points, so this is not a hidden payload. The soft hyphen is still an invisible character that now sits in the resume text (see WARNING 7).
- Probe scripts: the phase PLAN and SUMMARY files declare none, so Step 7c does not apply.
- No previous VERIFICATION.md exists, so this is initial mode.

## Goal Achievement: Roadmap Success Criteria

| # | Truth (ROADMAP SC) | Status | Evidence |
| - | ------------------ | ------ | -------- |
| SC1 | Paragraphs come out in document order; headings are recognised by style, including Russian style names; bullet and numbered lists keep nesting | VERIFIED (with WARNINGs 1 and 2) | `DocxContentTest::paragraphsKeepDocumentOrder`; `headingByStyleIdNameOrOutline` (rows include "Russian name Заголовок 2 under styleId a0"); `listsKeepKindAndLevel` (levels 0 to 2, ilvl above 8 clamped, numId 0 is no list); `numberedCountersRestartOnHigherLevel`; `styleNumPrLevelFromPStyle`. Code: `headingLevel` DocxContent.cpp:420-428, `applyList` 629-658. |
| SC2 | Tables read row by row; empty and merged cells keep rows intact; hyperlinks and insertions give plain text; deleted revisions do not appear; images are skipped without distorting surrounding text | VERIFIED at the model layer (with WARNING 4) | `tableWithOneRowKeepsCells`; `tableRowsKeepGridWidth` (5 shapes, every grid column covered exactly once); `vMergeNotDuplicated`; `emptyAndNestedTables`; `inlineWrappersAndTracking` (hyperlink, ins, del, moveFrom, sdt placeholder, fldSimple, complex field, smartTag); `drawingSkippedWithWarning` (text "до" + "после" = "допосле", one ImageSkipped). Code: `readCell` 773-803, `finishTable` 837-875, `skipUnknown` 962-983. |
| SC3 | Cyrillic in text and in the file name is read without corruption | VERIFIED | `DocxContentTest::cyrillicTextExact` (exact strings with ё, —); `DocumentImportTest::readsDocumentContentFromCyrillicDirectory` (directory "Резюме_ёлка", file "Резюме.docx", exact "Опыт работы" and "Ёлкин Пётр, менеджер"); `readsDocxFromCyrillicDirectory`. Synthetic fixtures only (see TEST-02 deferral). |
| SC4 | DTD and external entities are refused with an error; skipped constructs (text fields, headers and footers) are counted and returned as warnings, not lost silently | FAILED (BLOCKER on the "not lost silently" clause); DTD/XXE refusal VERIFIED | DTD refusal VERIFIED: `doctypeRejected` (internal subset, external SYSTEM entity, DOCTYPE in styles.xml, DOCTYPE in numbering.xml), `doctypeResultHasNoContent`, `DocumentImportTest::doctypeFileGivesUnsafeXmlTextNamingFileOnly`. Named constructs counted VERIFIED: `textBoxAndOtherSkippedCounted` (TextBoxSkipped, OtherSkipped), `headerAndFooterPartsCountedAsWarnings` (3 HeaderFooterSkipped), `headerFooterAndCommentsBytesNeverRead`. FAILED: `w:altChunk` and `w:subDoc` are dropped with no warning (see BLOCKER 1). |
| SC5 | Qt Test on synthetic fixtures, including malicious ones, passes; a test confirms the fixture marker text does not appear in error messages or logs | VERIFIED | ctest 6/6; 38 + 20 + 26 test functions with 0 failing incidents. Marker tests: `DocxContentTest::messagesDoNotCarryMarker`, `DocumentImportTest::messagesAndLogsCarryNoMarker` (5 planted inputs), `messagesLeakNeitherPathNorContent` (5 inputs). Malicious bombs and limits: `ZipArchiveTest::deflatedBombIsLimitedByRatio`, `tooManyEntriesIsLimit`, `duplicateEntryNameIsCorrupt`; `DocxContentTest::nodeLimitBoundary`, `nestingDepthLimit`, `textLimitBoundary`. Note: the layer has no logging calls, so the log half of the marker check is a guard that cannot trigger today; it is not an observed log. |

**Score:** 4/5 roadmap truths verified (SC4 failed on the silent-drop clause). 0 behavior-unverified truths.

## Requirements Coverage

| Requirement | Status | Evidence |
| ----------- | ------ | -------- |
| DOCX-02 paragraphs in document order | SATISFIED | `paragraphsKeepDocumentOrder`; `readsDocumentContentFromCyrillicDirectory` (through the file path) |
| DOCX-03 headings by style, including Russian names | SATISFIED (synthetic fixtures only; assumption A1) | `headingByStyleIdIsHeadingLevelOne`; `headingByStyleIdNameOrOutline` (Heading N, "heading N", "Заголовок N", outlineLvl, basedOn chain, cycles, depth bound); `DocumentImportTest::readsDocumentContentFromCyrillicDirectory` (headingLevel 1 via a "heading 1" name) |
| DOCX-04 bullet and numbered lists with nesting | SATISFIED (with WARNINGs 1 and 2) | `listsKeepKindAndLevel`; `numberedCountersRestartOnHigherLevel`; `styleNumPrLevelFromPStyle`; `unsupportedNumberingFormatCountedOncePerList`. Not tested through `readDocxFile` (WARNING 4). |
| DOCX-05 tables with rows, empty and merged cells | SATISFIED at the model layer (WARNING 4 and WARNING 5) | `tableRowsKeepGridWidth`, `vMergeNotDuplicated`, `emptyAndNestedTables`, `rowsInsideWrapperKeepCells`, `tableNodesCountedInLimit`, `paddingCellsCountedInNodeLimit` |
| DOCX-06 Cyrillic in text and paths | SATISFIED | `cyrillicTextExact`; `DocumentImportTest::readsDocxFromCyrillicDirectory`, `readsDocumentContentFromCyrillicDirectory` |
| DOCX-07 images skipped | SATISFIED | `drawingSkippedWithWarning`; `textBoxAndOtherSkippedCounted`. `w:pict` and `w:object` take the same branch in `skipUnknown` (line 969) but have no test of their own (INFO). |
| DOCX-08 hyperlinks and insertions as text; deleted revisions hidden | SATISFIED | `inlineWrappersAndTracking` (hyperlink, ins, moveTo nesting in `inlineNestingBoundedForEveryWrapper`, del, moveFrom, delText); `symbolElementsMappedOrCounted` (U+2011, U+00AD, tab) |
| SAFE-03 DTD and external entities refused | SATISFIED | `doctypeRejected` (3 parts), `doctypeFileGivesUnsafeXmlTextNamingFileOnly`; PartReader::next refuses the DTD token before any entity expansion (DocxContent.cpp:118-119) |
| SAFE-04 skipped content counted, not silently lost | NOT SATISFIED (BLOCKER 1; WARNING 6) | Counted and tested: images, text boxes, footnote and endnote references, symbols, unknown elements with w:t, headers, footers, comments, skipped-beyond-grid content. Silent: `w:altChunk`, `w:subDoc` (no rule, no test); vMerge continuation text (designed, untested for warnings). |
| SAFE-05 resume text not in logs or error messages | SATISFIED | `DocxError` and `DocxWarning` are enums with no text (DocxTypes.h); ZipArchive returns codes and never calls libzip error-text functions; `errorText` and `warningText` (DocumentImport.cpp:34-103) use the file name only; `messagesAndLogsCarryNoMarker`, `messagesLeakNeitherPathNorContent` |
| TEST-01 Qt Test on synthetic fixtures including malicious cases | SATISFIED | Fixtures built in code (`tests/DocxFixtures.h`, `wrapBody`, `partsWithBody`); ctest docx_content, document_import, zip_archive all pass with 0 failing incidents. Coverage gaps are listed as BLOCKER 1 and WARNINGs. |

## Findings

### BLOCKER 1: w:altChunk and w:subDoc are dropped silently (SAFE-04, SC4)

- **Where:** `src/documents/DocxContent.cpp`, `skipUnknown` (lines 962-983). The function gives a warning only for pictures, text boxes, footnote and endnote references, symbols, and elements that contain a `w:t`. An `altChunk` or `subDoc` element holds no `w:t`, so `skipSubtree` reports no facts and no warning is added.
- **Reach:** `altChunk` and `subDoc` can appear at body level and inside paragraphs. Both reach `skipUnknown` through `readChild` (lines 1128-1160).
- **Evidence:** a text search of `src` and `tests` for `altChunk`, `subDoc`, `contentPart` and `w:control` finds no match. Neither the 02-03 nor the 02-04 SUMMARY lists this gap. Static code reading only; the verifier did not execute a fixture for this case.
- **Why BLOCKER:** SAFE-04 is a must-have requirement for this phase. The phase goal says the model comes "с предупреждениями о пропущенном". Content that is skipped with no warning is the exact failure SAFE-04 forbids.
- **Fix (small):** count both elements as `OtherSkipped`, then add a test.

### WARNING 1: lvlOverride and startOverride are ignored (DOCX-04)

- `readNum` (DocxContent.cpp:516-535) reads only `w:abstractNumId`. A numbering restart or start value in `w:lvlOverride` or `w:startOverride` is ignored, so ordered-list markers can be wrong in Word files (bullets and nesting are unaffected). Known gap in 02-02-SUMMARY. No test covers it.

### WARNING 2: an undefined list level is shown as decimal with no warning (DOCX-04, SAFE-04 spirit)

- A level that the abstractNum does not define keeps the `NumLevel` defaults (decimal, start 1, not approximated; DocxContent.cpp:438-442 and 493). The marker is "N." and no `ListFormatApproximated` warning is raised. Known gap in 02-02-SUMMARY.

### WARNING 3: warning texts are checked against copies in the test, not against the code

- `DocumentImportTest::warningTextsArePairwiseDistinct` (lines 576-604) compares seven literal strings in the test with each other. Its comment (lines 577-578) says "a drift in either place is caught". That is false: nothing calls `DocumentImport::warningText`, so an edit to DocumentImport.cpp cannot fail this test.
- Exact-text checks against the implementation exist for the styles and numbering warnings (line 378-380), the HeaderFooterSkipped text (lines 227-230, 547-551), and the OtherSkipped text for comments (line 231-234). They are missing for `ImageSkipped`, `TextBoxSkipped`, and `ListFormatApproximated`. 02-02 and 02-03 SUMMARY both record this. The texts are owner-pending (OQ8).

### WARNING 4: lists, tables and hyperlinks are not tested through DocumentImport (SC1, SC2)

- `tests/DocumentImportTest.cpp` contains no `w:tbl`, `w:numPr` or `w:hyperlink`. `DocumentImport::readDocxFile` passes `readDocxContent` output through unchanged (DocumentImport.cpp:141-146), so the wiring looks direct, but the service-level path is not exercised. Known gap in 02-04-SUMMARY.

### WARNING 5: Russian style names and Word-saved files are not tested (DOCX-03, DOCX-06)

- Russian heading names (A1), the drawing and text-box readings (A6, A7, A12), and the numbering rules (A3, A4, A5, A13) are checked only on synthetic fixtures. A DOCX saved by Word is not tested; that is TEST-02 in Phase 3 (see deferred).

### WARNING 6: text inside a vMerge continuation cell is dropped silently (SAFE-04)

- `readCell` (DocxContent.cpp:790-795) returns without storing the continuation cell's blocks and without a warning. The 02-03-SUMMARY (line 83) records this as intended ("its content is not shown"). Word normally writes an empty paragraph in continuation cells, so the real-world impact is small. It still contradicts SAFE-04's literal wording. `vMergeNotDuplicated` checks that the text is absent but makes no assertion on `warnings`.
- **Owner decision needed:** accept the silent drop as a documented exception to SAFE-04, or count it as `OtherSkipped`.

### WARNING 7: soft hyphen and no-break hyphen enter the text as characters (DOCX-06 exactness)

- `w:softHyphen` becomes U+00AD and `w:noBreakHyphen` becomes U+2011 (DocxContent.cpp:1040-1043). The soft hyphen is invisible in Word unless it breaks a line, so a copy of the text can contain characters the user never typed. This is an owner decision (related to OQ11), not a defect in the mapping itself.

### INFO

- `readDocxContent` does not call `readToEnd` on `word/document.xml` (lines 1198-1201). Malformed markup after `</w:body>` (for example a missing `</w:document>`) imports successfully. The body is complete, so the content is not affected.
- `messagesDoNotCarryMarker` and `messagesAndLogsCarryNoMarker` check that captured messages lack the marker. The layer produces no messages, so these checks cannot fail today. The real protection is that the error types carry codes only.
- `isNumberedXmlPart` (DocxContainer.cpp:22-32) also accepts `word/header.xml` with no digits, because `all_of` on an empty range is true. The 02-04 SUMMARY records this as accepted.
- A `w:sdt` placeholder is recognised only if `w:showingPlcHdr` comes before `w:sdtContent` (02-03-SUMMARY). The schema order makes this safe for valid files.

## Key Link Verification

| From | To | Via | Status | Details |
| ---- | -- | --- | ------ | ------- |
| DocumentImport::readDocxFile | documents::readDocxParts | direct call, DocumentImport.cpp:135 | WIRED | Bytes come from a bounded QFile read (lines 121-132) |
| readDocxParts | ZipArchive::open and readEntry | DocxContainer.cpp:89, 101-112 | WIRED | Only word/document.xml, styles.xml and numbering.xml are read; headers, footers and comments are counted by name (137-142) |
| readDocxFile | readDocxContent | DocumentImport.cpp:141-146 | WIRED | Model and raw parts assigned to ImportedDocx; warnings converted to Russian texts (150-151) |
| readDocxContent | readStyles, readNumbering, body walk | DocxContent.cpp:1175-1199 | WIRED | Styles and numbering are read first, so the body walk can use them |
| readText | Paragraph.text | appendText, DocxContent.cpp:987-1026 | WIRED | Characters are kept as-is, with the length limit counted first |

## Artifacts (Levels 1-3)

| Artifact | Exists | Substantive | Wired | Status |
| -------- | ------ | ----------- | ----- | ------ |
| src/documents/DocxContent.cpp | yes | yes (1204 lines) | yes | VERIFIED, except the silent-drop gaps above |
| src/documents/DocxContainer.cpp | yes | yes | yes | VERIFIED |
| src/documents/DocxModel.h, DocxTypes.h | yes | yes | yes | VERIFIED |
| src/application/DocumentImport.cpp | yes | yes | yes | VERIFIED (warning-text gap, WARNING 3) |
| tests/DocxContentTest.cpp | yes | yes (1247 lines, 38 functions) | yes (docx_content in ctest) | VERIFIED |
| tests/DocumentImportTest.cpp | yes | yes (20 functions) | yes (document_import in ctest) | VERIFIED |
| tests/DocxFixtures.h | yes | yes (in-code fixtures) | yes | VERIFIED |

## Data-Flow Trace (Level 4)

| Artifact | Variable | Source | Produces real data | Status |
| -------- | -------- | ------ | ------------------ | ------ |
| DocxDocument.blocks | Paragraph.text | w:t character data from document.xml | Yes, tests assert exact strings | FLOWING |
| Paragraph.headingLevel | style chain from styles.xml | readStyles, then headingLevel | Yes, tests use style tables | FLOWING |
| Paragraph.listMarker | counters from numbering.xml | readNumbering, then applyList | Yes, tests assert "1.", "2.", bullet | FLOWING |
| ImportedDocx.warnings | Russian texts from warning codes | DocumentImport::warningText | Yes, tests assert several texts exactly | FLOWING (4 of 7 texts not exact-checked, WARNING 3) |

## Behavioral Spot-Checks (Step 7b)

The phase produces a library, not a runnable entry point, so the spot-checks are the named Qt Test runs above.

| Behavior | Command | Result | Status |
| -------- | ------- | ------ | ------ |
| Build is current | `cmake --build --preset debug` | exit 0, no work to do | PASS (not a clean build) |
| Full ctest set | `ctest --preset debug` | 6/6 passed, exit 0 | PASS |
| DOCX content model | `docx_content_test` (XML output) | 38 functions, 0 failing | PASS |
| Import and error texts | `document_import_test` (XML output) | 20 functions, 0 failing | PASS |
| ZIP limits | `zip_archive_test` (XML output) | 26 functions, 0 failing | PASS |
| Silent-drop check for altChunk | not executed (no fixture exists) | n/a | NOT RUN; static reading only (BLOCKER 1) |

## Human Verification and Owner Decisions

These do not change the status, which is already gaps_found, but the owner must decide them before Phase 3 relies on the model:

1. **vMerge continuation text (WARNING 6):** accept the silent drop as a documented exception to SAFE-04, or count it.
2. **Soft and no-break hyphens in text (WARNING 7):** keep U+00AD and U+2011 in the text, or map them differently (OQ11).
3. **Russian warning texts (WARNING 3, OQ8):** review the seven texts in the context of the UI before Phase 3 shows them.
4. **Owner-pending defaults:** OQ1 to OQ11 and A1 to A15 in 02-RESEARCH.md remain owner-pending. The verifier did not change or confirm them. The MB reading as MiB (D-01, D-02) is one of them.

## Gaps Summary

One BLOCKER: `w:altChunk` and `w:subDoc` are dropped with no warning, so SAFE-04 and the SC4 clause "не теряются молча" are not met for those constructs. The fix is small and contained in `skipUnknown`, plus one test. Phase 3 should not start until this is closed or the owner accepts it explicitly.

Warnings: lvlOverride and undefined list levels (numbering fidelity), the untested warning texts and the misleading `warningTextsArePairwiseDistinct` comment, no service-level table and list tests, the vMerge continuation policy, the invisible hyphen characters, and the Word-saved file gap (deferred to Phase 3, TEST-02).

ROADMAP.md and REQUIREMENTS.md still show Phase 2 as in progress and all requirements as Pending. The verifier did not edit them, as instructed.

---

_Verified: 2026-10-25T02:53:38Z_
_Verifier: Claude (gsd-verifier)_
