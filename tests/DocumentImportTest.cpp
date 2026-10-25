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

    void warningTextsArePairwiseDistinct() {
        // Literal copies of the texts in DocumentImport.cpp (warningText), with one file name.
        // The test does not call the implementation, so a drift in either place is caught.
        const QString name = QStringLiteral("Файл.docx");
        const QStringList texts = {
            QStringLiteral(
                "В файле «%1» нет описания стилей: заголовки могут отображаться как обычный текст.")
                .arg(name),
            QStringLiteral(
                "В файле «%1» нет описания нумерации: списки могут отображаться без маркеров и номеров.")
                .arg(name),
            QStringLiteral(
                "В файле «%1» есть список с нумерацией, которую приложение показывает упрощённо: номера "
                "показаны в виде «1.», «2.».")
                .arg(name),
            QStringLiteral(
                "В файле «%1» есть рисунок или встроенный объект: он не показан, текст вокруг сохранён.")
                .arg(name),
            QStringLiteral("В файле «%1» есть текстовое поле: его содержимое не показано.").arg(name),
            QStringLiteral(
                "В файле «%1» есть элемент, который приложение не разбирает: его текст не показан.")
                .arg(name),
            QStringLiteral("В файле «%1» есть колонтитул: его текст не показан.").arg(name),
        };
        QCOMPARE(texts.size(), qsizetype{7});
        for (qsizetype i = 0; i < texts.size(); ++i)
            for (qsizetype j = i + 1; j < texts.size(); ++j)
                QVERIFY2(texts.at(i) != texts.at(j), "two warning codes share one text");
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
