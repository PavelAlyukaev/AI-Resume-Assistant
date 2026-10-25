#pragma once

#include "documents/DocxModel.h"
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <expected>

namespace DocumentImport {

// The result of one import. document is the parsed body model (plan 02-04). documentXml,
// stylesXml and numberingXml are the raw parts, kept until phase 3 (OQ6). warnings holds the
// Russian texts of all warnings, missing parts included, in the order of document.warnings;
// its size is the count of such items (D-08, D-09).
struct ImportedDocx {
    documents::DocxDocument document;
    QByteArray documentXml;
    QByteArray stylesXml;
    QByteArray numberingXml;
    QStringList warnings;
};

// Reads the file at path only. This function never receives or changes a draft, so a
// failed import cannot change an open draft (SAFE-01 draft rule). The error is a fixed
// Russian text that names only the file, never the directory or document content (D-04, D-05).
std::expected<ImportedDocx, QString> readDocxFile(const QString &path);

} // namespace DocumentImport
