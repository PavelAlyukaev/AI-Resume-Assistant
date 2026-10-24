#include "documents/ZipArchive.h"
#include <array>
#include <string>
#include <utility>
#include <vector>
#include <zip.h>

namespace documents {

namespace {

constexpr std::size_t kReadChunkBytes = 64 * 1024;

// RAII holder for zip_error_t so zip_error_fini runs on every path.
// The error text is never read; only the status is mapped to DocxError.
class ZipError {
  public:
    ZipError() { zip_error_init(&error_); }
    ~ZipError() { zip_error_fini(&error_); }
    ZipError(const ZipError &) = delete;
    ZipError &operator=(const ZipError &) = delete;
    zip_error_t *get() { return &error_; }

  private:
    zip_error_t error_{};
};

} // namespace

struct ZipArchive::Impl {
    // Owned bytes. libzip keeps a pointer into this buffer while the archive is open,
    // so it must outlive `archive` (Pattern 1, Pitfall 7).
    QByteArray bytes;
    zip_t *archive = nullptr;
};

std::expected<std::unique_ptr<ZipArchive>, DocxError> ZipArchive::open(QByteArray bytes) {
    auto impl = std::make_unique<Impl>();
    impl->bytes = std::move(bytes);

    ZipError error;
    // freep = 0: the QByteArray keeps ownership of the memory; the source only borrows it.
    // constData() is the only accessor used, so the buffer cannot detach while libzip reads it.
    zip_source_t *source = zip_source_buffer_create(
        impl->bytes.constData(), static_cast<zip_uint64_t>(impl->bytes.size()), 0, error.get());
    if (!source)
        return std::unexpected(DocxError::Corrupt);

    // Ownership of `source` after a failed zip_open_from_source is not documented in zip.h
    // or in the zip_open_from_source man page, so it is deliberately not freed here:
    // a small leak on this rare path is preferred over a possible double free.
    zip_t *archive = zip_open_from_source(source, ZIP_RDONLY | ZIP_CHECKCONS, error.get());
    if (!archive)
        return std::unexpected(DocxError::Corrupt);

    impl->archive = archive;
    return std::unique_ptr<ZipArchive>(new ZipArchive(std::move(impl)));
}

ZipArchive::ZipArchive(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

ZipArchive::~ZipArchive() {
    // Close the archive first. impl_ (and the QByteArray libzip points into) is released
    // only after this body returns.
    if (impl_ && impl_->archive)
        zip_discard(impl_->archive);
}

bool ZipArchive::hasEntry(std::string_view exactName) const {
    if (exactName.find('\0') != std::string_view::npos)
        return false;
    const std::string name(exactName);
    return zip_name_locate(impl_->archive, name.c_str(), 0) >= 0;
}

std::expected<QByteArray, DocxError> ZipArchive::readEntry(std::string_view exactName,
                                                           qint64 maxBytes) const {
    // A NUL would make zip_name_locate compare only the prefix, so "word/document.xml\0.bak"
    // would match "word/document.xml". Reject such names outright.
    if (exactName.find('\0') != std::string_view::npos)
        return std::unexpected(DocxError::MissingPart);

    const std::string name(exactName);
    // Flags 0: case-sensitive exact match (no ZIP_FL_NOCASE).
    const zip_int64_t index = zip_name_locate(impl_->archive, name.c_str(), 0);
    if (index < 0)
        return std::unexpected(DocxError::MissingPart);

    // Refined to Encrypted in plan 01-02; until then every failed open is Corrupt.
    zip_file_t *file = zip_fopen_index(impl_->archive, static_cast<zip_uint64_t>(index), 0);
    if (!file)
        return std::unexpected(DocxError::Corrupt);

    // The declared size from the central directory is never used for allocation (Pitfall 3).
    // Bytes are counted as they are actually produced by zip_fread.
    QByteArray out;
    std::vector<char> chunk(kReadChunkBytes);
    qint64 total = 0;
    for (;;) {
        const zip_int64_t got = zip_fread(file, chunk.data(), chunk.size());
        if (got < 0) {
            zip_fclose(file);
            return std::unexpected(DocxError::Corrupt);
        }
        if (got == 0)
            break;
        if (total + got > maxBytes) {
            zip_fclose(file);
            return std::unexpected(DocxError::Limit);
        }
        out.append(chunk.data(), static_cast<qsizetype>(got));
        total += got;
    }

    // A nonzero close result (for example a CRC mismatch) discards the whole entry (D-07).
    if (zip_fclose(file) != 0)
        return std::unexpected(DocxError::Corrupt);
    return out;
}

} // namespace documents
