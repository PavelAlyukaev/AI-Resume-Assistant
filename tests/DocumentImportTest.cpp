#include "application/DocumentImport.h"
#include "DocxFixtures.h"
#include "documents/DocxTypes.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

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

        const QByteArray document = QStringLiteral("<w:document>Тело резюме</w:document>").toUtf8();
        const QByteArray styles = QStringLiteral("<w:styles>Стили</w:styles>").toUtf8();
        const QByteArray numbering = QStringLiteral("<w:numbering>Нумерация</w:numbering>").toUtf8();
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
        };
        QCOMPARE(texts.size(), qsizetype{5});
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
        QVERIFY(writeBytes(
            textOnlyPath,
            buildStoredZip({ZipEntry{"word/document.xml", QStringLiteral("<w:document/>").toUtf8()}})));
        auto withWarnings = DocumentImport::readDocxFile(textOnlyPath);
        QVERIFY2(withWarnings.has_value(), "a DOCX without optional parts must still import");
        QCOMPARE(withWarnings->warnings.size(), qsizetype{2});
        QCOMPARE(withWarnings->warnings.at(0), stylesText.arg(textOnlyName));
        QCOMPARE(withWarnings->warnings.at(1), numberingText.arg(textOnlyName));

        // All three parts: no warning.
        const QString fullName = QStringLiteral("Полный.docx");
        const QString fullPath = QDir(dir).filePath(fullName);
        QVERIFY(writeBytes(fullPath,
                           buildStoredZip({
                               ZipEntry{"word/document.xml", QStringLiteral("<w:document/>").toUtf8()},
                               ZipEntry{"word/styles.xml", QStringLiteral("<w:styles/>").toUtf8()},
                               ZipEntry{"word/numbering.xml", QStringLiteral("<w:numbering/>").toUtf8()},
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
            {QStringLiteral("Полный.docx"),
             buildStoredZip({
                 ZipEntry{"word/document.xml", QStringLiteral("<w:document/>").toUtf8()},
                 ZipEntry{"word/styles.xml", QStringLiteral("<w:styles/>").toUtf8()},
                 ZipEntry{"word/numbering.xml", QStringLiteral("<w:numbering/>").toUtf8()},
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
