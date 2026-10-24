# Feature Research

**Domain:** Windows desktop resume editor — read-only DOCX text import (milestone: open .docx and show its text in a Qt window)
**Researched:** 2026-10-24
**Confidence:** MEDIUM overall. Table stakes rest on the stable OOXML package structure. Resume-specific frequencies are LOW (vendor blogs only). All web-sourced claims are LOW per the `classify-confidence` seam. OOXML details taken from model knowledge (marked "verify") must be checked against real sample files.

## Scope Assumed

From `.planning/PROJECT.md`: open a `.docx`, show paragraphs, headings, bulleted/numbered lists and tables as text, ignore images, and replace the current draft without a prompt. Text boxes, headers/footers, hyperlinks, encryption and legacy `.doc` are handling decisions for this milestone, not full features.

## What a Typical Resume DOCX Contains

A `.docx` is a ZIP of XML parts. The body is `word/document.xml`; styles, numbering, relationships (hyperlink targets) and headers/footers are separate parts.

| Construct | Where it lives (verify) | Seen in resumes (LOW) | Milestone handling |
|-----------|-------------------------|-----------------------|--------------------|
| Heading styles (Heading 1/2, Title, custom) | `w:pStyle` in document.xml; definitions in `styles.xml` | Very common | Map to heading level |
| Bulleted / numbered lists | `w:numPr` (`numId` + `ilvl`); `numFmt` in `numbering.xml` | Very common | List item with indent by level |
| Tables (dates column + description) | `w:tbl` / `w:tr` / `w:tc` | Common in Word templates | Rows and columns, cell text |
| Nested tables | `w:tbl` inside `w:tc` | Occasional | Recurse |
| Merged cells | `w:gridSpan`, `w:vMerge` | Occasional | No text loss; approximate layout |
| Text boxes / sidebars (contact, skills) | `mc:AlternateContent` with `wps:txbxContent` (Choice) and VML `v:textbox` (Fallback) | Common in designed templates | Read one branch only (see Differentiators) |
| Headers / footers (name, contact, page no.) | `header*.xml`, `footer*.xml`, referenced from `sectPr` | Common | Separate block, not repeated per page |
| Hyperlinks (email, LinkedIn, GitHub) | `w:hyperlink` with `r:id` to `document.xml.rels` (External) | Very common | Show link text |
| Fields (PAGE, HYPERLINK) | `w:fldSimple`, or `w:fldChar` begin/separate/end with `w:instrText` | Occasional | Keep cached result, drop instruction text |
| Content controls | `w:sdt` / `w:sdtContent` (placeholder via `w:showingPlcHdr`) | Occasional (templates, date pickers) | Unwrap content; skip placeholder text |
| Tracked changes | `w:ins`, `w:del` with `w:delText` | Occasional in drafts | Accepted view (see Open Questions) |
| Tabs, line breaks, symbols | `w:tab`, `w:br`, `w:sym` (Wingdings/Symbol bullets) | Common | Tab, newline; symbol mapped or dropped |
| Images, shapes, charts | `w:drawing`, `w:pict` | Common (photos) | Ignored (milestone rule) |
| Comments, footnotes | `comments.xml`, `footnotes.xml` | Rare | Ignored and counted |
| Non-breaking spaces, Cyrillic `Ё` | Plain `w:t` text | Common in Russian resumes | Preserved exactly |

## Table Stakes (must ship in this milestone)

