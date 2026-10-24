---
phase: 01-zavisimost-libzip-i-bezopasnoe-chtenie-zip
verified: 2026-10-24T11:26:20Z
status: human_needed
score: 5/6 truths verified (1 uncertain: SAFE-01 draft half)
behavior_unverified: 0
overrides_applied: 0
covered_files: [".github/workflows/ci.yml", "CMakeLists.txt", "cmake/DeployMinGWRuntime.cmake", "src/application/DocumentImport.cpp", "src/application/DocumentImport.h", "src/documents/DocxContainer.cpp", "src/documents/DocxContainer.h", "src/documents/DocxTypes.h", "src/documents/ZipArchive.cpp", "src/documents/ZipArchive.h", "tests/DocumentImportTest.cpp", "tests/DocxFixtures.h", "tests/ZipArchiveTest.cpp", ".planning/phases/01-zavisimost-libzip-i-bezopasnoe-chtenie-zip/01-01-PLAN.md", ".planning/phases/01-zavisimost-libzip-i-bezopasnoe-chtenie-zip/01-01-SUMMARY.md", ".planning/phases/01-zavisimost-libzip-i-bezopasnoe-chtenie-zip/01-02-PLAN.md", ".planning/phases/01-zavisimost-libzip-i-bezopasnoe-chtenie-zip/01-02-SUMMARY.md", ".planning/phases/01-zavisimost-libzip-i-bezopasnoe-chtenie-zip/01-03-PLAN.md", ".planning/phases/01-zavisimost-libzip-i-bezopasnoe-chtenie-zip/01-03-SUMMARY.md", ".planning/phases/01-zavisimost-libzip-i-bezopasnoe-chtenie-zip/01-04-PLAN.md", ".planning/phases/01-zavisimost-libzip-i-bezopasnoe-chtenie-zip/01-04-SUMMARY.md"]
covered_digest: "v3:sha256:f7f8c2cc773063ba10141618d2acc42c91950adcb7e3a83b61a0c76ba15595cb"
coincidental_reliance_items:
  - truth: "duplicateEntryNameIsCorrupt: a duplicate entry name yields DocxError::Corrupt"
    reason: undeclared-precondition
    harden: "The Corrupt result holds only because libzip 1.11.4 refuses a duplicate name at zip_open_from_source (ZIP_ER_EXISTS, code 10). ZipArchive.cpp:46-47 is never the code that fires for this input. Either remove the redundant check and document the libzip precondition, or add a test that reaches the in-code check."
deferred:
  - truth: "SAFE-01 draft half: an import failure leaves an open draft unchanged"
    addressed_in: "Phase 3"
    evidence: "ROADMAP Phase 3 success criterion 2: 'Если в окне есть несохранённые правки, перед заменой появляется запрос подтверждения; отказ оставляет черновик без изменений'. Phase 1 has no draft code; readDocxFile takes only a path (DocumentImport.h:22)."
human_verification:
  - test: "Open a real Office-encrypted .docx and a real legacy .doc renamed to .docx through the import layer (or a test harness that calls DocumentImport::readDocxFile)."
    expected: "Encrypted text ('Файл «…» зашифрован…') for the encrypted file and LegacyDoc text ('…в старом формате .doc…') for the legacy file, with no document content in the message."
    why_human: "The test fixtures are synthetic (buildCfb writes the UTF-16LE name EncryptedPackage by hand). Real Office CFB layouts are not exercised, and whether the Russian wording is clear to a user is a judgment call."
  - test: "Run the GitHub Actions workflow (.github/workflows/ci.yml) on a branch PR to main."
    expected: "build-test matrix (debug and release) passes; the installer job's smoke step finds bin\\libzip.dll and the app starts."
    why_human: "CI was not run. The libzip install lines (ci.yml:45 and :77) were checked statically only, and no GitHub runner was available."
  - test: "Run the Inno Setup installer (build/mingw-release/installer/*.exe, or the CI artifact) silently on a machine without MSYS2 and launch the app."
    expected: "Installed app starts without a missing-DLL error. Its libzip.dll and transitive DLLs are present."
    why_human: "Only the cmake --install folder was checked locally. The Inno Setup installer and a clean machine were not."
  - test: "Owner confirmation of the planner and owner-pending values: kMaxEntries 1000, kMaxCompressionRatio 100:1 after 1 MiB, one 20 MiB budget shared across the three parts (D-02 reading), 'MB' read as MiB, and the Russian texts."
    expected: "Owner accepts or changes each value. A change requires updating DocxTypes.h, the README limits table, and the tests that hard-code 1001, 999 and 2 MiB."
    why_human: "These are product decisions. They are recorded as owner-pending in the phase memory and the phase brief; they are not code defects."
