# Architecture Research

**Domain:** Local Windows desktop resume tool (C++23, Qt 6), DOCX import milestone
**Researched:** 2026-10-24
**Confidence:** HIGH for layering and Qt API choices (checked against repo sources and Qt 6 docs); MEDIUM for the libzip packaging path (package name found in search, not yet confirmed with `pacman`)

## Standard Architecture

### Layer Flow for the Import Milestone

```
UI (src/ui/MainWindow)           menu "Импорт DOCX…", read-only text view
   │ readDocumentFile(path)       no XML, no zip, no parsing in QWidget
   ▼
Application (src/application/DocumentImport)
   │ file bytes, size cap, dispatch by extension
   ▼
Documents (src/documents/)       pure C++ + QtCore; no Widgets, Sql, HttpServer
   │ ZipArchive (libzip) -> DocxReader (QXmlStreamReader) -> Document model
   ▼
Bytes in, structured Document out. Nothing is written.

Domain (Resume.h)  unchanged.   Storage (ResumeRepository)  not called by import.
```

### Component Responsibilities

| Component | Responsibility | Depends on |
|-----------|----------------|------------|
| `MainWindow` (ui) | File dialog, call service, show `toPlainText`, errors via `reportError` | application, Qt Widgets |
| `DocumentImport` (application) | Read file (QFile), 10 MiB cap, pick reader by extension (`.docx` only; `.doc`/`.pdf` rejected with message) | documents, Qt Core |
| `DocxReader` (documents) | Find `word/document.xml`, stream-parse WordprocessingML into blocks, record warnings for unsupported constructs | ZipArchive, Qt Core |
| `ZipArchive` (documents) | Thin libzip wrapper: open from memory, read one named entry under a size limit, never extract to disk | libzip |
| `DocumentModel.h` (documents) | Value types `Paragraph`, `Table`, `Block`, `Document`; `toPlainText()` | Qt Core |
| Draft mapping (application, Sprint 2) | `Document` to `Resume` with `id = 0`, in memory only | documents, domain |

## Recommended Project Structure

```
src/
├── documents/                  # new static lib `resume_documents`
│   ├── DocumentModel.h         # Paragraph, Table, Block, Document
│   ├── ZipArchive.h / .cpp     # libzip wrapper with limits
│   └── DocxReader.h / .cpp     # word/document.xml -> Document
├── application/
│   └── DocumentImport.h / .cpp # readDocumentFile(path): file I/O, limits, dispatch
└── ui/MainWindow.*             # one menu action, one preview widget
tests/
├── DocxReaderTest.cpp          # docx_reader_test, ctest `docx_reader`
├── DocumentImportTest.cpp      # document_import_test, ctest `document_import`
└── data/synthetic_resume.docx  # fictional content only, no real PII
```

**Rationale:** `documents/` is its own library so the parser is testable without Widgets, SQL or HTTP, and the future PDF reader can sit beside it (AGENTS.md lists `documents` as a separate layer). The application layer owns file I/O, so `DocxReader` takes `QByteArray`, not a path. Parsed output gets its own model rather than reusing `domain/Resume.h`: `Resume` is the persisted aggregate, while a parsed DOCX is unverified input (AGENTS.md: keep facts separate from AI suggestions).

## Proposed Interfaces

```cpp
// src/documents/DocumentModel.h
namespace documents {
struct Paragraph { QString text; int headingLevel = 0; bool listItem = false; };
struct Table { QVector<QVector<QString>> rows; };
using Block = std::variant<Paragraph, Table>;
struct Document { QString sourceName; QVector<Block> blocks; QStringList warnings; };
QString toPlainText(const Document& doc);
}

// src/documents/DocxReader.h  (pure: no file I/O, no GUI; same error style as ResumeJson::fromJson)
namespace documents::docx {
std::optional<Document> read(const QByteArray& bytes, QString* error = nullptr);
}

// src/application/DocumentImport.h  (file read and limits; no DB access)
namespace DocumentImport {
std::optional<documents::Document> readFile(const QString& path, QString* error = nullptr);
}
```

`sourceName` is the base file name only, so no user directory leaks into the model or messages.

## Data Flow

1. `MainWindow` opens `QFileDialog` (filter `*.docx`) and calls `DocumentImport::readFile(path)`.
2. `readFile` checks `QFileInfo::size() <= 10 MiB`, reads bytes, and calls `docx::read(bytes, &error)`.
3. `docx::read` opens the zip from memory, requires `word/document.xml`, enforces the uncompressed cap (20 MiB) and ratio cap (100:1), then parses with `QXmlStreamReader` (namespace processing on).
4. `MainWindow` shows `toPlainText(doc)` in a read-only `QTextEdit` (plain text only) and the warnings in the status bar. On error it calls `reportError`.

Steps 1–4 never touch `ResumeRepository`, `resume_`, or the disk.

**DOCX element mapping (v1):**

| WordprocessingML | Model |
|------------------|-------|
| `w:body/w:p` | `Paragraph`; text from `w:t`, `w:tab` as `\t`, `w:br` as `\n` |
| `w:pPr/w:pStyle` = `Heading1..6` | `headingLevel` |
| `w:pPr/w:numPr` | `listItem = true` |
| `w:tbl` / `w:tr` / `w:tc` | `Table`, one row per `w:tr`; nested tables flattened with a warning |
| `w:txbxContent`, `w:sdt`, footnotes, comments; header/footer parts | skipped, warning recorded (headers often hold contact details) |