Missing any of these makes the reader feel broken or unsafe.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Open `.docx` via file dialog (filter `*.docx`) | Basic entry point | LOW | Pass paths as `QString`/`QFile`. `std::ifstream` with narrow paths breaks non-ASCII Windows paths on MinGW. |
| Validate package: ZIP readable, `word/document.xml` present | Malformed files are common (renamed `.doc`, truncated downloads) | MEDIUM | Show a readable reason; never crash |
| Skip Word lock files `~$*.docx` | They match the glob, are ~162 bytes and unreadable (verify) | LOW | Filter them out or report "file is open in Word" |
| Body in document order, paragraphs and tables interleaved | Resume reads top to bottom | MEDIUM | Walk `w:body` children in sequence |
| Heading detection by style | Career-stage structure depends on it | LOW–MEDIUM | `Heading1..N`, `Title`, `outlineLvl`. Test on a Russian-UI Word file. |
| Bulleted and numbered lists with nesting | Experience bullets are the core content | MEDIUM | `numId`+`ilvl` → `numbering.xml` `numFmt` (bullet vs decimal) |
| Tables keep rows, columns and empty cells | Career-stage tables | MEDIUM | Keep column count; honor `gridSpan`; cells can hold paragraphs and nested tables |
| Hyperlink text shown | Contact lines vanish otherwise | LOW | Common bug: reading only direct `w:r` children of `w:p` |
| Unwrap containers: `w:ins`, `w:sdt`, `w:smartTag`, `w:fldSimple`, `w:customXml` | Text would otherwise be missing | LOW–MEDIUM | Skip `w:del`/`w:delText` and `w:instrText` |
| Cyrillic and Unicode preserved exactly (UTF-8 XML, `Ё`, NBSP) | Russian resumes | LOW | Do not normalize `Ё`→`Е`; NBSP displayed as space only |
| Images ignored without error | Milestone rule | LOW | Skip `w:drawing`, `w:pict` |
| Encrypted file detected, clear message | Common in HR-system exports | LOW | OLE2 signature `D0 CF 11 E0 A1 B1 1A E1` plus an `EncryptedPackage` stream (PRONOM, MS-OFFCRYPTO) |
| Legacy `.doc` detected, clear message | Old resumes | LOW | OLE2 without `EncryptedPackage` → "save as .docx in Word" |
| Size and entry limits (zip-bomb guard) | Input is untrusted | MEDIUM | Cap total uncompressed size and entry count; XML parser must not load external entities |
| Failed open leaves current draft untouched | Replace-without-prompt makes failure dangerous | LOW | Parse into a new model; swap only on success |
| No silent loss of body text | Users trust what they see | LOW | Count skipped constructs (text boxes, comments) and show them in the status bar |
| Error text without personal data | AGENTS.md privacy rule | LOW | Show generic reason and file base name; never document text |

## Differentiators (valued, not required)

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Text box content in document order | Designed templates keep contact/skills there | MEDIUM | Mammoth.js moves box content after the anchoring paragraph. Read only one of Choice/Fallback to avoid duplicates (verify). |
| Header/footer block shown once, labeled | Name and contact often sit in the header | MEDIUM | Do not repeat per page |
| Faithful numbering formats (roman, letters, custom `lvlText`, restarts) | Lists look as authored | MEDIUM–HIGH | Needed only if numbers are displayed |
| Hyperlink target shown next to text | Verify links | LOW | Keep text and target as separate model fields |
| Heading outline / structure tree | Prepares career-stage mapping in the next milestone | MEDIUM | Built from heading list |
| Status-bar summary (file, paragraphs, tables, skipped items) | Trust and debugging | LOW | Same data as "no silent loss" |
| Single-level undo of the replaced draft | Mitigates the no-prompt decision | MEDIUM | In memory only; not a persistence feature |
| Drag-and-drop open, recent files | Convenience | LOW | |
| Symbol bullets mapped (Wingdings `F0B7` → `•`) | Bullets otherwise show as private-use glyphs | LOW–MEDIUM | Verify on a real template |

## Anti-Features (commonly requested, problematic here)

| Anti-Feature | Why Requested | Why Problematic | Alternative |
|--------------|---------------|-----------------|-------------|
| Layout-faithful rendering (fonts, columns, page breaks) | "Should look like Word" | Needs a layout engine; not needed to read text | Structural text view |
| Word COM automation, LibreOffice or Pandoc shell-out | Quick conversion | Requires Office installed, locks files, external process, opaque errors | Native ZIP + XML reader |
| Password prompt and decryption | Users have protected files | Key handling, privacy risk, out of scope | Detect and tell user to remove the password in Word |
| Binary `.doc` parser | Old files | Large separate format effort | Ask user to save as `.docx` |
| Auto-extracting career stages during import | Looks like time saved | Mixes parsed facts with AI suggestions; mapping belongs to a later sprint with user confirmation | Next milestone |
| Writing back to DOCX | Round-trip editing | Risks corrupting the user's file; export is Sprint 4 | Read-only import |
| Auto-saving the replaced draft to SQLite | Removes the data-loss risk | Silently persists unwanted state | Keep current decision; offer undo |
| Treating "restrict editing" as encryption | Looks similar in Word | Protection (`documentProtection` in settings) does not block opening; wrongly rejecting files | Open normally |
| Image extraction or preview | Visual check | Explicitly out of scope | Later milestone |
| Live reload when the file changes | Convenience | Locking and race complexity | Manual reopen |
| `[image]` placeholders in text | Marks where pictures were | Noise in a text view | Skip silently |

