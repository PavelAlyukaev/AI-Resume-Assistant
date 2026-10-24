#include "documents/DocxContent.h"
#include "documents/DocxContainer.h"
#include <QtTest>

namespace {

// Synthetic document wrapper: the w:document root with the WordprocessingML namespace and a
// w:body that holds bodyXml. Every fixture is built in code; no file is read (T-02-05).
QByteArray wrapBody(const QByteArray &bodyXml) {
    return QByteArrayLiteral(
               "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
               "<w:body>") +
           bodyXml + QByteArrayLiteral("</w:body></w:document>");
}

// Parts with only word/document.xml: styles and numbering are empty.
documents::DocxParts partsWithBody(const QByteArray &bodyXml) {
    documents::DocxParts parts;
    parts.documentXml = wrapBody(bodyXml);
    return parts;
}

// One paragraph with one run and one text element.
QByteArray paragraphXml(const QString &text) {
    return QStringLiteral("<w:p><w:r><w:t>%1</w:t></w:r></w:p>").arg(text).toUtf8();
}

} // namespace

class DocxContentTest : public QObject {
    Q_OBJECT
  private slots:
    void paragraphsKeepDocumentOrder() {
        const QByteArray body = paragraphXml(QStringLiteral("Иван Петров")) +
                                paragraphXml(QStringLiteral("Опыт работы")) +
                                paragraphXml(QStringLiteral("Разработчик C++"));
        auto result = documents::readDocxContent(partsWithBody(body));
        QVERIFY2(result.has_value(), "a plain three-paragraph body must parse");

        const auto &blocks = result->blocks;
        QCOMPARE(blocks.size(), qsizetype{3});
        const QStringList expected = {QStringLiteral("Иван Петров"), QStringLiteral("Опыт работы"),
                                      QStringLiteral("Разработчик C++")};
        for (qsizetype i = 0; i < blocks.size(); ++i) {
            const auto *paragraph = std::get_if<documents::Paragraph>(&blocks.at(i).content);
            QVERIFY2(paragraph != nullptr, "every block of this body is a paragraph");
            QCOMPARE(paragraph->text, expected.at(i));
        }
    }
};

QTEST_GUILESS_MAIN(DocxContentTest)
#include "DocxContentTest.moc"
