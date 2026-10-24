#include "documents/DocxContainer.h"
#include "documents/ZipArchive.h"
#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace documents {

namespace {

constexpr std::string_view kDocumentEntry = "word/document.xml";
constexpr std::string_view kStylesEntry = "word/styles.xml";
constexpr std::string_view kNumberingEntry = "word/numbering.xml";

// ZIP local file header "PK\x03\x04" and empty-archive record "PK\x05\x06".
constexpr std::array<char, 4> kZipLocalHeader{'P', 'K', '\x03', '\x04'};
constexpr std::array<char, 4> kZipEmptyArchive{'P', 'K', '\x05', '\x06'};

// Compares the leading bytes with an explicit length, so a short or empty input stays in
// bounds and a NUL inside the signature is compared as data, not as a terminator.
template <std::size_t N>
bool startsWithSignature(const QByteArray &bytes, const std::array<char, N> &signature) {
    return bytes.size() >= static_cast<qsizetype>(N) &&
           std::equal(signature.begin(), signature.end(), bytes.begin());
}

} // namespace

std::expected<DocxParts, DocxError> readDocxParts(QByteArray bytes) {
    if (bytes.size() > kMaxFileBytes)
        return std::unexpected(DocxError::TooLarge);

    // Classification happens before any libzip call (T-01-05). Only the ZIP signatures
    // reach ZipArchive; anything else is not a ZIP container.
    if (!startsWithSignature(bytes, kZipLocalHeader) && !startsWithSignature(bytes, kZipEmptyArchive))
        return std::unexpected(DocxError::NotZip);

    // A ZIP-prefixed input that libzip rejects is damaged, not foreign (research Pitfall 5).
    auto opened = ZipArchive::open(std::move(bytes));
    if (!opened)
        return std::unexpected(opened.error());
    const ZipArchive &archive = **opened;

    if (!archive.hasEntry(kDocumentEntry))
        return std::unexpected(DocxError::MissingPart);

    // One budget shared by the three parts in this open (D-02, as reinterpreted in
    // plan 01-02). Each read consumes what it returned.
    qint64 remaining = kMaxUnpackedBytes;
    auto readPart = [&archive, &remaining](std::string_view name) {
        auto part = archive.readEntry(name, remaining);
        if (part)
            remaining -= part->size();
        return part;
    };

    DocxParts parts;

    auto document = readPart(kDocumentEntry);
    if (!document)
        return std::unexpected(document.error());
    parts.documentXml = std::move(*document);

    // Optional parts: a missing entry is a warning with empty bytes (D-08, D-09); a present
    // entry that fails to read fails the whole read (D-07).
    if (archive.hasEntry(kStylesEntry)) {
        auto styles = readPart(kStylesEntry);
        if (!styles)
            return std::unexpected(styles.error());
        parts.stylesXml = std::move(*styles);
    } else {
        parts.warnings.append(DocxWarning::StylesPartMissing);
    }

    if (archive.hasEntry(kNumberingEntry)) {
        auto numbering = readPart(kNumberingEntry);
        if (!numbering)
            return std::unexpected(numbering.error());
        parts.numberingXml = std::move(*numbering);
    } else {
        parts.warnings.append(DocxWarning::NumberingPartMissing);
    }

    return parts;
}

} // namespace documents
