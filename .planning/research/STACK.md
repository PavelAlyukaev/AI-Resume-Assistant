# Stack Research

**Domain:** Windows desktop resume editor, milestone "read .docx and show text in a Qt window" (paragraphs, headings, lists, tables; images ignored)
**Researched:** 2026-10-24
**Confidence:** HIGH for local environment and licensing facts (checked on this machine); MEDIUM for OOXML details; LOW for upstream release facts taken from WebFetch (the seam tags WebFetch results LOW even when verified)

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| libzip | 1.11.4 (MSYS2 `mingw-w64-x86_64-libzip`, not installed yet) | Open the `.docx` ZIP container and read `word/document.xml`, `word/styles.xml`, `word/numbering.xml` | The only maintained ZIP reader with a public C API in the MSYS2 mingw64 repo. Returns error codes for corrupt or non-ZIP files, which the open question about broken files needs. BSD-3-Clause. |
| QXmlStreamReader | Qt 6 QtCore (local Qt 6.11.1; project minimum 6.7) | Stream-parse the OOXML parts | Already linked through `Qt6::Core`. No new dependency or license. Namespace-aware by default. Pull parser, so the walker controls depth. |
| QTextDocument / QTextCursor | Qt 6 QtGui (via `Qt6::Widgets`) | Display the parsed document | `insertTable`, `createList`, `insertBlock` and `insertText` map directly onto paragraphs, lists and tables. Avoids building HTML strings and escaping them. |

**Architecture implication:** libzip and QXmlStreamReader live in a new `src/documents/` layer that returns a neutral model (blocks: paragraph with heading level or list info, table with cells holding blocks). `MainWindow` only renders that model, consistent with AGENTS.md (no business logic in QWidget).

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| pugixml | 1.16 (installed locally, MIT) | DOM XML parser, static CMake target | Only if the streaming walker becomes hard to maintain (nested `sdt`, tables, text boxes). Static linking adds no DLL. |
| zlib | 1.3.2 (installed; Zlib license) | Pulled in by Qt and libzip | Not used directly. |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| pkgconf (`mingw-w64-x86_64-pkgconf`, installed) | Locates libzip for CMake | `pkg_check_modules(LIBZIP REQUIRED IMPORTED_TARGET libzip)`. Safer than assuming a libzip CMake package config exists; not verified on this machine yet. |
| Generated test fixtures | Qt Test input | Build minimal `.docx` fixtures with a script or Word. Do not commit real resumes (AGENTS.md). |

## Installation

```bash
# MSYS2 MINGW64 shell (C:/msys64/mingw64)
pacman -S mingw-w64-x86_64-libzip

# Already installed locally (checked): mingw-w64-x86_64-qt6-base 6.11.1,
# mingw-w64-x86_64-pugixml 1.16, mingw-w64-x86_64-pkgconf
```

