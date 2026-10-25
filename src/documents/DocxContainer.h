#pragma once

#include "documents/DocxTypes.h"
#include <QByteArray>
#include <QList>
#include <expected>

namespace documents {

// The three DOCX parts this layer reads, held in memory. Only these parts are read (D-10).
struct DocxParts {
    QByteArray documentXml;
    QByteArray stylesXml;
    QByteArray numberingXml;
    // One entry per missing optional part, per header or footer part, and for word/comments.xml
    // (D-08, D-09, OQ2, OQ10). The size is the count of those parts; no content is read for them.
    QList<DocxWarning> warnings;
};

// Classifies the input by its leading bytes, then reads the three parts from memory.
// Takes bytes, never a path (Pitfall 2, D-05). Returns a code and no partial result on
// any failure (D-07).
// unpackedBudget is the one budget shared by the three parts in this open (D-02, as
// reinterpreted). It defaults to kMaxUnpackedBytes; the parameter lets tests exercise the shared
// budget with small numbers. A declared size never changes it.
std::expected<DocxParts, DocxError> readDocxParts(QByteArray bytes,
                                                  qint64 unpackedBudget = kMaxUnpackedBytes);

} // namespace documents