---

# Phase 1: Зависимость libzip и безопасное чтение ZIP — Verification Report

**Phase Goal:** DOCX-файл можно безопасно открыть как ZIP-контейнер, а libzip собирается на ПК разработчика, в CI и в развёрнутой папке приложения
**Verified:** 2026-10-24T11:26:20Z
**Status:** human_needed
**Re-verification:** No (initial verification; no prior 01-VERIFICATION.md existed)

Verification ran against the working tree at HEAD `9a4398d` (branch `chore/map-codebase`). Phase files match HEAD (`git diff HEAD` empty for every phase file). SUMMARY.md claims were treated as leads and checked against the code and the test binaries.

## Goal Achievement

### Observable Truths

| #   | Truth | Status | Evidence |
| --- | ----- | ------ | -------- |
| 1 | SC1: project with libzip builds and passes ctest from zero; CI install lists contain libzip | ✓ VERIFIED | Fresh configure and build in a new directory (`cmake -S . -B …/fresh-debug -G Ninja`, compiler and prefix as in `CMakePresets.json`): configure exit 0, `cmake --build` exit 0, 47/47 steps, 0 warning lines. `ctest --test-dir …/fresh-debug` exit 0, 100% tests passed, 5/5. Incremental `cmake --build --preset debug` exit 0 (`ninja: no work to do`), `ctest --preset debug` exit 0, 5/5. CI: `.github/workflows/ci.yml:45` (build-test) and `:77` (installer) both list `mingw-w64-x86_64-libzip`. CI itself not run (see human items). |
| 2 | SC2: `cmake --install` into a clean folder runs without missing-DLL errors; libzip and transitive DLLs deployed | ✓ VERIFIED | `cmake --install <fresh-debug> --prefix <scratch>/install-clean` exit 0. `bin/libzip.dll` present (168384 bytes). `objdump -p libzip.dll` imports: `libbz2-1.dll`, `liblzma-5.dll`, `zlib1.dll`, `libzstd.dll` (all present in installed `bin`), plus system DLLs. Every import of every installed DLL and the exe resolves in `bin` or System32 except three `api-ms-win-*` API-set stubs, which Windows resolves without files. `AIResumeAssistant.exe` run with PATH=System32 only under `/usr/bin/timeout 10`: exit 124 (still running at timeout, so no DLL-load failure). The exe does not import libzip itself (`objdump` count 0), so the libzip check rests on the static import resolution above. |
| 3 | SC3: non-ZIP, corrupt or truncated ZIP, encrypted DOCX, and `.doc` renamed `.docx` each give their own clear error, with no file content and no disk write | ✓ VERIFIED | Code: `src/application/DocumentImport.cpp:33-60` (one Russian text per `DocxError`, no default branch, file name only); classification `src/documents/DocxContainer.cpp:53-61` (CFB → Encrypted or LegacyDoc before libzip; non-ZIP → NotZip); `ZipArchive.cpp:76-78` (open failure → Corrupt). Tests (`./document_import_test.exe`, QTest Totals 17 passed, 0 failed): `eachFailureHasItsOwnExactText` for NotZip, Corrupt, Encrypted, LegacyDoc, MissingPart (all five PASS); `failureTextsArePairwiseDistinct` PASS; `messagesLeakNeitherPathNorContent` PASS (content marker and directory absent from every message); `importWritesNothing` PASS (directory snapshot identical across every call). Container-level: `zip_archive_test.exe` Totals 26 passed, 0 failed, including `cfbWithEncryptedPackageIsEncrypted`, `cfbWithoutEncryptedPackageIsLegacyDoc`, `libzipEncryptedEntryIsEncrypted` (real libzip AES-256 entry), `truncatedAndGarbageInputsAreCorrupt`, `plainTextIsNotZip`, `zeroByteInputIsNotZip`, `damagedDocumentPartRefusesPartialResult`. Real Office files not tested (see human items). |
| 4 | SC4: malicious archives (ratio, huge write, too many entries) rejected by limits without unbounded memory growth; the needed parts read only by exact entry name | ✓ VERIFIED | Ratio: `ZipArchive.cpp:163-167`; tests `deflatedBombIsLimitedByRatio` PASS, `storedLargeEntryIsNotRatioLimited` PASS, `bombFromCyrillicDirectoryIsLimitWithFileNameOnly` PASS (17/17 document_import). Entry count: `ZipArchive.cpp:37-38` (before any name is read), tests `tooManyEntriesIsLimit` PASS (1001 → Limit), `exactlyMaxEntriesIsAccepted` PASS. Budget: `ZipArchive.cpp:153-156` (refuses before append), `DocxContainer.cpp:76-82` (remaining decremented by bytes returned), tests `readEntryEnforcesByteBudget`, `budgetBoundaryIsExact`, `sharedBudgetAcrossParts` PASS. Memory bound: file read capped at `kMaxFileBytes + 1` (`DocumentImport.cpp:98`), chunk 64 KiB (`ZipArchive.cpp:14`), output never exceeds `maxBytes`. Exact-name: `ZipArchive.cpp:116` (`zip_name_locate`, flags 0, case-sensitive), NUL rejected `:111`; tests `exactNameMissingIsMissingPart`, `caseAndSuffixVariantsAreNotMatched` PASS. Memory bound is structural, not measured. |
| 5 | SAFE-01 (error half): non-ZIP, damaged, encrypted, `.doc` each give a clear error and leave the draft unchanged | ? UNCERTAIN (WARNING) | Error half VERIFIED under truth 3. Draft half: `readDocxFile` takes only a path (`src/application/DocumentImport.h:22`), so the layer cannot touch a draft, and no draft code exists in Phase 1. The UI-level assertion (draft preserved when a confirmed replace fails) belongs to Phase 3 (deferred). REQUIREMENTS.md maps SAFE-01 to Phase 1, so the owner must decide whether it closes here. |
| 6 | SAFE-02: zip-bomb defence through decompression limits | ✓ VERIFIED | Same evidence as truth 4. The limits are the planner's values, owner-pending (see human items). |

