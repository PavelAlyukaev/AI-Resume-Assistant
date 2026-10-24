#include "application/DocumentImport.h"
#include "DocxFixtures.h"
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QTemporaryDir>
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
};

QTEST_GUILESS_MAIN(DocumentImportTest)
#include "DocumentImportTest.moc"
