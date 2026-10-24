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

// OLE2 (Compound File Binary) signature: old .doc files and encrypted DOCX wrappers.
constexpr std::array<char, 8> kCfbSignature{'\xD0', '\xCF', '\x11', '\xE0', '\xA1', '\xB1', '\x1A', '\xE1'};
constexpr std::string_view kEncryptedPackageName = "EncryptedPackage";

// Compares the leading bytes with an explicit length, so a short or empty input stays in
// bounds and a NUL inside the signature is compared as data, not as a terminator.
template <std::size_t N>
bool startsWithSignature(const QByteArray &bytes, const std::array<char, N> &signature) {
    return bytes.size() >= static_cast<qsizetype>(N) &&
           std::equal(signature.begin(), signature.end(), bytes.begin());
}

// UTF-16LE form of an ASCII name, built byte by byte (ASCII byte, then zero byte), so the
// encoding does not depend on the host byte order or on a Qt conversion.
QByteArray utf16LeFromAscii(std::string_view ascii) {
    QByteArray out;
    out.reserve(static_cast<qsizetype>(ascii.size() * 2));
    for (const char ch : ascii) {
        out.append(ch);
        out.append('\0');
    }
    return out;
}

} // namespace

std::expected<DocxParts, DocxError> readDocxParts(QByteArray bytes) {
    if (bytes.size() > kMaxFileBytes)
        return std::unexpected(DocxError::TooLarge);

    // Classification happens before any libzip call (T-01-05, D-06). A CFB container is
    // Encrypted when its bytes name the EncryptedPackage stream, otherwise LegacyDoc. The
    // scan is a bounds-checked search over the whole input; no offset is taken from the file.
    if (startsWithSignature(bytes, kCfbSignature)) {
        return bytes.contains(utf16LeFromAscii(kEncryptedPackageName))
                   ? std::unexpected(DocxError::Encrypted)
                   : std::unexpected(DocxError::LegacyDoc);
    }

    // Only the ZIP signatures reach ZipArchive; anything else is not a ZIP container.
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
