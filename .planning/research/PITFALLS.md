# Pitfalls Research

**Domain:** Windows Qt 6 desktop resume tool (C++23, MinGW-w64, SQLite) with a loopback web UI
**Milestone:** Read .docx files and show their text in a Qt window; opening replaces the current draft without prompting
**Researched:** 2026-10-24
**Confidence:** HIGH for codebase facts (read from source). MEDIUM for Qt/POI behavior (checked against docs, not run). LOW for numeric limits (proposals to tune).

Phases are provisional until the roadmap exists:
- **P1** DOCX reader core (pure function over `QByteArray`, no UI)
- **P2** Qt import UI and draft replacement
- **P3** Dependencies, CI, installer

## Codebase facts that shape these pitfalls

- `MainWindow` has no dirty flag and no `closeEvent`. `newResume()` and `openResume()` assign `resume_` directly.
- `selectStage` is connected to `currentRowChanged` and overwrites the editor fields without calling `updateSelectedStage`. Unapplied edits are lost on stage switch. This existing bug is not in CONCERNS.md.
- Saving is explicit, so the SQLite copy survives a replace. Only unsaved edits are at risk.
- Field limits in `src/application/ResumeJson.cpp`: 200 chars short fields, 10000 description, 200 stages. The web path validates them; the Qt save path appears not to (verify).
- `src/documents/` is a README-only placeholder. Linked modules: Core, Sql, Network, HttpServer, Widgets, Test. QtXml is not linked.
- `reportError` shows `QSqlError::text()` directly (CONCERNS.md). Import errors must not reuse it.
- Web server, repository, and UI all run on the GUI thread.

## Critical Pitfalls

### 1. Zip bomb (decompression bomb)
- **What goes wrong:** A small crafted .docx expands to gigabytes or holds tens of thousands of entries. The app freezes or runs out of memory.
- **Why it happens:** Zip headers declare sizes the attacker controls. Readers that allocate from declared sizes, or inflate without an output cap, are exploitable.
- **Prevention:**
  - Check on-disk size first (`QFileInfo::size()`), e.g. reject above 20 MB.
  - Cap entry count (~1000), total output (~64 MB), per-entry output (~32 MB).
  - Enforce caps on bytes actually produced during inflate, never on declared sizes.
  - Ratio check (~1:100) only above a grace size (~1 MB). Apache POI's `ZipSecureFile` uses a 0.01 ratio and has documented false positives on repetitive files, so treat ratio as secondary.
  - Inflate only `word/document.xml`.
- **Warning signs:** Memory climbs during open; small file takes over 1 s; entry counts in the thousands.
- **Phase:** P1.

### 2. XXE and entity expansion
- **What goes wrong:** A `<!DOCTYPE>` with an external entity (`file:///C:/Users/...`, `http://...`) pulls local data into displayed text, or a billion-laughs chain exhausts memory.
- **Why it happens:** Many XML wrappers resolve DTDs and entities by default. OOXML never needs a DTD.
- **Prevention:**
  - Use `QXmlStreamReader` (QtCore, no new dependency). Its entity resolver defaults to returning null, which yields an error. Confirm with a test on the target Qt version.
  - Reject any DTD token (`QXmlStreamReader::DTD`) and any `<!DOCTYPE` or `<!ENTITY` in raw bytes.
  - Set `entityExpansionLimit` as a second layer. Resolved entities do not count toward it, so do not add a custom resolver that expands anything.
  - Cap extracted text length (~200k chars) and nesting depth.
  - Do not add libxml2 or QtXml for this milestone.
- **Warning signs:** `DOCTYPE` in any fixture; a new XML dependency in the diff.
- **Tests:** External entity pointing to a sentinel file (must not be read; must fail). Billion-laughs fixture (must fail fast).
- **Phase:** P1.