CMake sketch (to be applied in the implementation step, not in this research):

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBZIP REQUIRED IMPORTED_TARGET libzip)
add_library(resume_documents STATIC src/documents/DocxReader.cpp src/documents/DocxReader.h)
target_link_libraries(resume_documents PUBLIC resume_core Qt6::Core Qt6::Gui PkgConfig::LIBZIP)
```

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| libzip | minizip-ng 4.2.0 (Zlib license, in mingw64 repo) | Fine technically. Its MSYS2 build depends on OpenSSL, which is a bigger runtime footprint than libzip's. Choose it if the Zlib license is preferred. |
| libzip | Own ZIP reader on zlib (raw inflate, about 200 lines) | Only if the libzip dependency chain is unacceptable. DOCX needs stored and deflate entries, but you must handle zip64 and data descriptors yourself. |
| QXmlStreamReader | pugixml 1.16 | Easier traversal of nested tables and content controls. Pick it if the streaming code becomes verbose. |
| QTextDocument (built with cursor) | `setHtml` with a generated HTML string | Fast to prototype, but requires escaping and loses the structure the model needs. Reject. |
| Parse into own model | Pandoc as external process | Adds a separate executable to install and ship, and a process boundary to manage. Not worth it for one format. |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| `QZipReader` from `<private/qzipreader_p.h>` (`Qt6::CorePrivate`) | Private API with no stability guarantee. The header says it may change or be removed. A May 2026 Qt development-list thread debates removing it or making it public. | libzip |
| Regex or string search over `document.xml` | Breaks on namespaces, entities, attributes and nested tables. | QXmlStreamReader or pugixml |
| Word COM automation (`Word.Application`) | Requires Microsoft Office on the user's machine, is slow, and triggers security prompts. | libzip + XML parser |
| Commercial DOCX SDKs | Proprietary licence and cost, which conflict with an open GPL-3.0-or-later project's dependency policy. | libzip + QXmlStreamReader |
| Reading legacy `.doc` (CFB binary) as a ZIP | It is not a ZIP, so the open fails. | Detect and show an error that names the expected format |

## Parser Requirements the Stack Must Support

These come from OOXML / ECMA-376 and Microsoft documentation (MEDIUM; verify with a fixture set):

- **Text:** `w:p` > `w:r` > `w:t`. `w:tab` becomes `\t`; `w:br` and `w:cr` become newlines. Skip `w:delText` (tracked deletions).
- **Headings:** `w:pPr/w:pStyle@w:val` matches a `w:styleId` in `styles.xml`. Follow `w:basedOn` and also check `w:outlineLvl`. Built-in heading styles carry names like "heading 1" in `styles.xml`; confirm this with a Russian-UI Word fixture.
- **Lists:** `w:pPr/w:numPr` (`w:numId`, `w:ilvl`) > `numbering.xml` `w:num` > `w:abstractNum/w:lvl/w:numFmt` (bullet or decimal). Word stores no list labels, so the parser must render the bullet or number. `numId` 0 means no list. Numbering can also come from the paragraph style.
- **Tables:** `w:tbl` > `w:tr` > `w:tc` > block content, including nested tables. `w:gridSpan` and `w:vMerge` need handling or an explicit "not supported" for v1.
- **Wrappers to descend into:** `w:sdt`/`w:sdtContent` (content controls, common in resume templates), `w:hyperlink`, `w:ins`, `w:fldSimple`, `w:smartTag`.
- **Images:** skip `w:drawing`, `w:pict` and `word/media/*`.
- **Text boxes:** `w:txbxContent` (often inside `mc:AlternateContent`) can hold resume text in templates. "Ignore images" does not mean "ignore text boxes", so decide this explicitly.
- **Errors:** not a ZIP (encrypted files, `.doc`), missing `word/document.xml`, malformed XML (`QXmlStreamReader::hasError()` gives line and column). Show a message without document content, as AGENTS.md requires.

A generic streaming walker over element names is enough. No schema-binding library is needed.

## Stack Patterns by Variant

**If the walker gets hard to maintain (nested `sdt`, tables, text boxes):**
- Switch XML parsing to pugixml (static target, MIT). Keep libzip.
- Because pugixml's DOM makes child traversal simpler, at the cost of loading the whole document into memory (fine for resumes).

**If the dependency footprint must stay at zero:**
- Write the ZIP reader on zlib with `inflateInit2(-MAX_WBITS)`. Support stored and deflate entries only.
- Because zlib is already shipped with Qt, no new DLL is added. Keep it behind the same `src/documents/` interface so it can be swapped for libzip later.

**If Sprint 4 needs DOCX export:**
- Reuse libzip, which also writes archives (not verified in this session).

## Version Compatibility

| Package | Compatible With | Notes |
|---------|-----------------|-------|
| libzip 1.11.4 (MSYS2) | zlib 1.3.2, bzip2, xz, zstd, gnutls, nettle (MSYS2 dependencies) | The transitive DLLs are copied by `cmake/DeployMinGWRuntime.cmake` through `get_runtime_dependencies`. Check the installer size. |
| Qt 6.11.1 (local) | CMake minimum Qt 6.7 | QXmlStreamReader and QTextCursor APIs are stable across this range. |
| pugixml 1.16 | MSYS2 `cc-libs` | CMake targets `pugixml::static` and `pugixml::shared` exist. |
| libzip upstream 1.12.1 | MSYS2 ships 1.11.4 | Upstream is newer, but the local repo is the build source. Do not add a manual build. |

## Licensing (project: GPL-3.0-or-later)

| Component | License | Compatible? | Action |
|-----------|---------|-------------|--------|
| Qt 6 (qt6-base) | LGPL-3.0-only with Qt GPL exception, plus others | Yes | Keep Qt as DLLs, as the project already does. Include the LGPL notices in the installer. |
| libzip | BSD-3-Clause | Yes | Keep the copyright notice in distributed files. |
| pugixml (optional) | MIT | Yes | Keep the notice. |
| zlib | Zlib | Yes | Keep the notice. |
| gnutls, nettle (libzip transitive) | LGPL family (verify exact versions) | Yes when dynamically linked | Ship as DLLs. Verify licence texts before release. |
| minizip-ng (rejected) | Zlib | Yes | No action. |

This is a technical licence check, not legal advice. Re-check the notice bundle at release time.

## Sources

- Local MSYS2 `pacman -Ss / -Si / -Qi` on `C:/msys64` (read-only, against the local sync database, which may be stale; `pacman -Sy` was not run): libzip 1.11.4 (BSD-3-Clause), minizip-ng 4.2.0 (Zlib), pugixml 1.16 installed (MIT), zlib 1.3.2 (Zlib), qt6-base 6.11.1 — HIGH
- Local headers and CMake files: `C:/msys64/mingw64/include/qt6/QtCore/qxmlstream.h` (QXmlStreamReader in QtCore), `.../QtCore/6.11.1/QtCore/private/qzipreader_p.h` (QZipReader is private, `Q_CORE_EXPORT`), `.../QtGui/qtextcursor.h` (insertTable, createList), `C:/msys64/mingw64/lib/cmake/pugixml/pugixml-targets.cmake` — HIGH
- [libzip.org](https://libzip.org/) — upstream 1.12.1, BSD licence (WebFetch) — LOW
- [pugixml manual](https://pugixml.org/docs/manual.html) — MIT, DOM semantics (WebFetch) — LOW
- [Qt QXmlStreamReader docs](https://doc.qt.io/qt-6/qxmlstreamreader.html) — QtCore, streaming, namespaces (WebFetch) — LOW
- [Qt QTextCursor docs](https://doc.qt.io/qt-6/qtextcursor.html) — QtGui, table and list API (WebFetch) — LOW
- [Qt development list, May 2026: remove or make QZipReader official](https://lists.qt-project.org/pipermail/development/2026-May/047182.html) — MEDIUM
- [Microsoft Learn: Numbered lists in WordprocessingML](https://learn.microsoft.com/en-us/previous-versions/office/developer/office-2010/ee922775(v=office.14)) — numId, ilvl, abstractNum mapping — MEDIUM
- [ECMA-376 `w:tbl` reference (docx4java)](https://webapp.docx4java.org/OnlineDemo/ecma376/WordML/tbl.html) — table structure (`tblGrid`, `tr`, `tc`) — MEDIUM

## Open Questions for the Implementation Phase

- libzip is not installed yet. Run a configure test to confirm the pkg-config path works under `CMAKE_PREFIX_PATH=C:/msys64/mingw64`.
- Confirm that `libzip-*.dll` and its transitive DLLs appear in the deploy directory and that `DeployMinGWRuntime.cmake` reports no unresolved dependencies.
- Build a fixture set: a Russian-UI Word file (heading names), text boxes, `sdt` content controls, merged table cells, a corrupt file, and a password-protected file.

---
*Stack research for: DOCX text import in a Qt 6 Widgets desktop app (MinGW, C++23)*
*Researched: 2026-10-24*
