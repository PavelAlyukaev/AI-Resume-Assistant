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

// Synthetic word/styles.xml: the w:styles root with the given style elements. These are our
// reading of the format, not a file saved by Word (see the assumptions in 02-RESEARCH.md).
QByteArray wrapStyles(const QByteArray &stylesBody) {
    return QByteArrayLiteral(
               "<w:styles xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">") +
           stylesBody + QByteArrayLiteral("</w:styles>");
}

// Parts with word/document.xml and word/styles.xml; numbering stays empty.
documents::DocxParts partsWithStyles(const QByteArray &bodyXml, const QByteArray &stylesBody) {
    documents::DocxParts parts;
    parts.documentXml = wrapBody(bodyXml);
    parts.stylesXml = wrapStyles(stylesBody);
    return parts;
}

// The paragraph at index of the result; null when the block there is not a paragraph.
const documents::Paragraph *paragraphAt(const documents::DocxDocument &document, qsizetype index) {
    if (index < 0 || index >= document.blocks.size())
        return nullptr;
    return std::get_if<documents::Paragraph>(&document.blocks.at(index).content);
}

// One paragraph with the text "Текст". pPrChildren are the children of its w:pPr; an empty value
// gives a paragraph without w:pPr.
QByteArray paragraphWithProps(const QByteArray &pPrChildren) {
    QByteArray xml = "<w:p>";
    if (!pPrChildren.isEmpty())
        xml += "<w:pPr>" + pPrChildren + "</w:pPr>";
    xml += QStringLiteral("<w:r><w:t>Текст</w:t></w:r></w:p>").toUtf8();
    return xml;
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

    void whitespaceAndCdataKept_data() {
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<QString>("expectedText");

        QTest::newRow("xml:space preserve keeps a lone space")
            << QByteArray("<w:p><w:r><w:t xml:space=\"preserve\"> </w:t></w:r></w:p>") << QStringLiteral(" ");
        QTest::newRow("CDATA joins the text around it")
            << QByteArray("<w:p><w:r><w:t>a<![CDATA[b]]>c</w:t></w:r></w:p>") << QStringLiteral("abc");
        QTest::newRow("escaped ampersand")
            << QByteArray("<w:p><w:r><w:t>A&amp;B</w:t></w:r></w:p>") << QStringLiteral("A&B");
        QTest::newRow("empty w:t gives empty text")
            << QByteArray("<w:p><w:r><w:t/></w:r></w:p>") << QString();
        QTest::newRow("two runs keep the space between them")
            << QStringLiteral("<w:p><w:r><w:t xml:space=\"preserve\">Опыт </w:t></w:r>"
                              "<w:r><w:t>работы</w:t></w:r></w:p>")
                   .toUtf8()
            << QStringLiteral("Опыт работы");
    }

    void whitespaceAndCdataKept() {
        QFETCH(QByteArray, body);
        QFETCH(QString, expectedText);

        auto result = documents::readDocxContent(partsWithBody(body));
        QVERIFY2(result.has_value(), "a single paragraph body must parse");
        QCOMPARE(result->blocks.size(), qsizetype{1});
        const auto *paragraph = std::get_if<documents::Paragraph>(&result->blocks.at(0).content);
        QVERIFY2(paragraph != nullptr, "the single block is a paragraph");
        QCOMPARE(paragraph->text, expectedText);
    }

    void interElementWhitespaceIgnored() {
        // Indentation and line breaks between elements, outside w:t, must not reach the text.
        const QByteArray body = QStringLiteral("\n  <w:p>\n    <w:pPr/>\n    <w:r>\n      <w:t>Текст</w:t>\n"
                                               "    </w:r>\n  </w:p>\n")
                                    .toUtf8();
        auto result = documents::readDocxContent(partsWithBody(body));
        QVERIFY2(result.has_value(), "indented body must parse");
        QCOMPARE(result->blocks.size(), qsizetype{1});
        const auto *paragraph = std::get_if<documents::Paragraph>(&result->blocks.at(0).content);
        QVERIFY2(paragraph != nullptr, "the single block is a paragraph");
        QCOMPARE(paragraph->text, QStringLiteral("Текст"));
    }

    void cyrillicTextExact() {
        const QString first = QStringLiteral("Ёлкина Йоханна — менеджер по продажам, стаж 5 лет");
        const QString second = QStringLiteral("Пётр Сидоров");
        auto result = documents::readDocxContent(partsWithBody(paragraphXml(first) + paragraphXml(second)));
        QVERIFY2(result.has_value(), "the Cyrillic body must parse");
        QCOMPARE(result->blocks.size(), qsizetype{2});
        const auto *one = std::get_if<documents::Paragraph>(&result->blocks.at(0).content);
        const auto *two = std::get_if<documents::Paragraph>(&result->blocks.at(1).content);
        QVERIFY2(one != nullptr && two != nullptr, "both blocks are paragraphs");
        QCOMPARE(one->text, first);
        QCOMPARE(two->text, second);
    }

    void textLimitBoundary() {
        // 200 paragraphs of 1000 characters: exactly kMaxTextChars, accepted.
        const QString chunk(1000, QChar(u'я'));
        QByteArray body;
        for (int i = 0; i < 200; ++i)
            body += paragraphXml(chunk);
        auto accepted = documents::readDocxContent(partsWithBody(body));
        QVERIFY2(accepted.has_value(), "exactly kMaxTextChars must be accepted");
        QCOMPARE(accepted->blocks.size(), qsizetype{200});
        qsizetype total = 0;
        for (const auto &block : accepted->blocks) {
            const auto *paragraph = std::get_if<documents::Paragraph>(&block.content);
            QVERIFY2(paragraph != nullptr, "every block is a paragraph");
            total += paragraph->text.size();
        }
        QCOMPARE(total, qsizetype{200000});

        // One more character, in a paragraph of its own: refused without a partial result.
        body += paragraphXml(QStringLiteral("я"));
        auto refused = documents::readDocxContent(partsWithBody(body));
        QVERIFY2(!refused.has_value(), "one character over kMaxTextChars must be refused");
        QCOMPARE(refused.error(), documents::DocxError::Limit);
    }

    void nodeLimitBoundary() {
        // Empty paragraphs take six bytes each, so only the node count bounds them (T-02-02).
        QByteArray body;
        body.reserve(50001 * 6);
        for (int i = 0; i < 50000; ++i)
            body += QByteArrayLiteral("<w:p/>");
        auto accepted = documents::readDocxContent(partsWithBody(body));
        QVERIFY2(accepted.has_value(), "exactly kMaxModelNodes must be accepted");
        QCOMPARE(accepted->blocks.size(), qsizetype{50000});

        body += QByteArrayLiteral("<w:p/>");
        auto refused = documents::readDocxContent(partsWithBody(body));
        QVERIFY2(!refused.has_value(), "one node over kMaxModelNodes must be refused");
        QCOMPARE(refused.error(), documents::DocxError::Limit);
    }

    void headingByStyleIdIsHeadingLevelOne() {
        // Synthetic style: the styleId Heading1 with the English internal name, as Word writes it.
        const QByteArray styles =
            QStringLiteral("<w:style w:styleId=\"Heading1\"><w:name w:val=\"heading 1\"/>"
                           "</w:style>")
                .toUtf8();
        const QByteArray body = QStringLiteral("<w:p><w:pPr><w:pStyle w:val=\"Heading1\"/></w:pPr>"
                                               "<w:r><w:t>Опыт работы</w:t></w:r></w:p>")
                                    .toUtf8() +
                                paragraphXml(QStringLiteral("Обычный текст"));
        auto result = documents::readDocxContent(partsWithStyles(body, styles));
        QVERIFY2(result.has_value(), "a body with a styled heading and a plain paragraph must parse");
        QCOMPARE(result->blocks.size(), qsizetype{2});

        const documents::Paragraph *heading = paragraphAt(*result, 0);
        QVERIFY2(heading != nullptr, "the first block is a paragraph");
        QCOMPARE(heading->text, QStringLiteral("Опыт работы"));
        QCOMPARE(heading->headingLevel, 1);

        const documents::Paragraph *plain = paragraphAt(*result, 1);
        QVERIFY2(plain != nullptr, "the second block is a paragraph");
        QCOMPARE(plain->text, QStringLiteral("Обычный текст"));
        QCOMPARE(plain->headingLevel, 0);
    }

    // Each row is one paragraph with its w:pPr children and the styles it may refer to. The Russian
    // name rows are synthetic (assumption A1, phase 3 checks a real Word file).
    void headingByStyleIdNameOrOutline_data() {
        QTest::addColumn<QByteArray>("stylesBody");
        QTest::addColumn<QByteArray>("pPrChildren");
        QTest::addColumn<int>("expectedLevel");

        const auto pStyle = [](const QByteArray &id) {
            return QByteArray("<w:pStyle w:val=\"") + id + "\"/>";
        };
        const auto styleName = [](const QByteArray &id, const QString &name) {
            return QByteArray("<w:style w:styleId=\"") + id + "\"><w:name w:val=\"" + name.toUtf8() +
                   "\"/></w:style>";
        };

        QTest::newRow("styleId Heading3")
            << QByteArray("<w:style w:styleId=\"Heading3\"/>") << pStyle("Heading3") << 3;
        QTest::newRow("name heading 4 under styleId x")
            << styleName("x", QStringLiteral("heading 4")) << pStyle("x") << 4;
        QTest::newRow("name is case-insensitive: HEADING 1")
            << styleName("Up", QStringLiteral("HEADING 1")) << pStyle("Up") << 1;
        QTest::newRow("Russian name Заголовок 2 under styleId a0")
            << styleName("a0", QStringLiteral("Заголовок 2")) << pStyle("a0") << 2;
        QTest::newRow("Russian name Заголовок without digit is not a heading")
            << styleName("a1", QStringLiteral("Заголовок")) << pStyle("a1") << 0;
        QTest::newRow("outlineLvl 2 without a heading name")
            << QByteArray("<w:style w:styleId=\"Sec\"><w:pPr><w:outlineLvl w:val=\"2\"/></w:pPr></w:style>")
            << pStyle("Sec") << 3;
        QTest::newRow("Custom based on Heading2")
            << QByteArray("<w:style w:styleId=\"Heading2\"/><w:style w:styleId=\"Custom\"><w:basedOn "
                          "w:val=\"Heading2\"/></w:style>")
            << pStyle("Custom") << 2;
        QTest::newRow("styleId Heading10 is not a heading")
            << QByteArray("<w:style w:styleId=\"Heading10\"/>") << pStyle("Heading10") << 0;
        QTest::newRow("name heading 0 is not a heading")
            << styleName("Zero", QStringLiteral("heading 0")) << pStyle("Zero") << 0;
        QTest::newRow("outlineLvl 9 in the style is not a heading")
            << QByteArray("<w:style w:styleId=\"Plain\"><w:pPr><w:outlineLvl w:val=\"9\"/></w:pPr></w:style>")
            << pStyle("Plain") << 0;
        QTest::newRow("name Title is not a heading")
            << styleName("Title", QStringLiteral("Title")) << pStyle("Title") << 0;
        QTest::newRow("direct outlineLvl 0 on a paragraph without style")
            << QByteArray() << QByteArray("<w:outlineLvl w:val=\"0\"/>") << 1;
        QTest::newRow("direct outlineLvl 9 beats the Heading2 style")
            << QByteArray("<w:style w:styleId=\"Heading2\"/>")
            << QByteArray("<w:pStyle w:val=\"Heading2\"/><w:outlineLvl w:val=\"9\"/>") << 0;
        QTest::newRow("outlineLvl 9 in the style beats basedOn Heading2")
            << QByteArray("<w:style w:styleId=\"Heading2\"/><w:style w:styleId=\"Custom\"><w:basedOn "
                          "w:val=\"Heading2\"/><w:pPr><w:outlineLvl w:val=\"9\"/></w:pPr></w:style>")
            << pStyle("Custom") << 0;
    }

    void headingByStyleIdNameOrOutline() {
        QFETCH(QByteArray, stylesBody);
        QFETCH(QByteArray, pPrChildren);
        QFETCH(int, expectedLevel);

        auto result =
            documents::readDocxContent(partsWithStyles(paragraphWithProps(pPrChildren), stylesBody));
        QVERIFY2(result.has_value(), "a single paragraph with styles must parse");
        const documents::Paragraph *paragraph = paragraphAt(*result, 0);
        QVERIFY2(paragraph != nullptr, "the block is a paragraph");
        QCOMPARE(paragraph->text, QStringLiteral("Текст"));
        QCOMPARE(paragraph->headingLevel, expectedLevel);
    }

    void cyclicBasedOnTerminates() {
        // A is based on B and B on A: the walk ends after both styles, with no heading and no error.
        const QByteArray styles =
            QStringLiteral("<w:style w:styleId=\"A\"><w:basedOn w:val=\"B\"/></w:style>"
                           "<w:style w:styleId=\"B\"><w:basedOn w:val=\"A\"/></w:style>")
                .toUtf8();
        auto result = documents::readDocxContent(
            partsWithStyles(paragraphWithProps("<w:pStyle w:val=\"A\"/>"), styles));
        QVERIFY2(result.has_value(), "a cycle in basedOn is bad style data, not an error");
        const documents::Paragraph *paragraph = paragraphAt(*result, 0);
        QVERIFY2(paragraph != nullptr, "the block is a paragraph");
        QCOMPARE(paragraph->text, QStringLiteral("Текст"));
        QCOMPARE(paragraph->headingLevel, 0);
    }

    void styleChainDepthIsBounded() {
        // A chain of styles S0..S(n-1) based on each other; the last one is a heading named "heading 2".
        // The chain is followed up to kMaxStyleChainDepth (16) styles, so the heading is reached at 10
        // styles and not at 20.
        const auto chainOf = [](int count) {
            QByteArray styles;
            for (int i = 0; i + 1 < count; ++i)
                styles += "<w:style w:styleId=\"S" + QByteArray::number(i) + "\"><w:basedOn w:val=\"S" +
                          QByteArray::number(i + 1) + "\"/></w:style>";
            styles += "<w:style w:styleId=\"S" + QByteArray::number(count - 1) +
                      "\"><w:name w:val=\"heading 2\"/></w:style>";
            return styles;
        };
        const QByteArray paragraph = paragraphWithProps("<w:pStyle w:val=\"S0\"/>");

        auto shallow = documents::readDocxContent(partsWithStyles(paragraph, chainOf(10)));
        QVERIFY2(shallow.has_value(), "a chain of 10 styles must parse");
        const documents::Paragraph *shallowParagraph = paragraphAt(*shallow, 0);
        QVERIFY2(shallowParagraph != nullptr, "the block is a paragraph");
        QCOMPARE(shallowParagraph->headingLevel, 2);

        auto deep = documents::readDocxContent(partsWithStyles(paragraph, chainOf(20)));
        QVERIFY2(deep.has_value(), "a chain of 20 styles must parse");
        const documents::Paragraph *deepParagraph = paragraphAt(*deep, 0);
        QVERIFY2(deepParagraph != nullptr, "the block is a paragraph");
        QCOMPARE(deepParagraph->headingLevel, 0);
    }
};

QTEST_GUILESS_MAIN(DocxContentTest)
#include "DocxContentTest.moc"