## Mapping to Resume / CareerStage (Sprint 2, not in this milestone)

- `Resume::title`: first `Heading1` text, else file base name; empty plus warning otherwise.
- `CareerStage` from a table whose header row matches known words (период/period, должность/position, компания/company, описание/description): one row = one stage.
- Fallback: a heading-delimited paragraph group becomes one stage with `description` set and other fields empty. Never guess company or period; leave empty and warn.
- `id` stays `0`. Saving is `ResumeRepository::save(draft.resume)` only after the user confirms each stage, reusing the existing transaction code. No schema or JSON change; provenance lives in a `ResumeDraft { Resume resume; QStringList warnings; }` wrapper, not in `CareerStage`.

## Testing with Qt Test

- **`docx_reader_test`** (links `resume_documents`, `Qt6::Test`):
  - Happy path with `tests/data/synthetic_resume.docx`: heading level, list flag, table cells, no warnings for supported elements.
  - Edge cases built in-test with libzip from XML string literals (no binary fixtures): not a zip; no `word/document.xml`; encrypted entry; uncompressed size over cap; ratio over cap; `<!DOCTYPE` with `<!ENTITY>` (must be rejected); malformed XML; empty body (valid, one warning). Use `QTest::newRow` tables.
- **`document_import_test`**: file over cap, `.doc` and `.pdf` rejected with a message, missing file error. Use `QTemporaryDir`.
- **Commands (not yet run):** `cmake --build --preset debug`, then `ctest --preset debug -R "docx_reader|document_import"`.
- **UI** is not unit-tested in this milestone; verify by running the app (`run` skill). Keep widget code thin so the tested surface stays in `documents/` and `application/`.

## Security and Robustness

- Read only the named entry `word/document.xml`; never extract, so `../` path traversal cannot occur.
- Caps: 10 MiB file, 20 MiB uncompressed XML, 100:1 ratio. Reject encrypted archives and OLE `.doc`.
- XML: `QXmlStreamReader` does not fetch external entities, but its entity-resolver path has its own cycle and expansion-limit caveats. Install no custom resolver and reject any `DOCTYPE` token outright.
- Errors and warnings carry element names and counts only, never document text or paths. Nothing is logged in this milestone.

## Build and Packaging

- `add_library(resume_documents STATIC ...)` with `PUBLIC Qt6::Core` and `PRIVATE` libzip; `resume_core` links `resume_documents` so the application layer can call it. `resume_web` stays independent.
- Dependency: libzip (BSD-3-Clause). MSYS2 page `mingw-w64-libzip` (1.11.4) found; expected pacman name `mingw-w64-x86_64-libzip`. Confirm with `pacman -Ss libzip`, then add it to the pacman step in `.github/workflows/ci.yml`.
- Installer: confirm that `cmake/DeployMinGWRuntime.cmake` ships `libzip.dll`; otherwise prefer a static libzip build.
- Alternative: vendored single-header miniz (MIT). Fewer system dependencies, but we would own the zip parser and its security checks. Use only if the MSYS2 package is unavailable.
- Rejected: Qt `QZipReader` (private Gui API, not stable); `QDomDocument` (whole-DOM load, DTD handling on by default); external converters such as pandoc (extra binary, against the offline-first stance).

## Anti-Patterns to Avoid

- **Parsing in `MainWindow`.** Repeats the widget-logic problem already noted in the codebase map and blocks reuse by a future CLI or web import. Put it in `DocumentImport`.
- **Writing an import straight into `resume_` or the DB.** Turns unverified text into a stored fact and breaks the "no invented achievements" rule. Keep the result in memory until the user saves.
- **Using the file path as identity.** Leaks directory names and ties the model to one machine. Store `sourceName` only.
- **Blocking the GUI for slow sources.** The 10 MiB cap keeps DOCX synchronous. When OCR or LLM work arrives, use `QtConcurrent::run` and deliver results by queued signal (AGENTS.md: no UI blocking).

## Open Questions

- Confirm the MSYS2 package name and whether `libzip.dll` is deployed by the installer.
- Decide whether header/footer text (name, phone, e-mail) is in v1; it is often the only place for contacts.
- Decide whether `toPlainText` stays in `documents/` or moves to the application layer when the web UI needs a different rendering.

## Sources

- Repository: `.planning/codebase/ARCHITECTURE.md`, `.planning/codebase/STRUCTURE.md`, `src/domain/Resume.h`, `src/application/ResumeJson.h`, `src/storage/ResumeRepository.h`, `CMakeLists.txt`, `docs/plan.md` (Sprint 2), `AGENTS.md` sections 1, 3, 4.
- Qt 6 QXmlStreamReader: https://doc.qt.io/qt-6/qxmlstreamreader.html
- Qt 6 QXmlStreamEntityResolver: https://doc.qt.io/qt-6/qxmlstreamentityresolver.html
- MSYS2 libzip package page: https://packages.msys2.org/base/mingw-w64-libzip (name to be confirmed locally)

---
*Architecture research for: AI Resume Assistant, DOCX import milestone*
*Researched: 2026-10-24*
