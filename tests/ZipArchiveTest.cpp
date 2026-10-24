#include "documents/ZipArchive.h"
#include "documents/DocxContainer.h"
#include <QtTest>
#include <string>

namespace {

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

// Synthetic fixtures are generated in memory; no binary files are committed (Pitfall 10).
quint32 crc32Ieee(const QByteArray &data) {
    quint32 crc = 0xFFFFFFFFu;
    for (const char ch : data) {
        crc ^= static_cast<quint8>(ch);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
    }
    return ~crc;
}

void appendLe16(QByteArray &out, quint16 value) {
    out.append(static_cast<char>(value & 0xFFu));
    out.append(static_cast<char>((value >> 8) & 0xFFu));
}

void appendLe32(QByteArray &out, quint32 value) {
    appendLe16(out, static_cast<quint16>(value & 0xFFFFu));
    appendLe16(out, static_cast<quint16>(value >> 16));
}

// Stored (method 0) ZIP: local headers, central directory, end record. Little-endian fields.
QByteArray buildStoredZip(const QList<ZipEntry> &entries) {
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
QByteArray buildDocxLike(const QByteArray &documentXml) {
    return buildStoredZip({
        ZipEntry{"[Content_Types].xml", "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Types/>"},
        ZipEntry{"word/document.xml", documentXml},
    });
}

} // namespace

class ZipArchiveTest : public QObject {
    Q_OBJECT
  private slots:
    void openAndReadExactEntry() {
        const QByteArray xml =
            QStringLiteral("<w:document><w:body><w:p><w:r><w:t>Тестовое резюме: инженер</w:t></w:r></w:p>"
                           "</w:body></w:document>")
                .toUtf8();
        auto opened = documents::ZipArchive::open(buildDocxLike(xml));
        QVERIFY2(opened.has_value(), "in-memory archive must open");
        const auto &archive = *opened.value();

        QVERIFY(archive.hasEntry("word/document.xml"));
        auto entry = archive.readEntry("word/document.xml", 1024 * 1024);
        QVERIFY2(entry.has_value(), "word/document.xml must be readable");
        QCOMPARE(entry.value(), xml);
    }

    void readEntryEnforcesByteBudget() {
        // Several 64 KiB chunks, so the limit is hit on a later chunk, not the first one.
        QByteArray body;
        for (int i = 0; i < 6000; ++i)
            body.append("<w:t>0123456789</w:t>");
        const qint64 size = body.size();
        QVERIFY(size > 64 * 1024);

        auto opened = documents::ZipArchive::open(buildDocxLike(body));
        QVERIFY2(opened.has_value(), "in-memory archive must open");
        const auto &archive = *opened.value();

        auto tooSmall = archive.readEntry("word/document.xml", size - 1);
        QVERIFY(!tooSmall.has_value());
        QCOMPARE(static_cast<int>(tooSmall.error()), static_cast<int>(documents::DocxError::Limit));

        auto exact = archive.readEntry("word/document.xml", size);
        QVERIFY2(exact.has_value(), "a budget equal to the entry size must succeed");
        QCOMPARE(exact.value(), body);
    }

    void exactNameMissingIsMissingPart() {
        const QByteArray xml = QByteArrayLiteral("<w:document/>");
        auto opened = documents::ZipArchive::open(buildDocxLike(xml));
        QVERIFY2(opened.has_value(), "in-memory archive must open");
        const auto &archive = *opened.value();

        // Lookup is case-sensitive, so an upper-case name is a different entry.
        QVERIFY(!archive.hasEntry("WORD/document.xml"));
        auto upper = archive.readEntry("WORD/document.xml", 1024);
        QVERIFY(!upper.has_value());
        QCOMPARE(static_cast<int>(upper.error()), static_cast<int>(documents::DocxError::MissingPart));

        // An embedded NUL must not truncate the name to the real entry.
        const std::string spoofed("word/document.xml\0.bak", 22);
        QVERIFY(!archive.hasEntry(spoofed));
        auto spoofedRead = archive.readEntry(spoofed, 1024);
        QVERIFY(!spoofedRead.has_value());
        QCOMPARE(static_cast<int>(spoofedRead.error()), static_cast<int>(documents::DocxError::MissingPart));
    }

    void truncatedAndGarbageInputsAreCorrupt() {
        const QByteArray full = buildDocxLike(QByteArrayLiteral("<w:document/>"));

        // The end-of-central-directory record is cut off, so libzip cannot find the directory.
        auto fromTruncated = documents::ZipArchive::open(full.first(full.size() * 40 / 100));
        QVERIFY(!fromTruncated.has_value());
        QCOMPARE(static_cast<int>(fromTruncated.error()), static_cast<int>(documents::DocxError::Corrupt));

        auto fromText =
            documents::ZipArchive::open(QByteArrayLiteral("This is plain text, not a ZIP archive."));
        QVERIFY(!fromText.has_value());
        QCOMPARE(static_cast<int>(fromText.error()), static_cast<int>(documents::DocxError::Corrupt));
    }

    void docxPartsReadDocumentStylesAndNumbering() {
        const QByteArray doc = QStringLiteral("<w:document>Резюме</w:document>").toUtf8();
        const QByteArray styles = QStringLiteral("<w:styles>Стили</w:styles>").toUtf8();
        const QByteArray numbering = QStringLiteral("<w:numbering>Нумерация</w:numbering>").toUtf8();
        const QByteArray zip = buildStoredZip({
            ZipEntry{"word/document.xml", doc},
            ZipEntry{"word/styles.xml", styles},
            ZipEntry{"word/numbering.xml", numbering},
        });

        auto parts = documents::readDocxParts(zip);
        QVERIFY2(parts.has_value(), "a ZIP with the three parts must read");
        QCOMPARE(parts->documentXml, doc);
        QCOMPARE(parts->stylesXml, styles);
        QCOMPARE(parts->numberingXml, numbering);
        QVERIFY(parts->warnings.isEmpty());
    }

    void plainTextIsNotZip() {
        auto parts = documents::readDocxParts(QStringLiteral("Резюме: инженер, опыт 5 лет").toUtf8());
        QVERIFY(!parts.has_value());
        QCOMPARE(static_cast<int>(parts.error()), static_cast<int>(documents::DocxError::NotZip));
    }

    void missingStylesAndNumberingGiveWarnings() {
        const QByteArray doc = QByteArrayLiteral("<w:document>only body</w:document>");
        auto parts = documents::readDocxParts(buildStoredZip({ZipEntry{"word/document.xml", doc}}));
        QVERIFY2(parts.has_value(), "a ZIP with only the document part must read");
        QCOMPARE(parts->documentXml, doc);
        QVERIFY(parts->stylesXml.isEmpty());
        QVERIFY(parts->numberingXml.isEmpty());

        // One warning per missing optional part: the list size is the count (D-08, D-09).
        QCOMPARE(static_cast<int>(parts->warnings.size()), 2);
        QCOMPARE(static_cast<int>(parts->warnings.at(0)),
                 static_cast<int>(documents::DocxWarning::StylesPartMissing));
        QCOMPARE(static_cast<int>(parts->warnings.at(1)),
                 static_cast<int>(documents::DocxWarning::NumberingPartMissing));
    }
};

QTEST_GUILESS_MAIN(ZipArchiveTest)
#include "ZipArchiveTest.moc"
