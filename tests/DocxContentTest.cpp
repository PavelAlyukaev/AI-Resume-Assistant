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

// The w:numPr of one paragraph: its level and its numId.
QByteArray numPrXml(int ilvl, int numId) {
    return "<w:numPr><w:ilvl w:val=\"" + QByteArray::number(ilvl) + "\"/><w:numId w:val=\"" +
           QByteArray::number(numId) + "\"/></w:numPr>";
}

// One w:lvl. The start, pStyle and lvlText children are written only when the value is not empty.
QByteArray levelXml(int ilvl, const QByteArray &numFmt, const QByteArray &lvlText = {},
                    const QByteArray &pStyle = {}, const QByteArray &start = {}) {
    QByteArray xml = "<w:lvl w:ilvl=\"" + QByteArray::number(ilvl) + "\">";
    if (!start.isEmpty())
        xml += "<w:start w:val=\"" + start + "\"/>";
    xml += "<w:numFmt w:val=\"" + numFmt + "\"/>";
    if (!pStyle.isEmpty())
        xml += "<w:pStyle w:val=\"" + pStyle + "\"/>";
    if (!lvlText.isEmpty())
        xml += "<w:lvlText w:val=\"" + lvlText + "\"/>";
    return xml + "</w:lvl>";
}

// A w:abstractNum with the given levels.
QByteArray abstractNumXml(int abstractId, const QByteArray &levels) {
    return "<w:abstractNum w:abstractNumId=\"" + QByteArray::number(abstractId) + "\">" + levels +
           "</w:abstractNum>";
}

// A w:num that maps numId to abstractId.
QByteArray numXml(int numId, int abstractId) {
    return "<w:num w:numId=\"" + QByteArray::number(numId) + "\"><w:abstractNumId w:val=\"" +
           QByteArray::number(abstractId) + "\"/></w:num>";
}

// Nine levels (0 to 8) of one format; lvlText is written on each level when it is not empty.
QByteArray nineLevels(const QByteArray &numFmt, const QByteArray &lvlText = {}) {
    QByteArray levels;
    for (int ilvl = 0; ilvl < 9; ++ilvl)
        levels += levelXml(ilvl, numFmt, lvlText);
    return levels;
}