**Score:** 5/6 truths verified (truth 5 uncertain, routed to human decision and Phase 3)

### Success Criteria (ROADMAP Phase 1)

| SC | Criterion | Status | Evidence |
| -- | --------- | ------ | -------- |
| 1 | Builds and `ctest --preset debug` from zero; CI install lists contain libzip | ✓ VERIFIED (CI run pending) | Truth 1 |
| 2 | `cmake --install` clean folder starts without missing DLL errors | ✓ VERIFIED (installer pending) | Truth 2 |
| 3 | Four error kinds, each with its own clear message, no content, no disk write | ✓ VERIFIED (real-file samples pending) | Truth 3 |
| 4 | Zip-bomb limits; memory bounded; exact-name reads | ✓ VERIFIED | Truth 4 |

### Requirements Coverage

| Requirement | Plan | Description | Status | Evidence |
| ----------- | ---- | ----------- | ------ | -------- |
| SAFE-01 | 01-02, 01-03 | Non-ZIP, damaged, encrypted, `.doc` give a clear error; draft unchanged | ? UNCERTAIN (WARNING) | Error half verified (truth 3). Draft half is structural at this layer; UI-level proof is Phase 3 (deferred). Owner decision needed on whether SAFE-01 closes in Phase 1. |
| SAFE-02 | 01-01, 01-04 | Zip-bomb defence through decompression limits | ✓ SATISFIED | Truth 4 and 6. |

