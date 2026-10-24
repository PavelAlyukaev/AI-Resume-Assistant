#pragma once

#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QList>
#include <QString>
#include <QTemporaryDir>
#include <expected>
#include <string_view>
#include <zip.h>

// Shared test builders. Every fixture is generated in code (a byte array or a scratch
// directory); no binary fixture is committed (Pitfall 10).
namespace fixtures {

struct ZipEntry {
    QByteArray name;
    QByteArray data;
};

struct CentralRecord {
    quint32 crc;
    quint32 size;
    quint32 offset;
    QByteArray name;
};

inline quint32 crc32Ieee(const QByteArray &data) {
    quint32 crc = 0xFFFFFFFFu;
    for (const char ch : data) {
        crc ^= static_cast<quint8>(ch);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
    }
    return ~crc;
}

inline void appendLe16(QByteArray &out, quint16 value) {
    out.append(static_cast<char>(value & 0xFFu));
    out.append(static_cast<char>((value >> 8) & 0xFFu));
}

inline void appendLe32(QByteArray &out, quint32 value) {
    appendLe16(out, static_cast<quint16>(value & 0xFFFFu));
    appendLe16(out, static_cast<quint16>(value >> 16));
}

// Stored (method 0) ZIP: local headers, central directory, end record. Little-endian fields.
inline QByteArray buildStoredZip(const QList<ZipEntry> &entries) {
    QByteArray out;
    QList<CentralRecord> central;
    for (const ZipEntry &entry : entries) {
        const quint32 crc = crc32Ieee(entry.data);
        const quint32 size = static_cast<quint32>(entry.data.size());
        const quint32 offset = static_cast<quint32>(out.size());
        appendLe32(out, 0x04034b50u); // local file header signature
        appendLe16(out, 20);          // version needed
        appendLe16(out, 0);           // flags
        appendLe16(out, 0);           // method: stored
        appendLe16(out, 0);           // mod time
        appendLe16(out, 0x21);        // mod date (1980-01-01)
        appendLe32(out, crc);
        appendLe32(out, size); // compressed size
        appendLe32(out, size); // uncompressed size
        appendLe16(out, static_cast<quint16>(entry.name.size()));
        appendLe16(out, 0); // extra length
        out.append(entry.name);
        out.append(entry.data);
        central.append({crc, size, offset, entry.name});
    }

    const quint32 centralOffset = static_cast<quint32>(out.size());
    for (const CentralRecord &record : central) {
        appendLe32(out, 0x02014b50u); // central directory header signature
        appendLe16(out, 20);          // version made by
        appendLe16(out, 20);          // version needed
        appendLe16(out, 0);           // flags
        appendLe16(out, 0);           // method: stored
        appendLe16(out, 0);           // mod time
        appendLe16(out, 0x21);        // mod date
        appendLe32(out, record.crc);
        appendLe32(out, record.size);
        appendLe32(out, record.size);
        appendLe16(out, static_cast<quint16>(record.name.size()));
        appendLe16(out, 0); // extra length
        appendLe16(out, 0); // comment length
        appendLe16(out, 0); // disk number start
        appendLe16(out, 0); // internal attributes
        appendLe32(out, 0); // external attributes
        appendLe32(out, record.offset);
        out.append(record.name);
    }
    const quint32 centralSize = static_cast<quint32>(out.size()) - centralOffset;

    appendLe32(out, 0x06054b50u); // end of central directory signature
    appendLe16(out, 0);           // disk number
    appendLe16(out, 0);           // disk with central directory
    appendLe16(out, static_cast<quint16>(central.size()));
    appendLe16(out, static_cast<quint16>(central.size()));
    appendLe32(out, centralSize);
    appendLe32(out, centralOffset);
    appendLe16(out, 0); // comment length
    return out;
}

// A DOCX-shaped archive: [Content_Types].xml plus word/document.xml with the given body.
inline QByteArray buildDocxLike(const QByteArray &documentXml) {
    return buildStoredZip({
        ZipEntry{"[Content_Types].xml", "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Types/>"},
        ZipEntry{"word/document.xml", documentXml},
    });
}

// Appends the UTF-16LE form of an ASCII name: each character, then a zero byte. Built by
// hand so the encoding does not depend on host byte order or on a Qt conversion.
inline void appendUtf16Le(QByteArray &out, std::string_view ascii) {
    for (const char ch : ascii) {
        out.append(ch);
        out.append('\0');
    }
}

// OLE2 (CFB) container: the eight-byte signature, a 512-byte header, then a directory
// region that names the WordDocument stream and, when requested, EncryptedPackage.
inline QByteArray buildCfb(bool withEncryptedPackage) {
    const char signature[8] = {'\xD0', '\xCF', '\x11', '\xE0', '\xA1', '\xB1', '\x1A', '\xE1'};
    QByteArray out(signature, 8);
    out.append(QByteArray(512 - 8, '\0'));
    appendUtf16Le(out, "WordDocument");
    if (withEncryptedPackage)
        appendUtf16Le(out, "EncryptedPackage");
    out.append(QByteArray(512, '\0'));
    return out;
}

// Writes a ZIP with libzip into a QTemporaryDir under the user temp directory (an ASCII
// path on the test machine, so the narrow zip_open path is safe) and returns the file bytes.
// The entry named by encryptedName, when present, is set to AES-256 with a fixed test
// password. On failure the error is a libzip-derived text for the test message.
// Ownership: each entry buffer is borrowed (freep 0) and must outlive zip_close, which it does
// because entries is a parameter of this function.
inline std::expected<QByteArray, QString> writeLibzipArchive(const QList<ZipEntry> &entries,
                                                             const QByteArray &encryptedName) {
    QTemporaryDir dir;
    if (!dir.isValid())
        return std::unexpected(QStringLiteral("QTemporaryDir is not valid"));
    const QString path = dir.filePath(QStringLiteral("fixture.docx"));
    const QByteArray narrowPath = QFile::encodeName(path);

    int openError = 0;
    zip_t *archive = zip_open(narrowPath.constData(), ZIP_CREATE | ZIP_TRUNCATE, &openError);
    if (!archive) {
        zip_error_t error;
        zip_error_init_with_code(&error, openError);
        const QString text = QString::fromUtf8(zip_error_strerror(&error));
        zip_error_fini(&error);
        return std::unexpected(QStringLiteral("zip_open failed: %1").arg(text));
    }

    for (const ZipEntry &entry : entries) {
        zip_source_t *source = zip_source_buffer(archive, entry.data.constData(),
                                                 static_cast<zip_uint64_t>(entry.data.size()), 0);
        if (!source) {
            const QString text = QString::fromUtf8(zip_strerror(archive));
            zip_discard(archive);
            return std::unexpected(QStringLiteral("zip_source_buffer failed: %1").arg(text));
        }
        const zip_int64_t index = zip_file_add(archive, entry.name.constData(), source, ZIP_FL_ENC_UTF_8);
        if (index < 0) {
            const QString text = QString::fromUtf8(zip_strerror(archive));
            zip_discard(archive);
            return std::unexpected(QStringLiteral("zip_file_add failed: %1").arg(text));
        }
        if (entry.name == encryptedName) {
            const int rc = zip_file_set_encryption(archive, static_cast<zip_uint64_t>(index), ZIP_EM_AES_256,
                                                   "fixture-password");
            if (rc != 0) {
                const QString text = QString::fromUtf8(zip_strerror(archive));
                zip_discard(archive);
                return std::unexpected(QStringLiteral("zip_file_set_encryption failed: %1").arg(text));
            }
        }
    }

    // After a failed zip_close the handle is not touched again: libzip's ownership of it on
    // that path is not a documented contract, so this test-only writer does not guess.
    if (zip_close(archive) != 0)
        return std::unexpected(QStringLiteral("zip_close failed"));

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::unexpected(QStringLiteral("cannot reopen the libzip output"));
    return file.readAll();
}

} // namespace fixtures
