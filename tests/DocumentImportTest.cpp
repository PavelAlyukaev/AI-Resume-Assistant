#include "application/DocumentImport.h"
#include "DocxFixtures.h"
#include "documents/DocxModel.h"
#include "documents/DocxTypes.h"
#include "documents/ZipArchive.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QSet>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>
#include <variant>

using namespace fixtures;

namespace {

// Subdirectory with Cyrillic letters and a letter with a diaeresis (ё): the path is the case
// the QFile-based read must handle (Pitfall 2 on Windows).
QString cyrillicSubdirName() { return QStringLiteral("Резюме_ёлка"); }

// Writes bytes to a new file and reports success. Used to create the test inputs only.
bool writeBytes(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    return file.write(bytes) == bytes.size();
}

// One WordprocessingML element with its own xmlns:w declaration. readDocxFile parses the body and
// the styles (plan 02-04), so the phase-1 test parts are well formed WordprocessingML too.
QByteArray wmlElement(const QByteArray &name, const QByteArray &content) {
    return "<w:" + name + " xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">" +
           content + "</w:" + name + ">";
}

// A word/document.xml whose w:body holds bodyXml; an empty body is a valid document.
QByteArray wordDocument(const QByteArray &bodyXml = {}) {
    return wmlElement("document", "<w:body>" + bodyXml + "</w:body>");
}

// A word/document.xml with the prefixes w, r (relationship ids of w:hyperlink) and v (VML), so that a
// fixture with those elements is well formed. The body holds bodyXml.
QByteArray documentRoot(const QByteArray &bodyXml) {
    return QByteArrayLiteral(
               "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
               "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
               "xmlns:v=\"urn:schemas-microsoft-com:vml\"><w:body>") +
           bodyXml + QByteArrayLiteral("</w:body></w:document>");
}

// Every message Qt delivers while a guard is alive is kept here for the test to inspect.
QStringList &capturedMessages() {
    static QStringList messages;
    return messages;
}

void captureMessage(QtMsgType, const QMessageLogContext &, const QString &message) {
    capturedMessages().append(message);
}

// Installs captureMessage and restores the previous handler when it goes out of scope, also on an
// early return from a failed QVERIFY.
class MessageCaptureGuard {
  public:
    MessageCaptureGuard() : previous_(qInstallMessageHandler(captureMessage)) { capturedMessages().clear(); }
    ~MessageCaptureGuard() { qInstallMessageHandler(previous_); }
    MessageCaptureGuard(const MessageCaptureGuard &) = delete;
    MessageCaptureGuard &operator=(const MessageCaptureGuard &) = delete;

  private:
    QtMessageHandler previous_;
};

// Sorted listing of every entry under root: relative path, kind, size and last-modified time.
// Two listings that are equal prove that nothing was created, removed, resized or touched.
QStringList snapshotTree(const QString &root) {
    QStringList lines;
    const QDir base(root);
    QDirIterator it(root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        const QString kind = info.isDir() ? QStringLiteral("dir") : QStringLiteral("file");
        lines.append(QStringLiteral("%1|%2|%3|%4")
                         .arg(base.relativeFilePath(info.filePath()), kind, QString::number(info.size()),
                              QString::number(info.lastModified().toMSecsSinceEpoch())));
    }
    lines.sort();
    return lines;
}

// One archive per DocxWarning code, and the exact text readDocxFile gives for it. The texts are copied
// from warningText in src/application/DocumentImport.cpp, with the file name «Резюме.docx». Each archive
// gives exactly one warning. The texts are owner-pending (OQ8): this table checks them, it does not
// change them.
struct WarningCase {
    QString code;
    QByteArray archive;
    QString expected;
};

// The word/document.xml part with bodyXml, as one zip entry.
ZipEntry documentEntry(const QByteArray &bodyXml) {
    return ZipEntry{"word/document.xml", documentRoot(bodyXml)};
}

QList<WarningCase> warningCases() {
    const ZipEntry emptyStyles{"word/styles.xml", wmlElement("styles", {})};
    const ZipEntry emptyNumbering{"word/numbering.xml", wmlElement("numbering", {})};

    // A paragraph numbered with numId 1, whose level 0 is lowerLetter: shown approximately.
    const ZipEntry lowerLetterNumbering{
        "word/numbering.xml",
        wmlElement("numbering",
                   "<w:abstractNum w:abstractNumId=\"0\"><w:lvl w:ilvl=\"0\"><w:start w:val=\"1\"/>"
                   "<w:numFmt w:val=\"lowerLetter\"/><w:lvlText w:val=\"%1)\"/></w:lvl></w:abstractNum>"
                   "<w:num w:numId=\"1\"><w:abstractNumId w:val=\"0\"/></w:num>")};
    const QByteArray numberedBody = "<w:p><w:pPr><w:numPr><w:ilvl w:val=\"0\"/><w:numId w:val=\"1\"/>"
                                    "</w:numPr></w:pPr><w:r><w:t>Пункт</w:t></w:r></w:p>";

    return {
        {QStringLiteral("StylesPartMissing"), buildStoredZip({documentEntry({}), emptyNumbering}),
         QStringLiteral(
             "В файле «Резюме.docx» нет описания стилей: заголовки могут отображаться как обычный текст.")},
        {QStringLiteral("NumberingPartMissing"), buildStoredZip({documentEntry({}), emptyStyles}),
         QStringLiteral("В файле «Резюме.docx» нет описания нумерации: списки могут отображаться без "
                        "маркеров и номеров.")},
        {QStringLiteral("ListFormatApproximated"),
         buildStoredZip({documentEntry(numberedBody), emptyStyles, lowerLetterNumbering}),
         QStringLiteral(
             "В файле «Резюме.docx» есть список с нумерацией, которую приложение показывает упрощённо: "
             "номера показаны в виде «1.», «2.».")},
        {QStringLiteral("ImageSkipped"),
         buildStoredZip({documentEntry("<w:p><w:r><w:t>до</w:t></w:r><w:r><w:drawing/></w:r>"
                                       "<w:r><w:t>после</w:t></w:r></w:p>"),
                         emptyStyles, emptyNumbering}),
         QStringLiteral("В файле «Резюме.docx» есть рисунок или встроенный объект: он не показан, текст "
                        "вокруг сохранён.")},
        {QStringLiteral("TextBoxSkipped"),
         buildStoredZip({documentEntry("<w:p><w:r><w:pict><v:shape><v:textbox><w:txbxContent><w:p><w:r>"
                                       "<w:t>Поле</w:t></w:r></w:p></w:txbxContent></v:textbox></v:shape>"
                                       "</w:pict></w:r></w:p>"),
                         emptyStyles, emptyNumbering}),
         QStringLiteral("В файле «Резюме.docx» есть текстовое поле: его содержимое не показано.")},
        {QStringLiteral("OtherSkipped"),
         buildStoredZip({documentEntry("<w:p><w:r><w:footnoteReference w:id=\"1\"/></w:r></w:p>"),
                         emptyStyles, emptyNumbering}),
         QStringLiteral(
             "В файле «Резюме.docx» есть элемент, который приложение не разбирает: его текст не показан.")},
        {QStringLiteral("HeaderFooterSkipped"),
         buildStoredZip({documentEntry({}), emptyStyles, emptyNumbering,
                         ZipEntry{"word/header1.xml", wmlElement("hdr", {})}}),
         QStringLiteral("В файле «Резюме.docx» есть колонтитул: его текст не показан.")},
    };
}

} // namespace