No orphaned requirements: REQUIREMENTS.md maps only SAFE-01 and SAFE-02 to Phase 1.

### Required Artifacts (all levels: exists, substantive, wired)

| Artifact | Exists | Substantive | Wired | Status |
| -------- | ------ | ----------- | ----- | ------ |
| `src/documents/DocxTypes.h` | ✓ | ✓ enum codes and limit constants (lines 10-48) | ✓ used by container, import, tests | ✓ VERIFIED |
| `src/documents/ZipArchive.h/.cpp` | ✓ | ✓ open, hasEntry, readEntry, checks (lines 33-174) | ✓ `DocxContainer.cpp:66`, tests | ✓ VERIFIED |
| `src/documents/DocxContainer.h/.cpp` | ✓ | ✓ classification and three-part read | ✓ `DocumentImport.cpp:107`, tests | ✓ VERIFIED |
| `src/application/DocumentImport.h/.cpp` | ✓ | ✓ file checks, Russian texts | ✓ `CMakeLists.txt:16`, tests | ✓ VERIFIED |
| `tests/ZipArchiveTest.cpp` | ✓ | ✓ 26 test functions | ✓ `CMakeLists.txt:105-107` | ✓ VERIFIED |
| `tests/DocumentImportTest.cpp` | ✓ | ✓ 17 test functions | ✓ `CMakeLists.txt:109-111` | ✓ VERIFIED |
| `tests/DocxFixtures.h` | ✓ | ✓ stored ZIP, CFB, libzip writer with AES | ✓ both test files | ✓ VERIFIED |
| `CMakeLists.txt` | ✓ | ✓ `find_package(libzip)` (line 9), `resume_documents` linked to libzip::zip (line 32), install `IMPORTED_RUNTIME_ARTIFACTS` (line 56) | ✓ configure and install succeeded | ✓ VERIFIED |
| `.github/workflows/ci.yml` | ✓ | ✓ libzip in both install blocks, smoke check line 104 | not executed | ⚠️ WIRED-STATIC (CI run pending) |
| `cmake/DeployMinGWRuntime.cmake` | ✓ | ✓ explicit `libzip.dll` scan (lines 24-29) | ✓ install transitive DLLs verified | ✓ VERIFIED |

### Key Link Verification

| From | To | Via | Status | Details |
| ---- | -- | --- | ------ | ------- |
| `ZipArchive.cpp` | libzip buffer source | `zip_source_buffer_create` freep 0, `zip_open_from_source` (lines 68-76) | ✓ WIRED | Build and tests use it. |
| `CMakeLists.txt` | `libzip::zip` | `target_link_libraries(resume_documents PUBLIC …)` (line 32) | ✓ WIRED | Fresh configure resolved the target. |
| `CMakeLists.txt` install | `bin/libzip.dll` | `install(IMPORTED_RUNTIME_ARTIFACTS …)` (line 56) | ✓ WIRED | `bin/libzip.dll` present in clean install. |
| `DocumentImport.cpp` | `documents::readDocxParts` | `DocumentImport.cpp:107` | ✓ WIRED | Tests traverse the path. |
| `DocxContainer.cpp` | `ZipArchive::readEntry` | `readPart` lambda, remaining budget (lines 76-82) | ✓ WIRED | `sharedBudgetAcrossParts` PASS. |

### Data-Flow Trace (Level 4)