## Feature Dependencies

```
Open file -> ZIP reader -> document.xml parser -> body model (new, not yet applied)
   |-- headings and lists need styles.xml and numbering.xml
   |-- URL display needs document.xml.rels
   |-- text boxes need mc:AlternateContent handling
   |-- header/footer block needs sectPr -> header/footer parts
Apply model to draft (only after successful parse)
Heading outline (differentiator) needs heading detection
Career-stage mapping (next milestone) needs paragraphs + headings + tables model
```

## MVP Definition

### Launch With (this milestone)

- [ ] All table stakes above — they define "reads a resume DOCX without losing text or crashing"
- [ ] Synthetic test fixtures (no personal data committed): Russian-UI Word file, LibreOffice-saved file, nested list, table with merged cell, hyperlink, text box, header, tracked change, content control, `.doc` renamed to `.docx`, truncated ZIP, password-protected file
- [ ] Qt Test on the Russian fixture with expected text (matches the PROJECT.md active requirement)

### Add After Validation (v1.x)

- [ ] Header/footer block — once real resumes show contact info there
- [ ] Text box content (if deferred from MVP) — once samples confirm usage
- [ ] Hyperlink URL display, faithful numbering formats — when the UI needs them
- [ ] Undo of the replaced draft — after the no-prompt behavior is used in practice

### Future Consideration (v2+)

- [ ] Legacy `.doc` reading — large effort; conversion guidance covers the need
- [ ] Password-protected DOCX support — only if users need it and key handling is designed
- [ ] Layout fidelity — not needed for text-based workflow

## Reference Behavior of Existing Readers

| Feature | Mammoth.js (DOCX to HTML) | Apache POI (XWPF) | Our Approach |
|---------|---------------------------|-------------------|--------------|
| Text boxes | Read; content moved after the anchoring paragraph | Older bug reports say not read from .docx (LOW) | Read once, after anchor |
| Headers / footers | Not described | `getText()` includes them; nested tables may be missed (LOW) | Separate labeled block |
| Tables | Text kept; borders and table formatting ignored | Text extracted | Text kept, no borders |
| Encryption | Not handled | Throws on encrypted input (LOW) | Detect and message |

## Open Questions for Requirements and Roadmap

1. **Text boxes:** include in this milestone (recommended if sample resumes use them) or report as skipped?
2. **"Replaces current draft":** is the draft only in UI memory or also in SQLite? This decides what the status message must say.
3. **Tracked changes:** assume accepted view (insertions shown, deletions hidden)? Confirm.
4. **Headers/footers:** show once as a labeled block, or count as skipped?
5. **Test Word versions:** which UI languages and Word versions must the fixtures cover?

## Sources

- mammoth.js README: https://github.com/mwilliamson/mammoth.js (fetched; LOW per seam)
- msoffcrypto-tool documentation: https://msoffcrypto-tool.readthedocs.io/ (fetched; LOW per seam)
- PRONOM format 494 (Office encrypted document): https://pronom.nationalarchives.gov.uk/fmt/494 (search snippet)
- MS-OFFCRYPTO specification: https://learn.microsoft.com/en-us/openspecs/office_file_formats/ms-offcrypto/9d9cebee-a465-4c2f-a4df-d9ae83be5d77 (search snippet)
- Apache POI bug reports: https://bz.apache.org/bugzilla/show_bug.cgi?id=58654 and https://bz.apache.org/bugzilla/show_bug.cgi?id=55733 (search; old, LOW)
- Resume-parsing vendor guides, anecdotal: https://www.jobmentis.com/en/guide/resume-parsing and https://careerbldr.com/blog/resume-parsing-how-it-works/ (LOW)
- ECMA-376 / ISO/IEC 29500 WordprocessingML: model knowledge, not fetched in this session. Verify on sample files.

Context7 was not available in this session, so OOXML questions fell back to WebSearch/WebFetch.

---
*Feature research for: DOCX text import milestone, AI Resume Assistant*
*Researched: 2026-10-24*