class DocumentImportTest : public QObject {
    Q_OBJECT
  private slots:
    void readsDocxFromCyrillicDirectory() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dir = QDir(root.path()).filePath(cyrillicSubdirName());
        QVERIFY(QDir().mkpath(dir));

        const QByteArray document =
            wordDocument(QStringLiteral("<w:p><w:r><w:t>Тело резюме</w:t></w:r></w:p>").toUtf8());
        const QByteArray styles = wmlElement("styles", QStringLiteral("Стили").toUtf8());
        const QByteArray numbering = wmlElement("numbering", QStringLiteral("Нумерация").toUtf8());
        const QByteArray archive = buildStoredZip({
            ZipEntry{"word/document.xml", document},
            ZipEntry{"word/styles.xml", styles},
            ZipEntry{"word/numbering.xml", numbering},
        });
        const QString path = QDir(dir).filePath(QStringLiteral("Резюме.docx"));
        QVERIFY(writeBytes(path, archive));

        auto result = DocumentImport::readDocxFile(path);
        QVERIFY2(result.has_value(), "a valid DOCX in a Cyrillic directory must import");
        QCOMPARE(result->documentXml, document);
        QCOMPARE(result->stylesXml, styles);
        QCOMPARE(result->numberingXml, numbering);
        QVERIFY(result->warnings.isEmpty());
    }

    void readsDocumentContentFromCyrillicDirectory() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dir = QDir(root.path()).filePath(cyrillicSubdirName());
        QVERIFY(QDir().mkpath(dir));

        // Heading1 has the name "heading 1": its level comes from the style (plan 02-02).
        const QByteArray styles = wmlElement(
            "styles",
            "<w:style w:type=\"paragraph\" w:styleId=\"Heading1\"><w:name w:val=\"heading 1\"/></w:style>");
        const QByteArray body = QStringLiteral("<w:p><w:pPr><w:pStyle w:val=\"Heading1\"/></w:pPr>"
                                               "<w:r><w:t>Опыт работы</w:t></w:r></w:p>"
                                               "<w:p><w:r><w:t>Ёлкин Пётр, менеджер</w:t></w:r></w:p>")
                                    .toUtf8();
        const QByteArray archive = buildStoredZip({
            ZipEntry{"[Content_Types].xml", "<Types/>"},
            ZipEntry{"word/document.xml", wordDocument(body)},
            ZipEntry{"word/styles.xml", styles},
            ZipEntry{"word/numbering.xml", wmlElement("numbering", {})},
        });
        const QString path = QDir(dir).filePath(QStringLiteral("Резюме.docx"));
        QVERIFY(writeBytes(path, archive));

        auto result = DocumentImport::readDocxFile(path);
        QVERIFY2(result.has_value(), "a DOCX with a heading and a paragraph must import");
        QVERIFY(result->warnings.isEmpty());
        QCOMPARE(result->document.blocks.size(), qsizetype{2});

        const auto *heading = std::get_if<documents::Paragraph>(&result->document.blocks.at(0).content);
        QVERIFY2(heading != nullptr, "the first block must be a paragraph");
        QCOMPARE(heading->headingLevel, 1);
        QCOMPARE(heading->text, QStringLiteral("Опыт работы"));

        const auto *plain = std::get_if<documents::Paragraph>(&result->document.blocks.at(1).content);
        QVERIFY2(plain != nullptr, "the second block must be a paragraph");
        QCOMPARE(plain->headingLevel, 0);
        QCOMPARE(plain->text, QStringLiteral("Ёлкин Пётр, менеджер"));
    }

    void doctypeFileGivesUnsafeXmlTextNamingFileOnly() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dir = QDir(root.path()).filePath(cyrillicSubdirName());
        QVERIFY(QDir().mkpath(dir));

        // An internal entity whose value is the marker: the DOCTYPE is refused before any entity is
        // expanded, so the marker never reaches the result.
        const QByteArray document =
            QStringLiteral(
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                "<!DOCTYPE w:document [<!ENTITY ext \"MARKER-DT-4412\">]>"
                "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
                "<w:body><w:p><w:r><w:t>&ext;</w:t></w:r></w:p></w:body></w:document>")
                .toUtf8();
        const QString path = QDir(dir).filePath(QStringLiteral("Резюме.docx"));
        QVERIFY(writeBytes(path, buildStoredZip({ZipEntry{"word/document.xml", document}})));

        auto result = DocumentImport::readDocxFile(path);
        QVERIFY2(!result.has_value(), "a DOCTYPE in word/document.xml must not import");
        QCOMPARE(result.error(),
                 QStringLiteral(
                     "Файл «Резюме.docx» содержит DTD или внешние сущности XML, поэтому не открыт из "
                     "соображений безопасности. Сохраните резюме в Word как .docx заново и повторите."));
        QVERIFY(!result.error().contains(QStringLiteral("MARKER-DT-4412")));
    }

    void headerFooterAndCommentsBytesNeverRead() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dir = QDir(root.path()).filePath(cyrillicSubdirName());
        QVERIFY(QDir().mkpath(dir));

        const QByteArray headerMarker = QByteArrayLiteral("HDRMARK-A1");
        const QByteArray footerMarker = QByteArrayLiteral("FTRMARK-B2");
        const QByteArray commentsMarker = QByteArrayLiteral("CMTMARK-C3");
        QByteArray archive = buildStoredZip({
            ZipEntry{"[Content_Types].xml", "<Types/>"},
            ZipEntry{"word/document.xml",
                     wordDocument(QStringLiteral("<w:p><w:r><w:t>Тест</w:t></w:r></w:p>").toUtf8())},
            ZipEntry{"word/styles.xml", wmlElement("styles", {})},
            ZipEntry{"word/numbering.xml", wmlElement("numbering", {})},
            ZipEntry{"word/header1.xml", wmlElement("hdr", headerMarker)},
            ZipEntry{"word/footer1.xml", wmlElement("ftr", footerMarker)},
            ZipEntry{"word/comments.xml", wmlElement("comments", commentsMarker)},
        });

        // Damage each payload in place: the first byte of its marker changes and the central
        // directory is untouched. The stored CRC then no longer matches the bytes.
        for (const QByteArray &marker : {headerMarker, footerMarker, commentsMarker}) {
            const qsizetype at = archive.indexOf(marker);
            QVERIFY2(at >= 0, "the marker must be present in the archive bytes");
            QVERIFY2(archive.indexOf(marker, at + 1) < 0, "the marker must occur exactly once");
            archive[at] = 'X';
        }

        // The damage is real: the archive still opens, and a direct read of the header fails.
        auto opened = documents::ZipArchive::open(archive);
        QVERIFY2(opened.has_value(), "payload damage must not stop the archive from opening");
        auto header = (*opened)->readEntry("word/header1.xml", documents::kMaxUnpackedBytes);
        QVERIFY2(!header.has_value(), "a read of the damaged header must fail");
        QVERIFY(header.error() == documents::DocxError::Corrupt);

        // The import never reads those entries, so the damage gives warnings and no error.
        const QString name = QStringLiteral("Резюме.docx");
        const QString path = QDir(dir).filePath(name);
        QVERIFY(writeBytes(path, archive));

        auto result = DocumentImport::readDocxFile(path);
        QVERIFY2(result.has_value(), "damaged header, footer and comments parts must not fail the import");
        QCOMPARE(result->warnings.size(), qsizetype{3});
        const QString headerText =
            QStringLiteral("В файле «%1» есть колонтитул: его текст не показан.").arg(name);
        QCOMPARE(result->warnings.at(0), headerText);
        QCOMPARE(result->warnings.at(1), headerText);
        QCOMPARE(result->warnings.at(2),
                 QStringLiteral(
                     "В файле «%1» есть элемент, который приложение не разбирает: его текст не показан.")
                     .arg(name));
        for (const QString &warning : result->warnings) {
            QVERIFY(!warning.contains(QString::fromUtf8(headerMarker)));
            QVERIFY(!warning.contains(QString::fromUtf8(footerMarker)));
            QVERIFY(!warning.contains(QString::fromUtf8(commentsMarker)));
        }
    }

    void notZipMessageNamesFileOnly() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dir = QDir(root.path()).filePath(cyrillicSubdirName());
        QVERIFY(QDir().mkpath(dir));

        const QString name = QStringLiteral("Резюме.docx");
        const QString path = QDir(dir).filePath(name);
        QVERIFY(writeBytes(path, QStringLiteral("Обычный текст, а не DOCX").toUtf8()));

        auto result = DocumentImport::readDocxFile(path);
        QVERIFY2(!result.has_value(), "a plain text file must not import");
        const QString expected = QStringLiteral("Файл «%1» не является DOCX: это не ZIP-архив. "
                                                "Сохраните резюме в Word как .docx и повторите.")
                                     .arg(name);
        QCOMPARE(result.error(), expected);
        QVERIFY(result.error().contains(name));
        QVERIFY(!result.error().contains(root.path()));
        QVERIFY(!result.error().contains(dir));
        QVERIFY(!result.error().contains(cyrillicSubdirName()));
    }

    void eachFailureHasItsOwnExactText_data() {
        QTest::addColumn<QByteArray>("bytes");
        QTest::addColumn<QString>("fileName");
        QTest::addColumn<QString>("expected");

        // Expected texts are literal templates copied from the plan, not calls into the
        // implementation, so a drift in DocumentImport.cpp fails this test.
        const QString plainName = QStringLiteral("Текст.docx");
        QTest::newRow("plain text is NotZip")
            << QStringLiteral("Обычный текст резюме").toUtf8() << plainName
            << QStringLiteral("Файл «%1» не является DOCX: это не ZIP-архив. Сохраните резюме в Word как "
                              ".docx и повторите.")
                   .arg(plainName);

        const QString corruptName = QStringLiteral("Повреждённый.docx");
        QByteArray zipPrefixGarbage("PK\x03\x04", 4);
        zipPrefixGarbage.append(QByteArray(64, 'Z'));
        QTest::newRow("zip prefix with garbage is Corrupt")
            << zipPrefixGarbage << corruptName
            << QStringLiteral("Файл «%1» повреждён или обрезан. Сохраните резюме заново в Word и повторите.")
                   .arg(corruptName);

        const QString encryptedName = QStringLiteral("Шифр.docx");
        QTest::newRow("CFB with EncryptedPackage is Encrypted")
            << buildCfb(true) << encryptedName
            << QStringLiteral("Файл «%1» зашифрован. Сохраните резюме без пароля и повторите.")
                   .arg(encryptedName);

        const QString legacyName = QStringLiteral("Старый.docx");
        QTest::newRow("CFB without EncryptedPackage is LegacyDoc")
            << buildCfb(false) << legacyName
            << QStringLiteral(
                   "Файл «%1» в старом формате .doc, а не .docx. Откройте его в Word, сохраните как "
                   ".docx и повторите.")
                   .arg(legacyName);

        const QString missingName = QStringLiteral("Без_текста.docx");
        const QByteArray unrelatedOnly = buildStoredZip({
            ZipEntry{"docProps/core.xml", QStringLiteral("<cp:coreProperties/>").toUtf8()},
        });
        QTest::newRow("archive without word/document.xml is MissingPart")
            << unrelatedOnly << missingName
            << QStringLiteral(
                   "В файле «%1» не найден текст резюме. Сохраните его как .docx в Word и повторите.")
                   .arg(missingName);
    }

    void eachFailureHasItsOwnExactText() {
        QFETCH(QByteArray, bytes);
        QFETCH(QString, fileName);
        QFETCH(QString, expected);

        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dir = QDir(root.path()).filePath(cyrillicSubdirName());
        QVERIFY(QDir().mkpath(dir));
        const QString path = QDir(dir).filePath(fileName);
        QVERIFY(writeBytes(path, bytes));

        auto result = DocumentImport::readDocxFile(path);
        QVERIFY2(!result.has_value(), "this input must be refused");
        QCOMPARE(result.error(), expected);
    }

    void failureTextsArePairwiseDistinct() {
        const QString name = QStringLiteral("Файл.docx");
        const QStringList texts = {
            QStringLiteral("Файл «%1» не является DOCX: это не ZIP-архив. Сохраните резюме в Word как .docx "
                           "и повторите.")
                .arg(name),
            QStringLiteral("Файл «%1» повреждён или обрезан. Сохраните резюме заново в Word и повторите.")
                .arg(name),
            QStringLiteral("Файл «%1» зашифрован. Сохраните резюме без пароля и повторите.").arg(name),
            QStringLiteral("Файл «%1» в старом формате .doc, а не .docx. Откройте его в Word, сохраните как "
                           ".docx и повторите.")
                .arg(name),
            QStringLiteral("В файле «%1» не найден текст резюме. Сохраните его как .docx в Word и повторите.")
                .arg(name),
            QStringLiteral(
                "Файл «%1» содержит DTD или внешние сущности XML, поэтому не открыт из соображений "
                "безопасности. Сохраните резюме в Word как .docx заново и повторите.")
                .arg(name),
            QStringLiteral(
                "Файл «%1» записан в кодировке XML, которую приложение не читает (поддерживаются UTF-8 и "
                "UTF-16). Сохраните резюме в Word как .docx заново и повторите.")
                .arg(name),
        };
        QCOMPARE(texts.size(), qsizetype{7});
        for (qsizetype i = 0; i < texts.size(); ++i)
            for (qsizetype j = i + 1; j < texts.size(); ++j)
                QVERIFY2(texts.at(i) != texts.at(j), "two failure codes share one text");
    }

    void unreadableFileGivesItsOwnText() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString name = QStringLiteral("Нет_такого.docx");
        const QString path = QDir(root.path()).filePath(name);

        auto result = DocumentImport::readDocxFile(path);
        QVERIFY2(!result.has_value(), "a missing file must not import");
        QCOMPARE(result.error(),
                 QStringLiteral(
                     "Не удалось открыть файл «%1». Проверьте, что он существует и доступен для чтения.")
                     .arg(name));
    }

    void missingOptionalPartsGiveTwoRussianWarnings() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dir = QDir(root.path()).filePath(cyrillicSubdirName());
        QVERIFY(QDir().mkpath(dir));

        const QString stylesText = QStringLiteral(
            "В файле «%1» нет описания стилей: заголовки могут отображаться как обычный текст.");
        const QString numberingText = QStringLiteral(
            "В файле «%1» нет описания нумерации: списки могут отображаться без маркеров и номеров.");

        // Only word/document.xml: both optional parts are missing, so two warnings in order.
        const QString textOnlyName = QStringLiteral("Только_текст.docx");
        const QString textOnlyPath = QDir(dir).filePath(textOnlyName);
        QVERIFY(writeBytes(textOnlyPath, buildStoredZip({ZipEntry{"word/document.xml", wordDocument()}})));
        auto withWarnings = DocumentImport::readDocxFile(textOnlyPath);
        QVERIFY2(withWarnings.has_value(), "a DOCX without optional parts must still import");
        QCOMPARE(withWarnings->warnings.size(), qsizetype{2});
        QCOMPARE(withWarnings->warnings.at(0), stylesText.arg(textOnlyName));
        QCOMPARE(withWarnings->warnings.at(1), numberingText.arg(textOnlyName));

        // All three parts: no warning.
        const QString fullName = QStringLiteral("Полный.docx");
        const QString fullPath = QDir(dir).filePath(fullName);
        QVERIFY(writeBytes(fullPath, buildStoredZip({
                                         ZipEntry{"word/document.xml", wordDocument()},
                                         ZipEntry{"word/styles.xml", wmlElement("styles", {})},
                                         ZipEntry{"word/numbering.xml", wmlElement("numbering", {})},
                                     })));
        auto complete = DocumentImport::readDocxFile(fullPath);
        QVERIFY2(complete.has_value(), "a complete DOCX must import");
        QVERIFY(complete->warnings.isEmpty());
    }

    void fileAboveLimitIsTooLargeBeforeOpen() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dir = QDir(root.path()).filePath(cyrillicSubdirName());
        QVERIFY(QDir().mkpath(dir));

        // One byte above the limit: refused by its size, with the limit stated in megabytes.
        const QString name = QStringLiteral("Слишком_большой.docx");
        const QString path = QDir(dir).filePath(name);
        QVERIFY(writeBytes(path, QByteArray(documents::kMaxFileBytes + 1, '\0')));

        auto result = DocumentImport::readDocxFile(path);
        QVERIFY2(!result.has_value(), "a file above the limit must not import");
        QCOMPARE(result.error(),
                 QStringLiteral(
                     "Файл «%1» больше %2 МБ. Уменьшите файл (например, удалите изображения) и повторите.")
                     .arg(name, QStringLiteral("10")));
    }

    void fileOfExactlyLimitIsNotTooLarge() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dir = QDir(root.path()).filePath(cyrillicSubdirName());
        QVERIFY(QDir().mkpath(dir));

        // Exactly the limit is not refused by size; zero bytes are then classified by content.
        const QString name = QStringLiteral("Ровно_лимит.docx");
        const QString path = QDir(dir).filePath(name);
        QVERIFY(writeBytes(path, QByteArray(documents::kMaxFileBytes, '\0')));

        auto result = DocumentImport::readDocxFile(path);
        QVERIFY2(!result.has_value(), "zero bytes must not import");
        QCOMPARE(result.error(), QStringLiteral("Файл «%1» не является DOCX: это не ZIP-архив. Сохраните "
                                                "резюме в Word как .docx и повторите.")
                                     .arg(name));
    }

    void importWritesNothing() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dir = QDir(root.path()).filePath(cyrillicSubdirName());
        QVERIFY(QDir().mkpath(dir));

        struct Input {
            QString name;
            QByteArray bytes;
        };
        const QByteArray garbage = QByteArray("PK\x03\x04", 4) + QByteArray(64, 'Z');
        const QList<Input> inputs = {
            {QStringLiteral("Полный.docx"), buildStoredZip({
                                                ZipEntry{"word/document.xml", wordDocument()},
                                                ZipEntry{"word/styles.xml", wmlElement("styles", {})},
                                                ZipEntry{"word/numbering.xml", wmlElement("numbering", {})},
                                            })},
            {QStringLiteral("Текст.docx"), QStringLiteral("Обычный текст").toUtf8()},
            {QStringLiteral("Мусор.docx"), garbage},
            {QStringLiteral("Шифр.docx"), buildCfb(true)},
            {QStringLiteral("Старый.docx"), buildCfb(false)},
            {QStringLiteral("Чужой.docx"), buildStoredZip({ZipEntry{"docProps/core.xml", "<core/>"}})},
            {QStringLiteral("Большой.docx"), QByteArray(documents::kMaxFileBytes + 1, '\0')},
        };
        for (const Input &input : inputs)
            QVERIFY(writeBytes(QDir(dir).filePath(input.name), input.bytes));

        // NTFS updates directory timestamps lazily: a listing taken right after the inputs were
        // written can show an older directory time than a listing taken a moment later, with no
        // operation in between. The baseline is therefore the first listing that a second listing
        // confirms, so every later difference is caused by the calls below.
        QStringList before = snapshotTree(root.path());
        bool settled = false;
        for (int attempt = 0; attempt < 50 && !settled; ++attempt) {
            QThread::msleep(100);
            const QStringList again = snapshotTree(root.path());
            settled = (again == before);
            before = again;
        }
        QVERIFY2(settled, "the directory listing never settled before the calls");
        QVERIFY(!before.isEmpty());

        // Each call must leave the listing unchanged: names, kinds, sizes and times.
        for (const Input &input : inputs) {
            [[maybe_unused]] const auto result = DocumentImport::readDocxFile(QDir(dir).filePath(input.name));
            QCOMPARE(snapshotTree(root.path()), before);
        }
    }

    void bombFromCyrillicDirectoryIsLimitWithFileNameOnly() {
        // A deflated two-mebibyte bomb: small on disk, refused by the ratio check during the read.
        const QByteArray zeros(qsizetype{2} * 1024 * 1024, '\0');
        const auto written = writeLibzipArchive({ZipEntry{"word/document.xml", zeros}}, QByteArray());
        if (!written)
            QFAIL(qPrintable(
                QStringLiteral("libzip could not write the bomb fixture: %1").arg(written.error())));
        QVERIFY2(written->size() < qsizetype{64} * 1024, "the bomb fixture must be deflated");

        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dir = QDir(root.path()).filePath(cyrillicSubdirName());
        QVERIFY(QDir().mkpath(dir));
        const QString name = QStringLiteral("Бомба.docx");
        const QString path = QDir(dir).filePath(name);
        QVERIFY(writeBytes(path, *written));

        auto result = DocumentImport::readDocxFile(path);
        QVERIFY2(!result.has_value(), "a zip bomb must not import");
        QCOMPARE(result.error(),
                 QStringLiteral("Файл «%1» отклонён: после распаковки содержимое слишком большое или состоит "
                                "из слишком многих частей. Сохраните резюме заново в Word и повторите.")
                     .arg(name));
        QVERIFY(!result.error().contains(root.path()));
        QVERIFY(!result.error().contains(dir));
        QVERIFY(!result.error().contains(cyrillicSubdirName()));
    }

    void headerAndFooterPartsCountedAsWarnings() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dir = QDir(root.path()).filePath(cyrillicSubdirName());
        QVERIFY(QDir().mkpath(dir));

        // Three parts count: header1, footer1 and footer2. The other names must not count: headerNotes
        // (no digits), WORD/header3 (other case) and commentsExtended (not the exact comments name).
        const QString name = QStringLiteral("Резюме.docx");
        const QString path = QDir(dir).filePath(name);
        QVERIFY(writeBytes(
            path,
            buildStoredZip({
                ZipEntry{"[Content_Types].xml", "<Types/>"},
                ZipEntry{"word/document.xml",
                         wordDocument(QStringLiteral("<w:p><w:r><w:t>Текст</w:t></w:r></w:p>").toUtf8())},
                ZipEntry{"word/styles.xml", wmlElement("styles", {})},
                ZipEntry{"word/numbering.xml", wmlElement("numbering", {})},
                ZipEntry{"word/header1.xml", wmlElement("hdr", {})},
                ZipEntry{"word/footer1.xml", wmlElement("ftr", {})},
                ZipEntry{"word/footer2.xml", wmlElement("ftr", {})},
                ZipEntry{"word/headerNotes.xml", wmlElement("hdr", {})},
                ZipEntry{"WORD/header3.xml", wmlElement("hdr", {})},
                ZipEntry{"word/commentsExtended.xml", wmlElement("commentsEx", {})},
            })));

        auto result = DocumentImport::readDocxFile(path);
        QVERIFY2(result.has_value(), "a DOCX with header and footer parts must import");
        const QString headerText =
            QStringLiteral("В файле «Резюме.docx» есть колонтитул: его текст не показан.");
        QCOMPARE(result->warnings.size(), qsizetype{3});
        for (const QString &warning : result->warnings)
            QCOMPARE(warning, headerText);
    }

    void overTextLimitGivesLimitText() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dir = QDir(root.path()).filePath(cyrillicSubdirName());
        QVERIFY(QDir().mkpath(dir));

        // One paragraph of kMaxTextChars + 1 Cyrillic letters: one unit over the text limit (D-03).
        const QString longText(static_cast<qsizetype>(documents::kMaxTextChars + 1), QChar(0x044F));
        const QByteArray body = QByteArrayLiteral("<w:p><w:r><w:t>") + longText.toUtf8() +
                                QByteArrayLiteral("</w:t></w:r></w:p>");
        const QString name = QStringLiteral("Резюме.docx");
        const QString path = QDir(dir).filePath(name);
        QVERIFY(writeBytes(path, buildStoredZip({ZipEntry{"word/document.xml", wordDocument(body)}})));

        auto result = DocumentImport::readDocxFile(path);
        QVERIFY2(!result.has_value(), "text above kMaxTextChars must be refused");
        QCOMPARE(
            result.error(),
            QStringLiteral("Файл «Резюме.docx» отклонён: после распаковки содержимое слишком большое или "
                           "состоит из слишком многих частей. Сохраните резюме заново в Word и повторите."));
    }

    void listsKeepNestingThroughReadDocxFile() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dir = QDir(root.path()).filePath(cyrillicSubdirName());
        QVERIFY(QDir().mkpath(dir));

        // abstractNum 0 is bullets. abstractNum 1 is decimal; its level 1 is "%2.", which is shown
        // approximately (OQ5), so one ListFormatApproximated is expected, for numId 2 only.
        const QByteArray numbering =
            wmlElement("numbering", "<w:abstractNum w:abstractNumId=\"0\">"
                                    "<w:lvl w:ilvl=\"0\"><w:start w:val=\"1\"/><w:numFmt "
                                    "w:val=\"bullet\"/><w:lvlText w:val=\"-\"/></w:lvl>"
                                    "<w:lvl w:ilvl=\"1\"><w:start w:val=\"1\"/><w:numFmt "
                                    "w:val=\"bullet\"/><w:lvlText w:val=\"-\"/></w:lvl>"
                                    "</w:abstractNum>"
                                    "<w:abstractNum w:abstractNumId=\"1\">"
                                    "<w:lvl w:ilvl=\"0\"><w:start w:val=\"1\"/><w:numFmt "
                                    "w:val=\"decimal\"/><w:lvlText w:val=\"%1.\"/></w:lvl>"
                                    "<w:lvl w:ilvl=\"1\"><w:start w:val=\"1\"/><w:numFmt "
                                    "w:val=\"decimal\"/><w:lvlText w:val=\"%2.\"/></w:lvl>"
                                    "</w:abstractNum>"
                                    "<w:num w:numId=\"1\"><w:abstractNumId w:val=\"0\"/></w:num>"
                                    "<w:num w:numId=\"2\"><w:abstractNumId w:val=\"1\"/></w:num>");

        const auto listParagraph = [](const QString &text, int numId, int ilvl) {
            return QStringLiteral(
                       "<w:p><w:pPr><w:numPr><w:ilvl w:val=\"%1\"/><w:numId w:val=\"%2\"/></w:numPr>"
                       "</w:pPr><w:r><w:t>%3</w:t></w:r></w:p>")
                .arg(ilvl)
                .arg(numId)
                .arg(text)
                .toUtf8();
        };
        const QByteArray body = listParagraph(QStringLiteral("Навыки"), 1, 0) +
                                listParagraph(QStringLiteral("Python"), 1, 1) +
                                listParagraph(QStringLiteral("Первый этап"), 2, 0) +
                                listParagraph(QStringLiteral("Второй этап"), 2, 0) +
                                listParagraph(QStringLiteral("Подэтап"), 2, 1) +
                                listParagraph(QStringLiteral("Третий этап"), 2, 0) +
                                QStringLiteral("<w:p><w:r><w:t>Без списка</w:t></w:r></w:p>").toUtf8();

        const QByteArray archive = buildStoredZip({
            ZipEntry{"[Content_Types].xml", "<Types/>"},
            ZipEntry{"word/document.xml", documentRoot(body)},
            ZipEntry{"word/styles.xml", wmlElement("styles", {})},
            ZipEntry{"word/numbering.xml", numbering},
        });
        const QString path = QDir(dir).filePath(QStringLiteral("Резюме.docx"));
        QVERIFY(writeBytes(path, archive));

        auto result = DocumentImport::readDocxFile(path);
        QVERIFY2(result.has_value(), "a DOCX with nested lists must import");
        QCOMPARE(result->document.blocks.size(), qsizetype{7});

        const QString bullet(QChar(0x2022));
        struct ExpectedItem {
            QString text;
            int listLevel;
            bool expectBullet;
            QString marker;
        };
        const QList<ExpectedItem> expected = {
            {QStringLiteral("Навыки"), 0, true, bullet},
            {QStringLiteral("Python"), 1, true, bullet},
            {QStringLiteral("Первый этап"), 0, false, QStringLiteral("1.")},
            {QStringLiteral("Второй этап"), 0, false, QStringLiteral("2.")},
            {QStringLiteral("Подэтап"), 1, false, QStringLiteral("1.")},
            {QStringLiteral("Третий этап"), 0, false, QStringLiteral("3.")},
            {QStringLiteral("Без списка"), -1, false, QString()},
        };
        for (qsizetype i = 0; i < expected.size(); ++i) {
            const auto *paragraph = std::get_if<documents::Paragraph>(&result->document.blocks.at(i).content);
            QVERIFY2(paragraph != nullptr, "every block of the list fixture is a paragraph");
            const ExpectedItem &want = expected.at(i);
            QCOMPARE(paragraph->text, want.text);
            QCOMPARE(paragraph->listLevel, want.listLevel);
            QCOMPARE(paragraph->listMarker, want.marker);
            if (want.listLevel >= 0)
                QCOMPARE(paragraph->listKind == documents::ListKind::Bullet, want.expectBullet);
        }

        // Intentional: this records the OQ5 false positive. Remove it together with the code change.
        QCOMPARE(result->warnings, QStringList{QStringLiteral(
                                       "В файле «Резюме.docx» есть список с нумерацией, которую приложение "
                                       "показывает упрощённо: номера показаны в виде «1.», «2.».")});
    }

    void tablesKeepRowsThroughReadDocxFile() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dir = QDir(root.path()).filePath(cyrillicSubdirName());
        QVERIFY(QDir().mkpath(dir));

        // The first child of w:tbl is an empty w:tblPr, as Word writes it. Row 2 starts a vertical merge
        // (restart); row 3 holds its continuation (no value), which is not shown.
        const QByteArray body =
            QStringLiteral("<w:p><w:r><w:t>Перед таблицей</w:t></w:r></w:p>"
                           "<w:tbl><w:tblPr/>"
                           "<w:tblGrid><w:gridCol w:w=\"2000\"/><w:gridCol w:w=\"2000\"/></w:tblGrid>"
                           "<w:tr><w:tc><w:tcPr><w:gridSpan w:val=\"2\"/></w:tcPr>"
                           "<w:p><w:r><w:t>Шапка</w:t></w:r></w:p></w:tc></w:tr>"
                           "<w:tr><w:tc><w:p><w:r><w:t>Левая</w:t></w:r></w:p></w:tc>"
                           "<w:tc><w:p/></w:tc></w:tr>"
                           "<w:tr><w:tc><w:tcPr><w:vMerge w:val=\"restart\"/></w:tcPr>"
                           "<w:p><w:r><w:t>Объединённая</w:t></w:r></w:p></w:tc>"
                           "<w:tc><w:p><w:r><w:t>Правая</w:t></w:r></w:p></w:tc></w:tr>"
                           "<w:tr><w:tc><w:tcPr><w:vMerge/></w:tcPr><w:p/></w:tc>"
                           "<w:tc><w:p><w:r><w:t>Ниже</w:t></w:r></w:p></w:tc></w:tr>"
                           "</w:tbl>"
                           "<w:p><w:r><w:t>После таблицы</w:t></w:r></w:p>")
                .toUtf8();
        const QByteArray archive = buildStoredZip({
            ZipEntry{"[Content_Types].xml", "<Types/>"},
            ZipEntry{"word/document.xml", documentRoot(body)},
            ZipEntry{"word/styles.xml", wmlElement("styles", {})},
            ZipEntry{"word/numbering.xml", wmlElement("numbering", {})},
        });
        const QString path = QDir(dir).filePath(QStringLiteral("Резюме.docx"));
        QVERIFY(writeBytes(path, archive));

        auto result = DocumentImport::readDocxFile(path);
        QVERIFY2(result.has_value(), "a DOCX with a table between two paragraphs must import");
        QVERIFY(result->warnings.isEmpty());
        QCOMPARE(result->document.blocks.size(), qsizetype{3});

        const auto *before = std::get_if<documents::Paragraph>(&result->document.blocks.at(0).content);
        QVERIFY2(before != nullptr, "the first block must be a paragraph");
        QCOMPARE(before->text, QStringLiteral("Перед таблицей"));
        const auto *after = std::get_if<documents::Paragraph>(&result->document.blocks.at(2).content);
        QVERIFY2(after != nullptr, "the last block must be a paragraph");
        QCOMPARE(after->text, QStringLiteral("После таблицы"));

        const auto *table = std::get_if<documents::Table>(&result->document.blocks.at(1).content);
        QVERIFY2(table != nullptr, "the middle block must be a table");
        QCOMPARE(table->gridColumns, 2);
        QCOMPARE(table->rows.size(), qsizetype{4});

        struct ExpectedCell {
            int startColumn;
            int columnSpan;
            int rowSpan;
            QString text;
        };
        const QList<QList<ExpectedCell>> expectedRows = {
            {{0, 2, 1, QStringLiteral("Шапка")}},
            {{0, 1, 1, QStringLiteral("Левая")}, {1, 1, 1, QString()}},
            {{0, 1, 2, QStringLiteral("Объединённая")}, {1, 1, 1, QStringLiteral("Правая")}},
            {{1, 1, 1, QStringLiteral("Ниже")}},
        };
        for (qsizetype r = 0; r < expectedRows.size(); ++r) {
            const auto &cells = table->rows.at(r).cells;
            QCOMPARE(cells.size(), expectedRows.at(r).size());
            for (qsizetype c = 0; c < cells.size(); ++c) {
                const documents::TableCell &cell = cells.at(c);
                const ExpectedCell &want = expectedRows.at(r).at(c);
                QCOMPARE(cell.startColumn, want.startColumn);
                QCOMPARE(cell.columnSpan, want.columnSpan);
                QCOMPARE(cell.rowSpan, want.rowSpan);
                QCOMPARE(cell.blocks.size(), qsizetype{1});
                const auto *paragraph = std::get_if<documents::Paragraph>(&cell.blocks.at(0).content);
                QVERIFY2(paragraph != nullptr, "a cell of the table fixture holds a paragraph");
                QCOMPARE(paragraph->text, want.text);
            }
        }
    }

    void hyperlinkAndInsertionGiveTextThroughReadDocxFile() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dir = QDir(root.path()).filePath(cyrillicSubdirName());
        QVERIFY(QDir().mkpath(dir));

        // The link text is not the address: a test that printed the address would fail. The deleted run
        // holds its text in w:delText; the insertion and the deletion carry w:id and w:author, as Word
        // writes.
        const QByteArray body =
            QStringLiteral(
                "<w:p><w:r><w:t xml:space=\"preserve\">Портфолио: </w:t></w:r>"
                "<w:hyperlink r:id=\"rId7\"><w:r><w:t>github.com/ivanov</w:t></w:r></w:hyperlink>"
                "<w:ins w:id=\"1\" w:author=\"Тест\"><w:r><w:t xml:space=\"preserve\"> "
                "вставка</w:t></w:r></w:ins>"
                "<w:del w:id=\"2\" w:author=\"Тест\"><w:r><w:delText>удалено</w:delText></w:r></w:del></w:p>")
                .toUtf8();
        const QByteArray rels =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            "<Relationship Id=\"rId7\" "
            "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink\" "
            "Target=\"https://example.org/skrytyi-adres\" TargetMode=\"External\"/>"
            "</Relationships>";
        const QByteArray archive = buildStoredZip({
            ZipEntry{"[Content_Types].xml", "<Types/>"},
            ZipEntry{"word/_rels/document.xml.rels", rels},
            ZipEntry{"word/document.xml", documentRoot(body)},
            ZipEntry{"word/styles.xml", wmlElement("styles", {})},
            ZipEntry{"word/numbering.xml", wmlElement("numbering", {})},
        });
        const QString path = QDir(dir).filePath(QStringLiteral("Резюме.docx"));
        QVERIFY(writeBytes(path, archive));

        auto result = DocumentImport::readDocxFile(path);
        QVERIFY2(result.has_value(), "a DOCX with a hyperlink, an insertion and a deletion must import");
        QVERIFY(result->warnings.isEmpty());
        QCOMPARE(result->document.blocks.size(), qsizetype{1});
        const auto *only = std::get_if<documents::Paragraph>(&result->document.blocks.at(0).content);
        QVERIFY2(only != nullptr, "the block is a paragraph");
        QCOMPARE(only->text, QStringLiteral("Портфолио: github.com/ivanov вставка"));
    }

    void warningTextsMatchEachCode_data() {
        QTest::addColumn<QString>("code");
        QTest::addColumn<QByteArray>("archive");
        QTest::addColumn<QString>("expected");
        for (const WarningCase &warningCase : warningCases())
            QTest::newRow(qPrintable(warningCase.code))
                << warningCase.code << warningCase.archive << warningCase.expected;
    }

    void warningTextsMatchEachCode() {
        QFETCH(QString, code);
        QFETCH(QByteArray, archive);
        QFETCH(QString, expected);
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dir = QDir(root.path()).filePath(cyrillicSubdirName());
        QVERIFY(QDir().mkpath(dir));
        const QString path = QDir(dir).filePath(QStringLiteral("Резюме.docx"));
        QVERIFY(writeBytes(path, archive));

        auto result = DocumentImport::readDocxFile(path);
        QVERIFY2(result.has_value(), qPrintable(code));
        // Exactly one warning, and its text is the text the implementation gives for this code.
        QCOMPARE(result->warnings, QStringList{expected});
    }

    void warningTextsOfRealRunsArePairwiseDistinct() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dir = QDir(root.path()).filePath(cyrillicSubdirName());
        QVERIFY(QDir().mkpath(dir));
        const QString path = QDir(dir).filePath(QStringLiteral("Резюме.docx"));

        // The texts are taken from the results of readDocxFile, not from copies in this file.
        const QList<WarningCase> cases = warningCases();
        QSet<QString> seen;
        for (const WarningCase &warningCase : cases) {
            QVERIFY(writeBytes(path, warningCase.archive));
            auto result = DocumentImport::readDocxFile(path);
            QVERIFY2(result.has_value(), qPrintable(warningCase.code));
            QCOMPARE(result->warnings.size(), qsizetype{1});
            seen.insert(result->warnings.first());
        }
        QCOMPARE(cases.size(), qsizetype{7});
        QCOMPARE(seen.size(), qsizetype{7});
    }

    void messagesAndLogsCarryNoMarker() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dir = QDir(root.path()).filePath(cyrillicSubdirName());
        QVERIFY(QDir().mkpath(dir));

        // ASCII marker, so it is found in the raw bytes whatever the file's encoding is.
        const QByteArray marker = QByteArrayLiteral("MARKER-SAFE05");
        const QByteArray xmlDeclaration = QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
        const QByteArray plainDocument =
            wordDocument(QByteArrayLiteral("<w:p><w:r><w:t>MARKER-SAFE05</w:t></w:r></w:p>"));

        struct Input {
            QString name;
            QByteArray bytes;
        };
        const QList<Input> inputs = {
            // 1. DOCTYPE with an internal entity whose value is the marker.
            {QStringLiteral("Сущность.docx"),
             buildStoredZip({ZipEntry{
                 "word/document.xml",
                 xmlDeclaration + QByteArrayLiteral("<!DOCTYPE w:document [<!ENTITY e \"MARKER-SAFE05\">]>") +
                     wmlElement("document", "<w:body><w:p><w:r><w:t>&e;</w:t></w:r></w:p></w:body>")}})},
            // 2. An undeclared entity next to the marker in text.
            {QStringLiteral("Неизвестная.docx"),
             buildStoredZip({ZipEntry{"word/document.xml",
                                      wordDocument(QByteArrayLiteral(
                                          "<w:p><w:r><w:t>MARKER-SAFE05 &undeclared;</w:t></w:r></w:p>"))}})},
            // 3. XML cut off right after the marker.
            {QStringLiteral("Обрезанный.docx"),
             buildStoredZip(
                 {ZipEntry{"word/document.xml",
                           QByteArrayLiteral("<w:document xmlns:w=\"http://schemas.openxmlformats.org/"
                                             "wordprocessingml/2006/main\"><w:body><w:p><w:r><w:t>"
                                             "MARKER-SAFE05")}})},
            // 4. A declared encoding that is not supported, with the marker in the text.
            {QStringLiteral("Кодировка.docx"),
             buildStoredZip({ZipEntry{"word/document.xml",
                                      QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"windows-1251\"?>") +
                                          plainDocument}})},
            // 5. DOCTYPE in word/styles.xml; the body is valid and carries the marker as well.
            {QStringLiteral("Стили.docx"),
             buildStoredZip({
                 ZipEntry{"word/document.xml", plainDocument},
                 ZipEntry{"word/styles.xml",
                          xmlDeclaration +
                              QByteArrayLiteral("<!DOCTYPE w:styles [<!ENTITY e \"MARKER-SAFE05\">]>") +
                              wmlElement("styles", "&e;")},
             })},
        };

        MessageCaptureGuard guard;
        for (const Input &input : inputs) {
            const QString path = QDir(dir).filePath(input.name);
            QVERIFY(writeBytes(path, input.bytes));
            capturedMessages().clear();

            auto result = DocumentImport::readDocxFile(path);
            QVERIFY2(!result.has_value(), "every planted input must be refused");
            QVERIFY2(!result.error().contains(QString::fromUtf8(marker)),
                     "an error text must not carry document content");
            for (const QString &message : capturedMessages())
                QVERIFY2(!message.contains(QString::fromUtf8(marker)),
                         "a logged message must not carry document content");
        }
    }

    void messagesLeakNeitherPathNorContent() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dir = QDir(root.path()).filePath(cyrillicSubdirName());
        QVERIFY(QDir().mkpath(dir));

        // Marker: capital Cyrillic letters and a digit run. It is planted in document bodies and
        // in an unrelated archive entry; no message may repeat it.
        const QString marker = QStringLiteral("СЕКРЕТНЫЙ-МАРКЕР-90817263");
        const QByteArray markerBytes = marker.toUtf8();
        const QByteArray garbage = QByteArray("PK\x03\x04", 4) + QByteArray(64, 'Z');

        struct Input {
            QString name;
            QByteArray bytes;
        };
        const QList<Input> inputs = {
            {QStringLiteral("Текст.docx"), QStringLiteral("Обычный текст ").toUtf8() + markerBytes},
            {QStringLiteral("Мусор.docx"), garbage + markerBytes},
            {QStringLiteral("Шифр.docx"), buildCfb(true) + markerBytes},
            {QStringLiteral("Старый.docx"), buildCfb(false) + markerBytes},
            {QStringLiteral("Без_текста.docx"), buildStoredZip({ZipEntry{"docProps/core.xml", markerBytes}})},
        };
        for (const Input &input : inputs) {
            const QString path = QDir(dir).filePath(input.name);
            QVERIFY(writeBytes(path, input.bytes));

            auto result = DocumentImport::readDocxFile(path);
            QVERIFY2(!result.has_value(), "every planted input must be refused");
            const QString &message = result.error();
            QVERIFY2(!message.contains(marker), "a message must not carry document content");
            QVERIFY2(!message.contains(root.path()), "a message must not carry the full directory path");
            QVERIFY2(!message.contains(dir), "a message must not carry the full file path");
            QVERIFY2(!message.contains(cyrillicSubdirName()), "a message must not carry the directory name");
        }
    }
};

QTEST_GUILESS_MAIN(DocumentImportTest)
#include "DocumentImportTest.moc"