Not applicable. Phase 1 renders no data; DocumentImport returns a value to a caller that does not exist until Phase 3.

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
| -------- | ------- | ------ | ------ |
| Full suite on the fresh build | `ctest --test-dir <scratch>/fresh-debug --output-on-failure` | 100% passed, 5/5, exit 0 | ✓ PASS |
| Full suite on the preset tree | `ctest --preset debug` (MSYS2 PATH) | 100% passed, 5/5, exit 0 | ✓ PASS |
| zip_archive per-function | `zip_archive_test.exe -v2 -o <file>,txt` | Totals 26 passed, 0 failed; each function PASS | ✓ PASS |
| document_import per-function | `document_import_test.exe -v2 -o <file>,txt` | Totals 17 passed, 0 failed; each function PASS | ✓ PASS |
| libzip refuses duplicate names | standalone probe (`dupcheck.exe`, scratch) calling `zip_open_from_source` on a two-entry archive | `zip_open_from_source REFUSED duplicate archive, zip code=10` | ✓ PROBE (see W1) |
| Installed app starts | `env -i PATH=System32 /usr/bin/timeout 10 bin/AIResumeAssistant.exe` | exit 124 (still running) | ✓ PASS |

Note: the stdout of QTest binaries is empty without `-o`; per-function results came from QTest file output. Test results above are from those files and the ctest runs, not from SUMMARY claims.

### Probe Execution

No probe scripts (`scripts/*/tests/probe-*.sh`) are declared for this phase. Skipped.

### Requirements and Mutation Evidence

The journal (`docs/memory.md`, 01-04 entry) reports mutation runs: disabling the entry-count check failed `tooManyEntriesIsLimit`; disabling the ratio check failed `deflatedBombIsLimitedByRatio` and `bombFromCyrillicDirectoryIsLimitWithFileNameOnly`; disabling the duplicate check did NOT fail `duplicateEntryNameIsCorrupt`. This verification did not re-run the mutations (the source must not be modified), so the mutation results are journal evidence. The duplicate-check result matches the standalone libzip probe above.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
| ---- | ---- | ------- | -------- | ------ |
| `src/documents/ZipArchive.cpp` | 46-47 | Duplicate-name check unreachable for this input (libzip refuses first) | ⚠️ Warning (W1) | Behavior correct; the code path is untested. |
| `src/documents/ZipArchive.cpp` | 73-78 | Failed `zip_open_from_source` does not free the source (branch c) | ℹ️ Info | Small leak per refused archive. Owner decided branch (c). Every duplicate-name archive takes this path. |
| `tests/DocumentImportTest.cpp` | 301-314 | NTFS settle loop before the no-write snapshot | ⚠️ Warning (W2) | Test-only workaround, capped at 50 × 100 ms. It does not weaken the comparison of calls under test. Watch for flakiness. |
| `src/documents/ZipArchive.cpp` | 164 | Integer-division ratio check allows up to 100 + 99/base | ℹ️ Info | Negligible for any realistic declared size. |
| `src/documents/README.md` | limits table | Limit values flagged owner-pending | ℹ️ Info | Documentation is honest about status. |

No TBD, FIXME, XXX, TODO, HACK, or placeholder markers in any phase file. No stub bodies (`return {}` at `ZipArchive.cpp:49` and `DocumentImport.cpp:59,74` are the success return or unreachable switch fall-through, not stubs). The documents API takes entry names and bytes; it has no file-path parameter (`ZipArchive.h`, `DocxContainer.h`).

## Gaps and Warnings

No blocker (FAILED truth). The phase goal is achieved in the code and tests. Items below are non-blocking.

