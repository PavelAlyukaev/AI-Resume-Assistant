#pragma once
#include "documents/DocxTypes.h"
#include <QByteArray>
#include <expected>
#include <memory>
#include <string_view>

namespace documents {

// Read-only ZIP archive held in memory. The class owns the bytes and never takes a
// file path: the caller reads the file with QFile and passes the QByteArray in, so a
// non-ASCII path never reaches libzip's narrow-path API (Pitfall 2, D-05).
// libzip types are not visible from this header (pimpl).
class ZipArchive {
  public:
    static std::expected<std::unique_ptr<ZipArchive>, DocxError> open(QByteArray bytes);
    ~ZipArchive();
    ZipArchive(const ZipArchive &) = delete;
    ZipArchive &operator=(const ZipArchive &) = delete;

    // Exact, case-sensitive entry name. A name containing NUL never matches.
    bool hasEntry(std::string_view exactName) const;

    // Bytes of one entry, or an error with no partial result (D-07). Reading stops with
    // DocxError::Limit before the running total would exceed maxBytes.
    std::expected<QByteArray, DocxError> readEntry(std::string_view exactName, qint64 maxBytes) const;

  private:
    struct Impl;
    explicit ZipArchive(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace documents
