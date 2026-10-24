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
    // One entry per missing optional part (D-08, D-09); the size is the count.
    QList<DocxWarning> warnings;
};

// Classifies the input by its leading bytes, then reads the three parts from memory.
// Takes bytes, never a path (Pitfall 2, D-05). Returns a code and no partial result on
// any failure (D-07).
std::expected<DocxParts, DocxError> readDocxParts(QByteArray bytes);

} // namespace documents