// Synthetic word/numbering.xml around the given abstractNum and num elements.
QByteArray wrapNumbering(const QByteArray &numberingBody) {
    return QByteArrayLiteral(
               "<w:numbering xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">") +
           numberingBody + QByteArrayLiteral("</w:numbering>");
}

// Parts with word/document.xml, word/numbering.xml and word/styles.xml (styles may be empty).
documents::DocxParts partsWithLists(const QByteArray &bodyXml, const QByteArray &numberingBody,
                                    const QByteArray &stylesBody = {}) {
    documents::DocxParts parts;
    parts.documentXml = wrapBody(bodyXml);
    parts.numberingXml = wrapNumbering(numberingBody);
    parts.stylesXml = wrapStyles(stylesBody);
    return parts;
}

// numId 1: bullets on nine levels. numId 2: decimal "N." on nine levels.
QByteArray bulletAndDecimalNumbering() {
    return abstractNumXml(0, nineLevels("bullet")) + abstractNumXml(1, nineLevels("decimal", "%1.")) +
           numXml(1, 0) + numXml(2, 1);
}

// numId 1 to 5, one level each: lowerLetter, upperRoman, decimal "%1.", decimal "%1)", bullet with a
// text that is not a bullet character. Only numIds 3 and 5 are reproduced exactly.
QByteArray mixedFormatsNumbering() {
    return abstractNumXml(0, levelXml(0, "lowerLetter")) + abstractNumXml(1, levelXml(0, "upperRoman")) +
           abstractNumXml(2, levelXml(0, "decimal", "%1.")) +
           abstractNumXml(3, levelXml(0, "decimal", "%1)")) + abstractNumXml(4, levelXml(0, "bullet", "x")) +
           numXml(1, 0) + numXml(2, 1) + numXml(3, 2) + numXml(4, 3) + numXml(5, 4);
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

    void listsKeepKindAndLevel_data() {
        QTest::addColumn<QByteArray>("numPr");
        QTest::addColumn<int>("expectedLevel");
        QTest::addColumn<bool>("expectBullet");
        QTest::addColumn<QString>("expectedMarker");

        const QString bullet(QChar(0x2022));
        QTest::newRow("bullet level 0") << numPrXml(0, 1) << 0 << true << bullet;
        QTest::newRow("bullet level 1") << numPrXml(1, 1) << 1 << true << bullet;
        QTest::newRow("bullet level 2") << numPrXml(2, 1) << 2 << true << bullet;
        QTest::newRow("ilvl above 8 is level 8") << numPrXml(12, 1) << 8 << true << bullet;
        QTest::newRow("negative ilvl is level 0")
            << QByteArray("<w:numPr><w:ilvl w:val=\"-3\"/><w:numId w:val=\"1\"/></w:numPr>") << 0 << true
            << bullet;
        QTest::newRow("numId 0 is not a list") << numPrXml(0, 0) << -1 << false << QString();
        QTest::newRow("numId missing from numbering is not a list")
            << numPrXml(0, 7) << -1 << false << QString();
        QTest::newRow("decimal level 0 is numbered 1.")
            << numPrXml(0, 2) << 0 << false << QStringLiteral("1.");
    }

    void listsKeepKindAndLevel() {
        QFETCH(QByteArray, numPr);
        QFETCH(int, expectedLevel);
        QFETCH(bool, expectBullet);
        QFETCH(QString, expectedMarker);

        auto result = documents::readDocxContent(
            partsWithLists(paragraphWithProps(numPr), bulletAndDecimalNumbering()));
        QVERIFY2(result.has_value(), "a list paragraph must parse");
        const documents::Paragraph *paragraph = paragraphAt(*result, 0);
        QVERIFY2(paragraph != nullptr, "the block is a paragraph");
        QCOMPARE(paragraph->text, QStringLiteral("Текст"));
        QCOMPARE(paragraph->listLevel, expectedLevel);
        QCOMPARE(paragraph->listMarker, expectedMarker);
        if (expectedLevel >= 0)
            QCOMPARE(paragraph->listKind == documents::ListKind::Bullet, expectBullet);
    }

    void numberedCountersRestartOnHigherLevel() {
        // Levels 0, 1, 1, 0, 1 of one decimal list give the markers 1., 1., 2., 2., 1.
        const QByteArray body = paragraphWithProps(numPrXml(0, 2)) + paragraphWithProps(numPrXml(1, 2)) +
                                paragraphWithProps(numPrXml(1, 2)) + paragraphWithProps(numPrXml(0, 2)) +
                                paragraphWithProps(numPrXml(1, 2));
        auto result = documents::readDocxContent(partsWithLists(body, bulletAndDecimalNumbering()));
        QVERIFY2(result.has_value(), "a numbered list must parse");
        QCOMPARE(result->blocks.size(), qsizetype{5});

        const QStringList expected = {QStringLiteral("1."), QStringLiteral("1."), QStringLiteral("2."),
                                      QStringLiteral("2."), QStringLiteral("1.")};
        for (qsizetype i = 0; i < result->blocks.size(); ++i) {
            const documents::Paragraph *paragraph = paragraphAt(*result, i);
            QVERIFY2(paragraph != nullptr, "every block is a paragraph");
            QCOMPARE(paragraph->listMarker, expected.at(i));
        }
    }

    void styleNumPrLevelFromPStyle() {
        // ListHeader carries numId 3 without an ilvl. Level 1 of abstract 2 names ListHeader in its
        // w:pStyle, so the paragraph with that style is at level 1. A paragraph with its own numPr
        // (no pStyle) at ilvl 0 is at level 0.
        const QByteArray styles =
            QStringLiteral(
                "<w:style w:styleId=\"ListHeader\"><w:pPr><w:numPr><w:numId w:val=\"3\"/></w:numPr>"
                "</w:pPr></w:style>")
                .toUtf8();
        const QByteArray numbering =
            abstractNumXml(2, levelXml(0, "decimal", "%1.") + levelXml(1, "decimal", {}, "ListHeader")) +
            numXml(3, 2);
        const QByteArray body =
            paragraphWithProps("<w:pStyle w:val=\"ListHeader\"/>") + paragraphWithProps(numPrXml(0, 3));
        auto result = documents::readDocxContent(partsWithLists(body, numbering, styles));
        QVERIFY2(result.has_value(), "a list taken from a style must parse");
        QCOMPARE(result->blocks.size(), qsizetype{2});

        const documents::Paragraph *fromStyle = paragraphAt(*result, 0);
        QVERIFY2(fromStyle != nullptr, "the first block is a paragraph");
        QCOMPARE(fromStyle->listLevel, 1);
        QVERIFY(fromStyle->listKind == documents::ListKind::Ordered);
        QCOMPARE(fromStyle->listMarker, QStringLiteral("1."));

        const documents::Paragraph *own = paragraphAt(*result, 1);
        QVERIFY2(own != nullptr, "the second block is a paragraph");
        QCOMPARE(own->listLevel, 0);
        QCOMPARE(own->listMarker, QStringLiteral("1."));
    }

    void numberingStartClamped() {
        // Start -5 becomes 1; 2000000000 becomes 1000000; 3 stays 3.
        const QByteArray numbering = abstractNumXml(0, levelXml(0, "decimal", "%1.", {}, "-5")) +
                                     abstractNumXml(1, levelXml(0, "decimal", "%1.", {}, "2000000000")) +
                                     abstractNumXml(2, levelXml(0, "decimal", "%1.", {}, "3")) +
                                     numXml(1, 0) + numXml(2, 1) + numXml(3, 2);
        const QByteArray body = paragraphWithProps(numPrXml(0, 1)) + paragraphWithProps(numPrXml(0, 2)) +
                                paragraphWithProps(numPrXml(0, 3));
        auto result = documents::readDocxContent(partsWithLists(body, numbering));
        QVERIFY2(result.has_value(), "start values outside the range must still parse");
        QCOMPARE(result->blocks.size(), qsizetype{3});

        const QStringList expected = {QStringLiteral("1."), QStringLiteral("1000000."), QStringLiteral("3.")};
        for (qsizetype i = 0; i < result->blocks.size(); ++i) {
            const documents::Paragraph *paragraph = paragraphAt(*result, i);
            QVERIFY2(paragraph != nullptr, "every block is a paragraph");
            QCOMPARE(paragraph->listMarker, expected.at(i));
        }
    }

    void unsupportedNumberingFormatCountedOncePerList_data() {
        QTest::addColumn<int>("firstNumId");
        QTest::addColumn<int>("secondNumId");
        QTest::addColumn<int>("expectedWarnings");
        QTest::addColumn<QString>("firstMarker");
        QTest::addColumn<QString>("secondMarker");

        const QString bullet(QChar(0x2022));
        QTest::newRow("lowerLetter: one warning for the list, markers 1. and 2.")
            << 1 << 1 << 1 << QStringLiteral("1.") << QStringLiteral("2.");
        QTest::newRow("lowerLetter and upperRoman are two lists: two warnings")
            << 1 << 2 << 2 << QStringLiteral("1.") << QStringLiteral("1.");
        QTest::newRow("decimal with %1. is exact: no warning")
            << 3 << 3 << 0 << QStringLiteral("1.") << QStringLiteral("2.");
        QTest::newRow("decimal with %1) is approximated: one warning, no parenthesis in the marker")
            << 4 << 4 << 1 << QStringLiteral("1.") << QStringLiteral("2.");
        QTest::newRow("bullet with any lvlText is exact: no warning") << 5 << 5 << 0 << bullet << bullet;
    }

    void unsupportedNumberingFormatCountedOncePerList() {
        QFETCH(int, firstNumId);
        QFETCH(int, secondNumId);
        QFETCH(int, expectedWarnings);
        QFETCH(QString, firstMarker);
        QFETCH(QString, secondMarker);

        const QByteArray body =
            paragraphWithProps(numPrXml(0, firstNumId)) + paragraphWithProps(numPrXml(0, secondNumId));
        auto result = documents::readDocxContent(partsWithLists(body, mixedFormatsNumbering()));
        QVERIFY2(result.has_value(), "unsupported formats are approximated, not refused");
        QCOMPARE(result->warnings.count(documents::DocxWarning::ListFormatApproximated),
                 qsizetype{expectedWarnings});

        const documents::Paragraph *first = paragraphAt(*result, 0);
        QVERIFY2(first != nullptr, "the first block is a paragraph");
        QCOMPARE(first->listMarker, firstMarker);
        const documents::Paragraph *second = paragraphAt(*result, 1);
        QVERIFY2(second != nullptr, "the second block is a paragraph");
        QCOMPARE(second->listMarker, secondMarker);
    }
};

QTEST_GUILESS_MAIN(DocxContentTest)
#include "DocxContentTest.moc"
