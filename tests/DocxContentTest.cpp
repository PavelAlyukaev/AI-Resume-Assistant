#include "documents/DocxContent.h"
#include "documents/DocxContainer.h"
#include <QStringList>
#include <QtTest>
#include <utility>
#include <variant>

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

// One paragraph with one run and one text element. The text is inserted as raw XML.
QByteArray paragraphXml(const QString &text) {
    return QStringLiteral("<w:p><w:r><w:t>%1</w:t></w:r></w:p>").arg(text).toUtf8();
}

// A complete document with a prolog (declaration, DOCTYPE) in front of the wrapped body.
QByteArray documentWithProlog(const QByteArray &prolog, const QByteArray &bodyXml) {
    return prolog + wrapBody(bodyXml);
}

constexpr auto kUtf8Declaration = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";

// UTF-16 bytes with a byte-order mark. QString::utf16() gives host (little-endian) order here.
QByteArray utf16WithBom(const QString &text) {
    QByteArray out("\xFF\xFE", 2);
    out.append(reinterpret_cast<const char *>(text.utf16()), text.size() * 2);
    return out;
}

// Messages Qt emits while a test runs. The marker test checks that none of them carries it.
QStringList g_capturedMessages;

void captureMessage(QtMsgType, const QMessageLogContext &, const QString &message) {
    g_capturedMessages.append(message);
}

// Installs captureMessage for one scope and restores the previous handler on exit, also when a
// QVERIFY in the test returns early.
class ScopedMessageCapture {
  public:
    ScopedMessageCapture() : m_previous(qInstallMessageHandler(captureMessage)) {
        g_capturedMessages.clear();
    }
    ~ScopedMessageCapture() { qInstallMessageHandler(m_previous); }
    ScopedMessageCapture(const ScopedMessageCapture &) = delete;
    ScopedMessageCapture &operator=(const ScopedMessageCapture &) = delete;