### 3. Path traversal inside the zip
- **What goes wrong:** Entry names like `../../x`, absolute paths, backslash paths, or duplicate names. Extraction writes outside the intended directory, or the wrong `document.xml` is used.
- **Prevention:**
  - Do not extract to disk. Parse in memory, which removes the write path entirely.
  - Look up `word/document.xml` by exact name. Reject names with `..`, leading `/` or `\`, or a drive letter.
  - Reject duplicate entry names instead of picking one.
  - Require `[Content_Types].xml` and `word/document.xml`; otherwise report "not a .docx".
- **Warning signs:** `QFile` opened for write, `mkpath`, or `QDir` joins in import code.
- **Phase:** P1 (tests), P2 (review gate).

### 4. Cyrillic, UTF-8, and encoding
- **What goes wrong:** Mojibake in the window; files with Cyrillic user paths fail to open; decomposed letters (NFD) fail to match in search.
- **Why it happens:** Windows profiles often have Cyrillic names (`C:\Users\Пётр\...`). MinGW narrow-path APIs use the ANSI code page. Word can store `й` as base letter plus combining mark.
- **Prevention:**
  - Let `QXmlStreamReader` decode each part (it honors the XML declaration, including UTF-16). Never use `fromLatin1` or `toLocal8Bit()`.
  - Pass paths as `QString` to `QFile`. Never pass `toStdString()` or `char*` to `fopen`, `std::ifstream`, or a third-party zip library.
  - Normalize output to NFC (`QString::normalized(QString::NormalizationForm_C)`).
  - Map OOXML structure deliberately: `w:p` to paragraph break, `w:br` to newline, `w:tab` to `\t`, `w:tbl` cells to tab-separated lines, `w:softHyphen` removed.
  - Test with Cyrillic fixtures. Do not print test data to the console (code-page garbling).
  - Docx structure traps: tracked changes (skip `w:del`, keep `w:ins`; recommended); text boxes duplicate text in `mc:Choice` and `mc:Fallback` (take one); legacy `.doc` or encrypted files fail as "unsupported" (check the zip signature and `[Content_Types].xml`); headers and footers are out of scope.
- **Warning signs:** `?` or `Ð`/`Ñ` sequences in the window; tests pass in CI but fail on the dev PC with a Cyrillic profile path.
- **Phase:** P1 (decoding, fixtures), P2 (paths in the file dialog).

### 5. UI freeze on large or slow input
- **What goes wrong:** Inflate, parse, and `setPlainText` on the GUI thread freeze the window and the loopback web server for large files.
- **Prevention:**
  - Check size first, read bytes on the UI thread, then unzip and parse in a worker (`QtConcurrent::run` or `QThread` with a QObject worker). The worker takes a `QByteArray` and returns a `QString` or a sanitized error. It never touches widgets, `ResumeRepository`, or `WebServer`.
  - Deliver results via queued signals with a generation counter, so a stale result cannot overwrite a newer open.
  - Disable Open and Import while busy; show a busy indicator; support cancel via an atomic flag.
  - Call `setPlainText` once on the final string, using `QPlainTextEdit`.
  - Make the P1 API a pure function so the threading decision is not retrofitted.
- **Warning signs:** `processEvents()` inside the parser; widget pointers captured in worker lambdas; repaint gaps.
- **Phase:** P1 (API shape), P2 (worker and UI).

### 6. Data loss on replace without prompting
- **What goes wrong:** Opening or importing replaces `resume_`; unsaved edits vanish. Closing the window also loses them silently.
- **Prevention:**
  - Add a `dirty_` flag set on every edit, compared against the last-saved snapshot (`ResumeJson::toJson`).
  - **Requirement conflict to confirm:** the milestone says "no prompt". A compatible mitigation is one-level "Undo replace" plus a status message such as "Draft replaced (not saved). Undo". Prompting on window close when dirty is recommended regardless.
  - Imported text is candidate facts from a file. Silently overwriting them conflicts with AGENTS.md (facts kept separate from AI suggestions). Consider an explicit preview-then-apply step.
  - Never write imported text to SQLite automatically. Label the state "Imported, not saved".
  - Fix the `selectStage` overwrite in the same milestone.
- **Warning signs:** No `dirty_` or `closeEvent` in the diff; a test that edits, opens another file, and finds the edit gone.
- **Phase:** P2 (decide before coding; record in `docs/memory.md`).

### 7. MinGW build, linking, and packaging
- **What goes wrong:** A new zip or zlib dependency works on the dev PC but breaks CI, or the installed app fails with "DLL not found".
- **Why it happens:** CI installs MSYS2 packages from explicit lists; the installer deploys DLLs via `GET_RUNTIME_DEPENDENCIES`.
- **Prevention:**
  - Do not depend on `QZipReader`. It is in the private header `qzipreader_p.h`, which Qt says may change or disappear, and a May 2026 Qt development-list thread debates removing or promoting it.
  - Prefer a small in-house reader over zlib raw inflate (`inflateInit2` with `-MAX_WBITS`) so caps apply during output. Use the MSYS2 `zlib` package and `find_package(ZLIB)`; check that the `CMakePresets.json` prefix resolves it on `C:/msys64/mingw64`.
  - Add any package to both `install:` lists in `.github/workflows/ci.yml` (build-test and installer jobs) and to local setup notes.
  - After `cmake --install`, run the installed `.exe` from a clean folder, not only the build tree.
  - Keep `CMAKE_CXX_STANDARD 23`. Do not lower it for an older GCC.
- **Warning signs:** CI red only on `windows-latest`; "DLL not found" on first launch of the installed build.
- **Phase:** P3, merged before or with P1.

### 8. Privacy: logging and error text
- **What goes wrong:** Resume text (names, employers, periods) reaches `qDebug`, dialogs, the status bar, test output, CI logs, or crash reports.
- **Why it happens:** Qt emits `qDebug` in release builds too; `QCOMPARE` prints actual values on failure.
- **Prevention:**
  - Never log parsed text, XML snippets, or `QXmlStreamReader::text()`. Log only error category and line or byte offset.
  - Show the base file name in dialogs, not the full path (it contains the Windows user name).
  - Use a sanitized import error type, not raw `reportError` text.
  - Fixtures must be synthetic (for example "Иван Тестов", "ООО Пример"). Never commit a real .docx (AGENTS.md).
  - Parse in memory; write no temp files; do not persist import paths in `QSettings` unless needed.
- **Warning signs:** `qDebug() <<` near parser code; fixtures named after real people; `.docx` files under `tests/`.
- **Phase:** P1 (error type, fixtures), P2 (UI messages); check in every review.

## "Looks Done But Isn't" Checklist

- [ ] **Zip caps:** Generated bomb aborts under cap with bounded memory.
- [ ] **XXE:** External-entity fixture is rejected and the sentinel file is never read.
- [ ] **Traversal:** Fixtures with `../`, absolute, backslash, and duplicate names are rejected.
- [ ] **Cyrillic:** Round-trip test passes; open works from a Cyrillic-named folder on the dev PC.
- [ ] **Replace:** Edit, open or import, confirm the agreed behavior (prompt or undo).
- [ ] **Row switch:** Typed text survives a stage switch (existing bug).
- [ ] **Responsiveness:** A 10 MB file keeps the window repainting; stale results are ignored.
- [ ] **CI and installer:** New package in both jobs; installed app launches from a clean folder.
- [ ] **Logs:** `grep` for `qDebug|qWarning|qCritical` in import code finds no text output.
- [ ] **Verification:** `cmake --build --preset debug` and `ctest --preset debug` actually run before any "passed" claim.

## Pitfall-to-Phase Mapping

| Pitfall | Prevention phase | Verification |
|---------|------------------|--------------|
| Zip bomb | P1 | Generated bomb test: aborts under cap, memory bounded |
| XXE / entity expansion | P1 | DTD rejected; no file I/O in fixture test |
| Path traversal | P1 | Unsafe-name fixtures rejected |
| Encoding and Cyrillic | P1 (decode), P2 (paths) | Cyrillic round-trip; NFD normalized |
| UI freeze | P1 (API), P2 (worker) | 10 MB manual test; no off-thread widget access |
| Data loss on replace | P2 | Decision recorded; dirty-flag and row-switch tests |
| MinGW, CI, DLL | P3 | CI green in both jobs; clean-folder launch |
| Privacy | P1, P2, each review | `grep` clean; synthetic fixtures only |

**Recovery notes:** Lost edits: restore from one-level undo if present, otherwise only the last SQLite save. Real resume committed: remove it, and decide with the owner on history rewrite before any push.

## Open Questions for the User

1. Replace without prompt: keep as written with undo and close-prompt mitigations, or require a prompt when the draft is dirty?
2. Tracked changes: show accepted text only (recommended) or both?
3. Are the proposed limits (20 MB file, 64 MB output, 200k chars) acceptable?

## Sources

- Codebase: `.planning/codebase/CONCERNS.md`, `src/ui/MainWindow.cpp`, `src/application/ResumeJson.cpp`, `src/web/WebServer.cpp`, `CMakeLists.txt`, `.github/workflows/ci.yml`, `cmake/DeployMinGWRuntime.cmake`, `AGENTS.md`.
- Qt docs, `QXmlStreamEntityResolver`: https://doc.qt.io/qt-6/qxmlstreamentityresolver.html
- Qt source, `qzipreader_p.h` (private): https://codebrowser.dev/qt6/qtbase/src/corelib/io/qzipreader_p.h.html
- Qt development list, May 2026 (QZipReader public or removed): https://lists.qt-project.org/pipermail/development/2026-May/047182.html
- Apache POI `ZipSecureFile` Javadoc: https://poi.apache.org/apidocs/org/apache/poi/openxml4j/util/ZipSecureFile.html
- Unconfirmed: QXmlStreamReader's handling of external DTD subsets, and exact entity-limit semantics. Verify with a test on the target Qt. Numeric limits are proposals, not sourced values.

---
*Pitfalls research for: Qt 6 DOCX import in a local resume desktop app*
*Researched: 2026-10-24*
