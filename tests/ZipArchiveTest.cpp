#include "documents/ZipArchive.h"
#include "DocxFixtures.h"
#include "documents/DocxContainer.h"
#include <QtTest>
#include <string>

using namespace fixtures;

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

    void deflatedBombIsLimitedByRatio() {
        // Two mebibytes of zeros deflate to a few kilobytes: the ratio is far above 100 to 1.
        const QByteArray zeros(2 * 1024 * 1024, '\0');
        const auto written = writeLibzipArchive({ZipEntry{"word/document.xml", zeros}}, QByteArray());
        if (!written)
            QFAIL(qPrintable(
                QStringLiteral("libzip could not write the bomb fixture: %1").arg(written.error())));
        // Without compression the ratio check could not be exercised at all, so the fixture must be small.
        QVERIFY2(written->size() < 64 * 1024, "the bomb fixture must be deflated");

        auto opened = documents::ZipArchive::open(*written);
        QVERIFY2(opened.has_value(), "the bomb archive itself must open");
        const auto &archive = *opened.value();

        auto entry = archive.readEntry("word/document.xml", documents::kMaxUnpackedBytes);
        QVERIFY(!entry.has_value());
        QCOMPARE(static_cast<int>(entry.error()), static_cast<int>(documents::DocxError::Limit));
    }

    void storedLargeEntryIsNotRatioLimited() {
        // The same two mebibytes stored without compression: the ratio is one, so the check must not fire.
        const QByteArray zeros(2 * 1024 * 1024, '\0');
        auto opened = documents::ZipArchive::open(buildStoredZip({ZipEntry{"word/document.xml", zeros}}));
        QVERIFY2(opened.has_value(), "the stored archive must open");
        const auto &archive = *opened.value();

        auto entry = archive.readEntry("word/document.xml", documents::kMaxUnpackedBytes);
        QVERIFY2(entry.has_value(), "a stored entry of the same size must be read");
        QCOMPARE(entry.value(), zeros);
    }

    void tooManyEntriesIsLimit() {
        // 1001 entries, one more than kMaxEntries, and no document part: refused at open.
        auto opened = documents::ZipArchive::open(buildStoredZip(smallEntries(1001)));
        QVERIFY(!opened.has_value());
        QCOMPARE(static_cast<int>(opened.error()), static_cast<int>(documents::DocxError::Limit));

        auto parts = documents::readDocxParts(buildStoredZip(smallEntries(1001)));
        QVERIFY(!parts.has_value());
        QCOMPARE(static_cast<int>(parts.error()), static_cast<int>(documents::DocxError::Limit));
    }

    void exactlyMaxEntriesIsAccepted() {
        // 999 small entries plus word/document.xml: exactly kMaxEntries entries, which is allowed.
        const QByteArray doc = QByteArrayLiteral("<w:document>at the limit</w:document>");
        QList<ZipEntry> entries = smallEntries(999);
        entries.append(ZipEntry{"word/document.xml", doc});
        QCOMPARE(static_cast<qint64>(entries.size()), documents::kMaxEntries);

        auto parts = documents::readDocxParts(buildStoredZip(entries));
        QVERIFY2(parts.has_value(), "an archive with exactly kMaxEntries entries must read");
        QCOMPARE(parts->documentXml, doc);
    }

    void duplicateEntryNameIsCorrupt() {
        // Two entries named word/document.xml: a duplicate could hide which part a reader selects.
        const QByteArray zip = buildStoredZip({
            ZipEntry{"word/document.xml", QByteArrayLiteral("<w:document>first</w:document>")},
            ZipEntry{"word/document.xml", QByteArrayLiteral("<w:document>second</w:document>")},
        });
        auto parts = documents::readDocxParts(zip);
        QVERIFY(!parts.has_value());
        QCOMPARE(static_cast<int>(parts.error()), static_cast<int>(documents::DocxError::Corrupt));
    }

    void caseAndSuffixVariantsAreNotMatched() {
        // Neither an upper-case directory nor a suffixed name is the document part.
        const QByteArray zip = buildStoredZip({
            ZipEntry{"WORD/document.xml", QByteArrayLiteral("<w:document>upper</w:document>")},
            ZipEntry{"word/document.xml.bak", QByteArrayLiteral("<w:document>backup</w:document>")},
        });
        auto parts = documents::readDocxParts(zip);
        QVERIFY(!parts.has_value());
        QCOMPARE(static_cast<int>(parts.error()), static_cast<int>(documents::DocxError::MissingPart));
    }

    void budgetBoundaryIsExact() {
        const QByteArray body(4096, 'b');
        auto opened = documents::ZipArchive::open(buildStoredZip({ZipEntry{"word/document.xml", body}}));
        QVERIFY2(opened.has_value(), "in-memory archive must open");
        const auto &archive = *opened.value();

        auto exact = archive.readEntry("word/document.xml", 4096);
        QVERIFY2(exact.has_value(), "a budget equal to the entry size must succeed");
        QCOMPARE(exact.value(), body);

        auto oneShort = archive.readEntry("word/document.xml", 4095);
        QVERIFY(!oneShort.has_value());
        QCOMPARE(static_cast<int>(oneShort.error()), static_cast<int>(documents::DocxError::Limit));
    }

    void sharedBudgetAcrossParts() {
        const QByteArray document(3000, 'd');
        const QByteArray styles(2000, 's');
        const QByteArray zip = buildStoredZip({
            ZipEntry{"word/document.xml", document},
            ZipEntry{"word/styles.xml", styles},
        });

        // 3000 + 2000 bytes in one open: the styles part finds only 1096 bytes left of 4096.
        auto tight = documents::readDocxParts(zip, 4096);
        QVERIFY(!tight.has_value());
        QCOMPARE(static_cast<int>(tight.error()), static_cast<int>(documents::DocxError::Limit));

        // A budget equal to the total produced bytes is enough.
        auto exact = documents::readDocxParts(zip, 5000);
        QVERIFY2(exact.has_value(), "a budget equal to the bytes of all parts must read");
        QCOMPARE(exact->documentXml, document);
        QCOMPARE(exact->stylesXml, styles);
    }

    void declaredSizeCannotHideOverflow() {
        // 4000 real bytes stored, with both headers declaring 1000 bytes fewer. The archive opens;
        // the read must not return bytes, and the failure is Corrupt (D-07, T-01-13).
        const QByteArray body(4000, 'z');
        auto opened =
            documents::ZipArchive::open(buildStoredZip({ZipEntry{"word/document.xml", body, 1000}}));
        QVERIFY2(opened.has_value(), "the archive with a forged declared size must open");
        const auto &archive = *opened.value();

        auto forged = archive.readEntry("word/document.xml", 3500);
        QVERIFY2(!forged.has_value(), "a forged declared size must not yield bytes");
        QCOMPARE(static_cast<int>(forged.error()), static_cast<int>(documents::DocxError::Corrupt));
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

    void cfbWithEncryptedPackageIsEncrypted() {
        auto parts = documents::readDocxParts(buildCfb(true));
        QVERIFY(!parts.has_value());
        QCOMPARE(static_cast<int>(parts.error()), static_cast<int>(documents::DocxError::Encrypted));
    }

    void cfbWithoutEncryptedPackageIsLegacyDoc() {
        auto parts = documents::readDocxParts(buildCfb(false));
        QVERIFY(!parts.has_value());
        QCOMPARE(static_cast<int>(parts.error()), static_cast<int>(documents::DocxError::LegacyDoc));
    }

    void pkHeaderOnlyIsCorrupt() {
        // A ZIP local header followed by bytes that are not a central directory: libzip rejects it.
        const QByteArray bytes = QByteArray("PK\x03\x04", 4) + QByteArrayLiteral("garbage, no directory");
        auto parts = documents::readDocxParts(bytes);
        QVERIFY(!parts.has_value());
        QCOMPARE(static_cast<int>(parts.error()), static_cast<int>(documents::DocxError::Corrupt));
    }

    void zipWithoutDocumentIsMissingPart() {
        auto parts = documents::readDocxParts(
            buildStoredZip({ZipEntry{"word/styles.xml", QByteArrayLiteral("<w:styles/>")}}));
        QVERIFY(!parts.has_value());
        QCOMPARE(static_cast<int>(parts.error()), static_cast<int>(documents::DocxError::MissingPart));
    }

    void libzipEncryptedEntryIsEncrypted() {
        const auto written = writeLibzipArchive(
            {ZipEntry{"word/document.xml", QByteArrayLiteral("<w:document>secret text</w:document>")}},
            QByteArrayLiteral("word/document.xml"));
        if (!written)
            QFAIL(qPrintable(
                QStringLiteral("libzip could not write the encrypted fixture: %1").arg(written.error())));

        // The central directory is readable; the document entry needs a password it does not have.
        auto parts = documents::readDocxParts(*written);
        QVERIFY(!parts.has_value());
        QCOMPARE(static_cast<int>(parts.error()), static_cast<int>(documents::DocxError::Encrypted));
    }

    void zeroByteInputIsNotZip() {
        auto parts = documents::readDocxParts(QByteArray());
        QVERIFY(!parts.has_value());
        QCOMPARE(static_cast<int>(parts.error()), static_cast<int>(documents::DocxError::NotZip));
    }

    void inputAboveFileLimitIsTooLarge() {
        // One byte over the D-01 limit; the container refuses it before any classification.
        const QByteArray tooBig(static_cast<qsizetype>(documents::kMaxFileBytes + 1), '\0');
        auto parts = documents::readDocxParts(tooBig);
        QVERIFY(!parts.has_value());
        QCOMPARE(static_cast<int>(parts.error()), static_cast<int>(documents::DocxError::TooLarge));
    }

    void damagedDocumentPartRefusesPartialResult() {
        // The marker lives only in the stored data of word/document.xml (not in the names),
        // so searching for it locates that data without a hard-coded offset.
        const QByteArray marker = QByteArrayLiteral("RESUME-MARKER-0123456789");
        QByteArray body = QByteArrayLiteral("<w:document><w:t>");
        body.append(marker);
        body.append(QByteArrayLiteral("</w:t></w:document>"));
        QByteArray zip = buildDocxLike(body);

        const qsizetype at = zip.indexOf(marker);
        QVERIFY2(at >= 0, "the marker must be present in the stored archive");
        // Flip one bit in the middle of the stored data: the entry's CRC-32 no longer matches.
        const qsizetype flip = at + marker.size() / 2;
        zip[flip] = static_cast<char>(zip[flip] ^ 0x01);

        auto parts = documents::readDocxParts(zip);
        QVERIFY2(!parts.has_value(), "a damaged document part must not return any part (D-07)");
        QCOMPARE(static_cast<int>(parts.error()), static_cast<int>(documents::DocxError::Corrupt));
    }
};

QTEST_GUILESS_MAIN(ZipArchiveTest)
#include "ZipArchiveTest.moc"