  private:
    QtMessageHandler m_previous;
};

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

    void doctypeRejected_data() {
        QTest::addColumn<QByteArray>("documentXml");
        QTest::addColumn<QByteArray>("stylesXml");
        QTest::addColumn<QByteArray>("numberingXml");

        const QByteArray plain = documentWithProlog(kUtf8Declaration, paragraphXml(QStringLiteral("Текст")));
        const QByteArray doctypeStyles =
            QByteArray(kUtf8Declaration) +
            QByteArrayLiteral("<!DOCTYPE w:styles><w:styles xmlns:w=\"http://schemas.openxmlformats.org/"
                              "wordprocessingml/2006/main\"/>");
        const QByteArray doctypeNumbering =
            QByteArray(kUtf8Declaration) +
            QByteArrayLiteral(
                "<!DOCTYPE w:numbering><w:numbering xmlns:w=\"http://schemas.openxmlformats.org/"
                "wordprocessingml/2006/main\"/>");

        QTest::newRow("internal subset in document.xml")
            << documentWithProlog(QByteArray(kUtf8Declaration) +
                                      QByteArrayLiteral("<!DOCTYPE w:document [<!ENTITY inner \"внутри\">]>"),
                                  paragraphXml(QStringLiteral("&inner;")))
            << QByteArray() << QByteArray();
        QTest::newRow("external SYSTEM entity in document.xml")
            << documentWithProlog(QByteArray(kUtf8Declaration) +
                                      QByteArrayLiteral("<!DOCTYPE w:document [<!ENTITY xxe SYSTEM "
                                                        "\"file:///nonexistent/resume-test-secret.txt\">]>"),
                                  paragraphXml(QStringLiteral("&xxe;")))
            << QByteArray() << QByteArray();
        QTest::newRow("DOCTYPE in styles.xml, valid document.xml") << plain << doctypeStyles << QByteArray();
        QTest::newRow("DOCTYPE in numbering.xml, valid document.xml")
            << plain << QByteArray() << doctypeNumbering;
    }

    void doctypeRejected() {
        QFETCH(QByteArray, documentXml);
        QFETCH(QByteArray, stylesXml);
        QFETCH(QByteArray, numberingXml);

        documents::DocxParts parts;
        parts.documentXml = documentXml;
        parts.stylesXml = stylesXml;
        parts.numberingXml = numberingXml;
        auto result = documents::readDocxContent(parts);
        QVERIFY2(!result.has_value(), "a DOCTYPE in any of the three parts must be refused");
        QCOMPARE(result.error(), documents::DocxError::UnsafeXml);
    }

    void doctypeResultHasNoContent() {
        // The paragraph after the DOCTYPE must not survive as a partial result (D-07).
        const QByteArray document = documentWithProlog(
            QByteArray(kUtf8Declaration) +
                QByteArrayLiteral("<!DOCTYPE w:document [<!ENTITY xxe SYSTEM "
                                  "\"file:///nonexistent/resume-test-secret.txt\">]>"),
            paragraphXml(QStringLiteral("Абзац после DOCTYPE")) + paragraphXml(QStringLiteral("&xxe;")));
        documents::DocxParts parts;
        parts.documentXml = document;
        auto result = documents::readDocxContent(parts);
        QVERIFY2(!result.has_value(), "no value may be returned for a refused DOCTYPE");
        QCOMPARE(result.error(), documents::DocxError::UnsafeXml);
    }

    void undeclaredEntityIsCorrupt() {
        const QByteArray document =
            documentWithProlog(kUtf8Declaration, paragraphXml(QStringLiteral("до &nope; после")));
        documents::DocxParts parts;
        parts.documentXml = document;
        auto result = documents::readDocxContent(parts);
        QVERIFY2(!result.has_value(), "an undeclared entity must be refused");
        QCOMPARE(result.error(), documents::DocxError::Corrupt);
    }

    void truncatedXmlIsCorrupt() {
        // Cut in the middle of a paragraph: the document ends with elements still open.
        const QByteArray full =
            documentWithProlog(kUtf8Declaration, paragraphXml(QStringLiteral("Обрезанный абзац")));
        const QByteArray cut = QByteArrayLiteral("Обрез");
        const QByteArray document = full.left(full.indexOf(cut) + cut.size());
        documents::DocxParts parts;
        parts.documentXml = document;
        auto result = documents::readDocxContent(parts);
        QVERIFY2(!result.has_value(), "a truncated document must be refused");
        QCOMPARE(result.error(), documents::DocxError::Corrupt);
    }

    void declaredEncodingPolicy_data() {
        QTest::addColumn<QByteArray>("documentXml");
        QTest::addColumn<bool>("accepted");
        QTest::addColumn<QString>("expectedText");

        const QString text = QStringLiteral("Тест");
        const QByteArray paragraph = paragraphXml(text);
        QTest::newRow("windows-1251 is refused")
            << documentWithProlog(QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"windows-1251\"?>"),
                                  paragraph)
            << false << QString();
        QTest::newRow("UTF-8 is accepted") << documentWithProlog(kUtf8Declaration, paragraph) << true << text;
        QTest::newRow("utf-8 in lower case is accepted")
            << documentWithProlog(QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"utf-8\"?>"), paragraph)
            << true << text;
        QTest::newRow("no declaration is accepted") << wrapBody(paragraph) << true << text;
        QTest::newRow("UTF-16 with BOM is accepted")
            << utf16WithBom(QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-16\"?>") +
                            QString::fromUtf8(wrapBody(paragraph)))
            << true << text;
    }

    void declaredEncodingPolicy() {
        QFETCH(QByteArray, documentXml);
        QFETCH(bool, accepted);
        QFETCH(QString, expectedText);

        documents::DocxParts parts;
        parts.documentXml = documentXml;
        auto result = documents::readDocxContent(parts);
        if (!accepted) {
            QVERIFY2(!result.has_value(), "a declared encoding outside UTF-8 and UTF-16 must be refused");
            QCOMPARE(result.error(), documents::DocxError::UnsupportedEncoding);
            return;
        }
        QVERIFY2(result.has_value(), "UTF-8, UTF-16 and a missing declaration must parse");
        QCOMPARE(result->blocks.size(), qsizetype{1});
        const auto *paragraph = std::get_if<documents::Paragraph>(&result->blocks.at(0).content);
        QVERIFY2(paragraph != nullptr, "the single block is a paragraph");
        QCOMPARE(paragraph->text, expectedText);
    }

    void messagesDoNotCarryMarker() {
        const QString marker = QStringLiteral("MARKER-7731");

        // Marker as an entity name in the DOCTYPE (external entity case).
        {
            ScopedMessageCapture capture;
            const QByteArray document =
                documentWithProlog(QByteArray(kUtf8Declaration) +
                                       QByteArrayLiteral("<!DOCTYPE w:document [<!ENTITY MARKER-7731 SYSTEM "
                                                         "\"file:///nonexistent/resume-test.txt\">]>"),
                                   paragraphXml(QStringLiteral("&MARKER-7731;")));
            documents::DocxParts parts;
            parts.documentXml = document;
            auto result = documents::readDocxContent(parts);
            QVERIFY(!result.has_value());
            QCOMPARE(result.error(), documents::DocxError::UnsafeXml);
            for (const QString &message : std::as_const(g_capturedMessages))
                QVERIFY2(!message.contains(marker), "a DOCTYPE message must not carry the marker");
        }

        // Marker in paragraph text next to an undeclared entity (Corrupt case).
        {
            ScopedMessageCapture capture;
            const QByteArray document =
                documentWithProlog(kUtf8Declaration, paragraphXml(QStringLiteral("MARKER-7731 &nope;")));
            documents::DocxParts parts;
            parts.documentXml = document;
            auto result = documents::readDocxContent(parts);
            QVERIFY(!result.has_value());
            QCOMPARE(result.error(), documents::DocxError::Corrupt);
            for (const QString &message : std::as_const(g_capturedMessages))
                QVERIFY2(!message.contains(marker), "a parser message must not carry the marker");
        }
    }
};

QTEST_GUILESS_MAIN(DocxContentTest)
#include "DocxContentTest.moc"
