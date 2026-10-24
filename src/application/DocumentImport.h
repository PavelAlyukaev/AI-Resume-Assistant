#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <expected>

namespace DocumentImport {

// The three DOCX parts read from one file. warnings holds one Russian text per missing
// optional part (D-08, D-09); its size is the count of such parts.
struct ImportedDocx {
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
