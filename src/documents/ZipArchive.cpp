#include "documents/ZipArchive.h"
#include <algorithm>
#include <array>
#include <set>
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

// Structural checks run before any entry is read. The count comes first: more than kMaxEntries
// entries is Limit, and no name is read at all. Then every name is read once; a name seen twice
// is Corrupt, because a duplicate could hide which part a reader selects (T-01-14, T-01-15).
// Each name is also appended to names, in central-directory order, for entryNames().
std::expected<void, DocxError> checkEntryStructure(zip_t *archive, std::vector<std::string> &names) {
    const zip_int64_t count = zip_get_num_entries(archive, 0);
    if (count < 0)
        return std::unexpected(DocxError::Corrupt);
    if (count > kMaxEntries)
        return std::unexpected(DocxError::Limit);

    std::set<std::string> seen;
    names.reserve(static_cast<std::size_t>(count));
    for (zip_int64_t i = 0; i < count; ++i) {
        // Only the name is compared. No libzip message text is read here either (T-01-04).
        const char *name = zip_get_name(archive, static_cast<zip_uint64_t>(i), 0);
        if (!name)
            return std::unexpected(DocxError::Corrupt);
        if (!seen.insert(name).second)
            return std::unexpected(DocxError::Corrupt);
        names.emplace_back(name);
    }
    return {};
}

} // namespace

struct ZipArchive::Impl {
    // Owned bytes. libzip keeps a pointer into this buffer while the archive is open,
    // so it must outlive `archive` (Pattern 1, Pitfall 7).
    QByteArray bytes;
    zip_t *archive = nullptr;
    // Entry names from checkEntryStructure, in central-directory order (entryNames()).
    std::vector<std::string> names;
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

    // A refused archive is closed here. Closing it releases the source that libzip now owns,
    // which is the case where the source was accepted by a successful open.
    if (auto structure = checkEntryStructure(archive, impl->names); !structure) {
        zip_discard(archive);
        return std::unexpected(structure.error());
    }

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

std::vector<std::string> ZipArchive::entryNames() const { return impl_->names; }

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

    // Declared compressed size from the central directory, used only by the ratio check below.
    // When libzip does not report it as known, 1 stands in, which makes the ratio check stricter
    // (fails closed). A failed stat is damage, not a lookup miss.
    zip_stat_t stat;
    zip_stat_init(&stat);
    if (zip_stat_index(impl_->archive, static_cast<zip_uint64_t>(index), 0, &stat) != 0)
        return std::unexpected(DocxError::Corrupt);
    const zip_uint64_t declaredCompressed = (stat.valid & ZIP_STAT_COMP_SIZE) ? stat.comp_size : 1;
    const zip_uint64_t ratioBase = std::max<zip_uint64_t>(declaredCompressed, 1);

    zip_file_t *file = zip_fopen_index(impl_->archive, static_cast<zip_uint64_t>(index), 0);
    if (!file) {
        // Only the status code is read; libzip message text never leaves this file (T-01-04).
        // A missing or wrong password is an encrypted entry, anything else is damaged.
        const int code = zip_error_code_zip(zip_get_error(impl_->archive));
        if (code == ZIP_ER_NOPASSWD || code == ZIP_ER_WRONGPASSWD)
            return std::unexpected(DocxError::Encrypted);
        return std::unexpected(DocxError::Corrupt);
    }

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

        // Compression-ratio check (T-01-12). Once more than kCompressionCheckAfterBytes have been
        // produced, the produced size may not exceed kMaxCompressionRatio times the declared
        // compressed size. The declared value only feeds this comparison; it never widens maxBytes.
        if (total > kCompressionCheckAfterBytes &&
            static_cast<zip_uint64_t>(total) / static_cast<zip_uint64_t>(kMaxCompressionRatio) > ratioBase) {
            zip_fclose(file);
            return std::unexpected(DocxError::Limit);
        }
    }

    // A nonzero close result (for example a CRC mismatch) discards the whole entry (D-07).
    if (zip_fclose(file) != 0)
        return std::unexpected(DocxError::Corrupt);
    return out;
}

} // namespace documents
