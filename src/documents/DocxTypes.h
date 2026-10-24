#pragma once

#include <QtGlobal>

// Error and warning codes plus limit constants of the documents layer. This file holds
// codes and numbers only: user-facing texts live in the application layer (D-04, D-05),
// never here.
namespace documents {

// Error codes only; the Russian texts for them live in the application layer (D-04, D-05).
enum class DocxError {
    NotZip,
    Corrupt,
    Encrypted,
    LegacyDoc,
    TooLarge,
    Limit,
    MissingPart,
    UnsafeXml,           // a DOCTYPE, with or without entities (SAFE-03)
    UnsupportedEncoding, // a declared XML encoding other than UTF-8 or UTF-16 (OQ3)
};

// Non-fatal findings. The read still succeeds; the warnings list holds one entry per
// missing optional part (D-08, D-09), so its size is the count of such parts.
enum class DocxWarning {
    StylesPartMissing,
    NumberingPartMissing,
};

// Units: "MB" in D-01 and D-02 is read as MiB (1024 * 1024 bytes). Owner confirmation pending.

// D-01: largest input accepted, in bytes. Enforced at the container and, before any open,
// at the file level in plan 01-03.
inline constexpr qint64 kMaxFileBytes = qint64{10} * 1024 * 1024;

// D-02, reinterpreted: one budget shared by word/document.xml, word/styles.xml and
// word/numbering.xml within one open. D-02 as written names only word/document.xml; the
// shared-budget scope awaits owner confirmation (recorded in plan 01-04).
inline constexpr qint64 kMaxUnpackedBytes = qint64{20} * 1024 * 1024;

// Planner choice, enforced in plan 01-04. Value to confirm with the owner.
inline constexpr qint64 kMaxEntries = 1000;

// Planner choice, enforced in plan 01-04. Value to confirm with the owner.
inline constexpr qint64 kMaxCompressionRatio = 100;

// Planner choice, enforced in plan 01-04. Value to confirm with the owner.
inline constexpr qint64 kCompressionCheckAfterBytes = qint64{1024} * 1024;

// D-03, enforced in Phase 2.
inline constexpr qint64 kMaxTextChars = 200000;

} // namespace documents