- **W1 (WARNING): duplicate-name check is not isolated.** `ZipArchive.cpp:46-47` never fires for a duplicate name, because libzip 1.11.4 refuses it at `zip_open_from_source` (ZIP_ER_EXISTS, code 10; confirmed by probe). `duplicateEntryNameIsCorrupt` passes through the open-failure path, so the behavior is correct but the in-code branch is dead for this input. Suggested fix: remove the redundant check and document the libzip precondition, or add a test that feeds libzip a source that accepts duplicates and then reaches line 47.
- **W2 (WARNING): NTFS settle loop.** `DocumentImportTest.cpp:301-314` waits up to 5 s for two identical directory listings before the baseline. Test-only; it does not hide writes made by the calls under test, but it is a flakiness risk to watch.
- **W3 (WARNING, SAFE-01): draft half.** No draft code exists in Phase 1. The UI-level draft-preservation check is Phase 3 (deferred, `ROADMAP` Phase 3 SC2). Owner decides whether SAFE-01 closes in Phase 1 or stays open until Phase 3.
- **I1 (INFO): tracking files not updated.** `REQUIREMENTS.md` still shows SAFE-01 and SAFE-02 as Pending, and `ROADMAP.md` shows Phase 1 as `0/4 Planned`. Update after the owner accepts this report.
- **I2 (INFO): offscreen run of the installed app does not start.** `qoffscreen.dll` is not deployed, so `QT_QPA_PLATFORM=offscreen` exits 127 in the installed copy. This is a Qt deployment issue, not libzip; it is recorded in `01-01-SUMMARY.md` (issue 3). Any verify command that uses offscreen against the installed copy must be changed.
- **I3 (INFO): 20 MiB budget at true scale is not tested end-to-end.** Budget mechanics are tested with small budgets. Memory boundedness is structural (code reading), not measured.

## Owner Decisions (accepted; not gaps)

Per the phase brief, these are treated as given: libzip 1.11.4-1 instead of planned 1.12.1-1 (`pacman -Q mingw-w64-x86_64-libzip` → `1.11.4-1`, re-checked); source ownership branch (c) after failed `zip_open_from_source`; D-02 read as one budget shared across three parts; "MB" read as MiB; limits 1000 entries, 100:1 after 1 MiB, 20 MiB budget (planner values, owner-pending); Russian texts and LGPL texts deferred to Phase 3.

## Human Verification Required

See frontmatter `human_verification`. Summary:

1. Real Office-encrypted `.docx` and real legacy `.doc` renamed `.docx`, through the import layer, with wording review.
2. GitHub Actions CI run (not executed here).
3. Inno Setup installer on a machine without MSYS2 (not executed here).
4. Owner confirmation of the owner-pending limits, MB reading, D-02 scope, and Russian texts.

## Verification Commands Run (real output)

- `cmake --build --preset debug` (MSYS2 PATH): exit 0, `ninja: no work to do`.
- `ctest --preset debug`: exit 0, `100% tests passed, 0 tests failed out of 5`; per test: resume_e2e, resume_json, zip_archive, document_import, web_api all Passed.
- Fresh configure `cmake -S . -B <scratch>/fresh-debug -G Ninja …`: exit 0. Fresh build: exit 0, 47/47, 0 warnings. Fresh ctest: exit 0, 5/5.
- `zip_archive_test.exe -v2 -o …,txt`: Totals 26 passed, 0 failed.
- `document_import_test.exe -v2 -o …,txt`: Totals 17 passed, 0 failed.
- `cmake --install <fresh-debug> --prefix <scratch>/install-clean`: exit 0.
- `objdump -p` import check over installed DLLs and exe: all real imports resolved.
- Installed exe, System32-only PATH, 10 s: exit 124.
- Standalone libzip duplicate probe: `REFUSED duplicate archive, zip code=10`.
- `gsd-tools query verification.fingerprint …`: covered_digest `v3:sha256:f7f8c2cc773063ba10141618d2acc42c91950adcb7e3a83b61a0c76ba15595cb`.

Not run: GitHub Actions CI, Inno Setup installer, clean machine, release preset, `web_e2e` (no `tests/e2e/node_modules`), mutation re-runs (journal evidence only), clang-format and clang-tidy re-runs.

---

_Verified: 2026-10-24T11:26:20Z_
_Verifier: Claude (gsd-verifier)_
