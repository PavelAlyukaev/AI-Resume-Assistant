#include "documents/DocxContent.h"
#include "documents/DocxContainer.h"
#include <QStringList>
#include <QtTest>
#include <utility>
#include <variant>

namespace {

// Synthetic document wrapper: the w:document root with the WordprocessingML namespace and a
// w:body that holds bodyXml. Every fixture is built in code; no file is read (T-02-05).
// The prefixes of the fixtures: w, r, mc (markup compatibility), wp, a, pic, wps and v (VML). The
// declarations do not change what is read; they only let the synthetic figures be well formed.
QByteArray wrapBody(const QByteArray &bodyXml) {
    return QByteArrayLiteral(
               "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
               "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
               "xmlns:mc=\"http://schemas.openxmlformats.org/markup-compatibility/2006\" "
               "xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\" "
               "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
               "xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\" "
               "xmlns:wps=\"http://schemas.microsoft.com/office/word/2010/wordprocessingShape\" "
               "xmlns:v=\"urn:schemas-microsoft-com:vml\">"
               "<w:body>") +
           bodyXml + QByteArrayLiteral("</w:body></w:document>");
}

// Parts with only word/document.xml: styles and numbering are empty.
documents::DocxParts partsWithBody(const QByteArray &bodyXml) {
    documents::DocxParts parts;
    parts.documentXml = wrapBody(bodyXml);
    return parts;
}

// Parses one body and checks its warnings: the exact list, in document order (SAFE-04).
void checkWarnings(const QByteArray &bodyXml, const QList<documents::DocxWarning> &expected) {
    auto result = documents::readDocxContent(partsWithBody(bodyXml));
    QVERIFY2(result.has_value(), "the body must parse");
    QVERIFY2(result->warnings == expected, "the warnings must match the expected list");
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

// A table with one grid column and one row whose single cell holds content.
QByteArray oneCellTable(const QByteArray &content) {
    return QByteArrayLiteral("<w:tbl><w:tblGrid><w:gridCol w:w=\"9000\"/></w:tblGrid><w:tr><w:tc>") +
           content + QByteArrayLiteral("</w:tc></w:tr></w:tbl>");
}

// A w:tblGrid with the given number of w:gridCol elements.
QByteArray gridXml(int columns) {
    QByteArray xml = "<w:tblGrid>";
    for (int i = 0; i < columns; ++i)
        xml += "<w:gridCol w:w=\"1000\"/>";
    return xml + "</w:tblGrid>";
}

// A w:tbl from its grid and its w:tr elements.
QByteArray tableXml(const QByteArray &grid, const QByteArray &rows) {
    return "<w:tbl>" + grid + rows + "</w:tbl>";
}

// A w:tr from its w:tc elements.
QByteArray rowXml(const QByteArray &cells) { return "<w:tr>" + cells + "</w:tr>"; }

// A w:tc with the given w:tcPr children (none when empty) and content.
QByteArray cellXml(const QByteArray &tcPrChildren, const QByteArray &content) {
    QByteArray xml = "<w:tc>";
    if (!tcPrChildren.isEmpty())
        xml += "<w:tcPr>" + tcPrChildren + "</w:tcPr>";
    return xml + content + "</w:tc>";
}

// For each row, how many times each grid column is covered: by a cell that starts in the row, or by
// a vertical merge that started in an earlier row and still spans the row. A valid table covers
// every column of every row exactly once (DOCX-05, T-02-10).
QList<QList<int>> columnCoverage(const documents::Table &table) {
    const auto grid = static_cast<qsizetype>(table.gridColumns);
    QList<QList<int>> coverage;
    QList<int> active(grid, 0); // rows after the current one that a merge still covers, per column
    for (const auto &row : table.rows) {
        QList<int> cover(grid, 0);
        QList<int> next(grid, 0);
        for (qsizetype c = 0; c < grid; ++c) {
            if (active.at(c) > 0) {
                cover[c] += 1;
                next[c] = active.at(c) - 1;
            }
        }
        for (const auto &cell : row.cells) {
            for (qsizetype c = cell.startColumn; c < cell.startColumn + cell.columnSpan && c < grid; ++c) {
                cover[c] += 1;
                next[c] = cell.rowSpan - 1;
            }
        }
        coverage.append(cover);
        active = next;
    }
    return coverage;
}

// The texts of all paragraphs in document order; the cells of tables are read recursively.
void collectTexts(const QList<documents::Block> &blocks, QStringList &texts) {
    for (const auto &block : blocks) {
        if (const auto *paragraph = std::get_if<documents::Paragraph>(&block.content)) {
            texts << paragraph->text;
        } else if (const auto *table = std::get_if<documents::Table>(&block.content)) {
            for (const auto &row : table->rows) {
                for (const auto &cell : row.cells)
                    collectTexts(cell.blocks, texts);
            }
        }
    }
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

    void tableWithOneRowKeepsCells() {
        const QByteArray table =
            QByteArrayLiteral(
                "<w:tbl><w:tblGrid><w:gridCol w:w=\"2000\"/><w:gridCol w:w=\"3000\"/></w:tblGrid>"
                "<w:tr><w:tc>") +
            paragraphXml(QStringLiteral("Ячейка А")) + QByteArrayLiteral("</w:tc><w:tc>") +
            paragraphXml(QStringLiteral("Ячейка Б")) + QByteArrayLiteral("</w:tc></w:tr></w:tbl>");
        auto result = documents::readDocxContent(partsWithBody(table));
        QVERIFY2(result.has_value(), "a one-row table must parse");
        QCOMPARE(result->blocks.size(), qsizetype{1});

        const auto *tbl = std::get_if<documents::Table>(&result->blocks.at(0).content);
        QVERIFY2(tbl != nullptr, "the block is a table");
        QCOMPARE(tbl->gridColumns, 2);
        QCOMPARE(tbl->rows.size(), qsizetype{1});
        const auto &cells = tbl->rows.at(0).cells;
        QCOMPARE(cells.size(), qsizetype{2});
        const QStringList expected = {QStringLiteral("Ячейка А"), QStringLiteral("Ячейка Б")};
        for (qsizetype i = 0; i < cells.size(); ++i) {
            const auto &cell = cells.at(i);
            QCOMPARE(cell.startColumn, static_cast<int>(i));
            QCOMPARE(cell.columnSpan, 1);
            QCOMPARE(cell.rowSpan, 1);
            QCOMPARE(cell.blocks.size(), qsizetype{1});
            const auto *paragraph = std::get_if<documents::Paragraph>(&cell.blocks.at(0).content);
            QVERIFY2(paragraph != nullptr, "a cell holds paragraphs in this body");
            QCOMPARE(paragraph->text, expected.at(i));
        }
    }

    void cellTextCountsTowardTextLimit() {
        // One paragraph of kMaxTextChars characters inside a cell: accepted (the limit is inclusive).
        const QString chunk(200000, QChar(u'я'));
        auto accepted = documents::readDocxContent(partsWithBody(oneCellTable(paragraphXml(chunk))));
        QVERIFY2(accepted.has_value(), "exactly kMaxTextChars in a cell must be accepted");

        // One character more in the cell: refused, the cell text is part of the same total.
        const QString over = chunk + QChar(u'я');
        auto refused = documents::readDocxContent(partsWithBody(oneCellTable(paragraphXml(over))));
        QVERIFY2(!refused.has_value(), "one character over kMaxTextChars in a cell must be refused");
        QCOMPARE(refused.error(), documents::DocxError::Limit);
    }

    void tableRowsKeepGridWidth_data() {
        QTest::addColumn<QByteArray>("rowsXml");
        QTest::addColumn<int>("gridColumns");
        QTest::addColumn<QString>("expected");

        const QByteArray empty = "<w:p/>";
        const QByteArray spanTwo = cellXml("<w:gridSpan w:val=\"2\"/>", empty);
        QTest::newRow("span of two, then one cell")
            << rowXml(spanTwo + cellXml("", empty)) << 3 << QStringLiteral("0:2,2:1");
        QTest::newRow("short row is padded")
            << rowXml(cellXml("", empty)) << 3 << QStringLiteral("0:1,1:1,2:1");
        QTest::newRow("span beyond the grid is trimmed")
            << rowXml(cellXml("<w:gridSpan w:val=\"5\"/>", empty)) << 3 << QStringLiteral("0:3");
        QTest::newRow("row without cells is padded")
            << rowXml(QByteArray()) << 2 << QStringLiteral("0:1,1:1");
        QTest::newRow("rows of different shape")
            << rowXml(spanTwo + cellXml("", empty)) +
                   rowXml(cellXml("", empty) + cellXml("", empty) + cellXml("", empty))
            << 3 << QStringLiteral("0:2,2:1;0:1,1:1,2:1");
    }

    void tableRowsKeepGridWidth() {
        QFETCH(QByteArray, rowsXml);
        QFETCH(int, gridColumns);
        QFETCH(QString, expected);

        auto result = documents::readDocxContent(partsWithBody(tableXml(gridXml(gridColumns), rowsXml)));
        QVERIFY2(result.has_value(), "a table with short, spanning and empty rows must parse");
        QCOMPARE(result->blocks.size(), qsizetype{1});
        const auto *table = std::get_if<documents::Table>(&result->blocks.at(0).content);
        QVERIFY2(table != nullptr, "the only block is a table");
        QCOMPARE(table->gridColumns, gridColumns);

        QStringList rows;
        for (const auto &row : table->rows) {
            QStringList cells;
            for (const auto &cell : row.cells)
                cells << QStringLiteral("%1:%2").arg(cell.startColumn).arg(cell.columnSpan);
            rows << cells.join(QLatin1Char(','));
        }
        QCOMPARE(rows.join(QLatin1Char(';')), expected);

        // Invariant: every grid column of every row is covered exactly once.
        for (const auto &cover : columnCoverage(*table)) {
            for (int count : cover)
                QCOMPARE(count, 1);
        }
    }

    void vMergeNotDuplicated() {
        const QByteArray restart =
            cellXml("<w:vMerge w:val=\"restart\"/>", paragraphXml(QStringLiteral("Группа")));
        const QByteArray continueVal =
            cellXml("<w:vMerge w:val=\"continue\"/>", paragraphXml(QStringLiteral("Продолжение А")));
        const QByteArray continueBare = cellXml("<w:vMerge/>", paragraphXml(QStringLiteral("Продолжение Б")));
        const QByteArray rows = rowXml(restart + cellXml("", paragraphXml(QStringLiteral("Б1")))) +
                                rowXml(continueVal + cellXml("", paragraphXml(QStringLiteral("Б2")))) +
                                rowXml(continueBare + cellXml("", paragraphXml(QStringLiteral("Б3"))));

        auto result = documents::readDocxContent(partsWithBody(tableXml(gridXml(2), rows)));
        QVERIFY2(result.has_value(), "a vertical merge over three rows must parse");
        QCOMPARE(result->blocks.size(), qsizetype{1});
        const auto *table = std::get_if<documents::Table>(&result->blocks.at(0).content);
        QVERIFY2(table != nullptr, "the only block is a table");
        QCOMPARE(table->rows.size(), qsizetype{3});

        // The first row holds the group cell with rowSpan 3 and its own cell in column 1.
        const auto &first = table->rows.at(0).cells;
        QCOMPARE(first.size(), qsizetype{2});
        QCOMPARE(first.at(0).startColumn, 0);
        QCOMPARE(first.at(0).rowSpan, 3);

        // The rows below hold only their own cell in column 1: no second cell starts in column 0.
        for (qsizetype r = 1; r < 3; ++r) {
            const auto &cells = table->rows.at(r).cells;
            QCOMPARE(cells.size(), qsizetype{1});
            QCOMPARE(cells.at(0).startColumn, 1);
            QCOMPARE(cells.at(0).rowSpan, 1);
        }

        // The text of the continuation cells is not repeated.
        QStringList texts;
        collectTexts(result->blocks, texts);
        QCOMPARE(texts, (QStringList{QStringLiteral("Группа"), QStringLiteral("Б1"), QStringLiteral("Б2"),
                                     QStringLiteral("Б3")}));
        for (const auto &cover : columnCoverage(*table)) {
            for (int count : cover)
                QCOMPARE(count, 1);
        }
    }

    void emptyAndNestedTables() {
        const QByteArray inner =
            tableXml(gridXml(1), rowXml(cellXml("", paragraphXml(QStringLiteral("Вложенная")))));
        const QByteArray rows = rowXml(cellXml("", "<w:p/>") + cellXml("", inner));

        auto result = documents::readDocxContent(partsWithBody(tableXml(gridXml(2), rows)));
        QVERIFY2(result.has_value(), "an empty cell and a nested table must parse");
        const auto *table = std::get_if<documents::Table>(&result->blocks.at(0).content);
        QVERIFY2(table != nullptr, "the only block is a table");
        QCOMPARE(table->gridColumns, 2);

        const auto &cells = table->rows.at(0).cells;
        QCOMPARE(cells.size(), qsizetype{2});

        // The empty cell holds one paragraph with empty text.
        QCOMPARE(cells.at(0).blocks.size(), qsizetype{1});
        const auto *empty = std::get_if<documents::Paragraph>(&cells.at(0).blocks.at(0).content);
        QVERIFY2(empty != nullptr, "the empty cell holds a paragraph");
        QVERIFY(empty->text.isEmpty());

        // The nested table is a block of its cell, and its grid does not change the outer one.
        QCOMPARE(cells.at(1).blocks.size(), qsizetype{1});
        const auto *nested = std::get_if<documents::Table>(&cells.at(1).blocks.at(0).content);
        QVERIFY2(nested != nullptr, "the nested table is a block of the cell");
        QCOMPARE(nested->gridColumns, 1);
        QStringList texts;
        collectTexts(cells.at(1).blocks, texts);
        QCOMPARE(texts, QStringList{QStringLiteral("Вложенная")});
    }

    void nestingDepthLimit() {
        // Each table holds the next one in its only cell. 64 levels are accepted, 65 are refused.
        auto nestedTables = [](int levels) {
            QByteArray xml = "<w:p/>";
            for (int i = 0; i < levels; ++i)
                xml = tableXml(gridXml(1), rowXml(cellXml("", xml)));
            return xml;
        };
        auto accepted = documents::readDocxContent(partsWithBody(nestedTables(64)));
        QVERIFY2(accepted.has_value(), "64 nested tables must be accepted");

        auto refused = documents::readDocxContent(partsWithBody(nestedTables(65)));
        QVERIFY2(!refused.has_value(), "65 nested tables must be refused");
        QCOMPARE(refused.error(), documents::DocxError::Limit);
    }

    void paddingCellsCountedInNodeLimit() {
        // One table, one row, one empty cell and a grid of N columns: N - 1 padding cells follow the
        // cell, and each one is a node. Nodes are N + 2, so N = 49998 is the last accepted value.
        auto oneCellGrid = [](int columns) {
            return tableXml(gridXml(columns), rowXml(cellXml("", QByteArray())));
        };
        auto accepted = documents::readDocxContent(partsWithBody(oneCellGrid(49998)));
        QVERIFY2(accepted.has_value(), "exactly kMaxModelNodes including padding must be accepted");

        auto refused = documents::readDocxContent(partsWithBody(oneCellGrid(49999)));
        QVERIFY2(!refused.has_value(), "one padding cell over kMaxModelNodes must be refused");
        QCOMPARE(refused.error(), documents::DocxError::Limit);

        auto wide = documents::readDocxContent(partsWithBody(oneCellGrid(60000)));
        QVERIFY2(!wide.has_value(), "59999 padding cells must be refused");
        QCOMPARE(wide.error(), documents::DocxError::Limit);
    }

    void rowsInsideWrapperKeepCells() {
        // A w:sdt around a w:tr and its cells: the cells are read, not dropped as an unknown element.
        const QByteArray rows =
            QByteArrayLiteral("<w:sdt><w:sdtPr><w:alias w:val=\"Строки\"/></w:sdtPr><w:sdtContent>") +
            rowXml(cellXml("", paragraphXml(QStringLiteral("А"))) +
                   cellXml("", paragraphXml(QStringLiteral("Б")))) +
            QByteArrayLiteral("</w:sdtContent></w:sdt>");
        auto result = documents::readDocxContent(partsWithBody(tableXml(gridXml(2), rows)));
        QVERIFY2(result.has_value(), "a row inside a content control must parse");
        const auto *table = std::get_if<documents::Table>(&result->blocks.at(0).content);
        QVERIFY2(table != nullptr, "the only block is a table");
        QCOMPARE(table->rows.size(), qsizetype{1});
        QCOMPARE(table->rows.at(0).cells.size(), qsizetype{2});
        QStringList texts;
        collectTexts(result->blocks, texts);
        QCOMPARE(texts, (QStringList{QStringLiteral("А"), QStringLiteral("Б")}));
        QVERIFY2(result->warnings.isEmpty(), "a content control around rows is not a skipped element");
    }

    void tableNodesCountedInLimit() {
        // No w:tblGrid: the grid is the widest row. One table, one row and n empty cells are n + 2 nodes.
        auto oneRow = [](int cells) {
            QByteArray row;
            for (int i = 0; i < cells; ++i)
                row += "<w:tc/>";
            return tableXml(QByteArray(), rowXml(row));
        };
        auto accepted = documents::readDocxContent(partsWithBody(oneRow(49998)));
        QVERIFY2(accepted.has_value(), "exactly kMaxModelNodes table nodes must be accepted");
        QCOMPARE(accepted->blocks.size(), qsizetype{1});

        auto refused = documents::readDocxContent(partsWithBody(oneRow(49999)));
        QVERIFY2(!refused.has_value(), "one cell over kMaxModelNodes must be refused");
        QCOMPARE(refused.error(), documents::DocxError::Limit);
    }

    void inlineWrappersAndTracking_data() {
        QTest::addColumn<QByteArray>("paragraph");
        QTest::addColumn<QString>("expected");

        QTest::newRow("hyperlink keeps its text")
            << QByteArray("<w:p><w:hyperlink r:id=\"rId9\"><w:r><w:t>ссылка</w:t></w:r></w:hyperlink></w:p>")
            << QStringLiteral("ссылка");
        QTest::newRow("insertion keeps its text")
            << QByteArray(
                   "<w:p><w:ins w:id=\"1\" w:author=\"Иван\"><w:r><w:t>добавлено</w:t></w:r></w:ins></w:p>")
            << QStringLiteral("добавлено");
        QTest::newRow("deletion gives no text")
            << QByteArray(
                   "<w:p><w:del w:id=\"2\" w:author=\"Иван\"><w:r><w:t>удалено</w:t></w:r></w:del></w:p>")
            << QString();
        QTest::newRow("moved-from text gives no text")
            << QByteArray("<w:p><w:moveFrom w:id=\"3\"><w:r><w:t>перенесено</w:t></w:r></w:moveFrom></w:p>")
            << QString();
        QTest::newRow("delText beside text")
            << QByteArray("<w:p><w:r><w:t>основной</w:t><w:delText>удалено</w:delText></w:r></w:p>")
            << QStringLiteral("основной");
        QTest::newRow("sdt content without placeholder")
            << QByteArray("<w:p><w:sdt><w:sdtPr><w:alias w:val=\"Имя\"/></w:sdtPr><w:sdtContent>"
                          "<w:r><w:t>Контент</w:t></w:r></w:sdtContent></w:sdt></w:p>")
            << QStringLiteral("Контент");
        QTest::newRow("sdt placeholder gives no text")
            << QByteArray("<w:p><w:sdt><w:sdtPr><w:showingPlcHdr/></w:sdtPr><w:sdtContent>"
                          "<w:r><w:t>Введите текст</w:t></w:r></w:sdtContent></w:sdt></w:p>")
            << QString();
        QTest::newRow("simple field keeps its result")
            << QByteArray(
                   "<w:p><w:fldSimple w:instr=\" ADDRESS \"><w:r><w:t>Москва</w:t></w:r></w:fldSimple></w:p>")
            << QStringLiteral("Москва");
        QTest::newRow("complex field keeps only its result")
            << QByteArray("<w:p><w:r><w:fldChar w:fldCharType=\"begin\"/></w:r>"
                          "<w:r><w:instrText xml:space=\"preserve\"> ADDRESS </w:instrText></w:r>"
                          "<w:r><w:fldChar w:fldCharType=\"separate\"/></w:r>"
                          "<w:r><w:t>Москва</w:t></w:r><w:r><w:fldChar w:fldCharType=\"end\"/></w:r></w:p>")
            << QStringLiteral("Москва");
        QTest::newRow("smart tag keeps its text")
            << QByteArray("<w:p><w:smartTag w:uri=\"urn:x\" w:element=\"name\"><w:r><w:t>Иван</w:t></w:r>"
                          "</w:smartTag></w:p>")
            << QStringLiteral("Иван");
        QTest::newRow("tab and break")
            << QByteArray("<w:p><w:r><w:t>а</w:t><w:tab/><w:t>б</w:t><w:br/><w:t>в</w:t></w:r></w:p>")
            << QStringLiteral("а\tб\nв");
    }

    void inlineWrappersAndTracking() {
        QFETCH(QByteArray, paragraph);
        QFETCH(QString, expected);
        auto result = documents::readDocxContent(partsWithBody(paragraph));
        QVERIFY2(result.has_value(), "inline content must parse");
        const documents::Paragraph *only = paragraphAt(*result, 0);
        QVERIFY2(only != nullptr, "the block is a paragraph");
        QCOMPARE(only->text, expected);
        QVERIFY2(result->warnings.isEmpty(), "none of these elements is skipped with a warning");
    }

    void drawingSkippedWithWarning() {
        // A picture between two runs: its own text is absent, the text around it is kept as it is.
        const QByteArray picture =
            "<w:p><w:r><w:t>до</w:t></w:r><w:r><w:drawing><wp:inline><a:graphic>"
            "<a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
            "<pic:pic><pic:blipFill><a:blip r:embed=\"rId1\"/></pic:blipFill></pic:pic>"
            "</a:graphicData></a:graphic></wp:inline></w:drawing></w:r>"
            "<w:r><w:t>после</w:t></w:r></w:p>";
        auto result = documents::readDocxContent(partsWithBody(picture));
        QVERIFY2(result.has_value(), "a picture must not refuse the document");
        const documents::Paragraph *only = paragraphAt(*result, 0);
        QVERIFY2(only != nullptr, "the block is a paragraph");
        QCOMPARE(only->text, QStringLiteral("допосле"));
        QVERIFY2(result->warnings == QList<documents::DocxWarning>{documents::DocxWarning::ImageSkipped},
                 "exactly one ImageSkipped for the one picture");
    }

    void textBoxAndOtherSkippedCounted() {
        using documents::DocxWarning;
        // A text box in the first branch of mc:AlternateContent; the same text box in the fallback is not
        // read.
        const QByteArray textBox =
            "<w:p><mc:AlternateContent><mc:Choice Requires=\"wps\"><w:r><w:drawing><wp:anchor><a:graphic>"
            "<a:graphicData><wps:wsp><wps:txbx><w:txbxContent><w:p><w:r><w:t>Поле</w:t></w:r></w:p>"
            "</w:txbxContent></wps:txbx></wps:wsp></a:graphicData></a:graphic></wp:anchor></w:drawing></w:r>"
            "</mc:Choice><mc:Fallback><w:r><w:pict><v:shape><v:textbox><w:txbxContent><w:p><w:r><w:t>Поле"
            "</w:t></w:r></w:p></w:txbxContent></v:textbox></v:shape></w:pict></w:r></mc:Fallback>"
            "</mc:AlternateContent></w:p>";
        const QByteArray footnote = "<w:p><w:r><w:footnoteReference w:id=\"1\"/></w:r></w:p>";
        const QByteArray unknown = "<w:p><w:unknownThing><w:r><w:t>Текст</w:t></w:r></w:unknownThing></w:p>";
        const QByteArray marks =
            "<w:p><w:bookmarkStart w:id=\"0\" w:name=\"x\"/><w:proofErr w:type=\"spellStart\"/>"
            "<w:r><w:t>ок</w:t></w:r><w:bookmarkEnd w:id=\"0\"/></w:p>";

        checkWarnings(textBox, {DocxWarning::TextBoxSkipped});
        checkWarnings(footnote, {DocxWarning::OtherSkipped});
        checkWarnings(unknown, {DocxWarning::OtherSkipped});
        checkWarnings(marks, {});
        checkWarnings(textBox + footnote + unknown + marks,
                      {DocxWarning::TextBoxSkipped, DocxWarning::OtherSkipped, DocxWarning::OtherSkipped});
    }

    void alternateContentUsesChoiceOnly() {
        const QByteArray alternate =
            "<w:p><mc:AlternateContent><mc:Choice Requires=\"wps\"><w:r><w:t>Выбор</w:t></w:r></mc:Choice>"
            "<mc:Fallback><w:r><w:t>Запасной</w:t></w:r></mc:Fallback></mc:AlternateContent></w:p>";
        auto result = documents::readDocxContent(partsWithBody(alternate));
        QVERIFY2(result.has_value(), "alternate content must parse");
        const documents::Paragraph *only = paragraphAt(*result, 0);
        QVERIFY2(only != nullptr, "the block is a paragraph");
        QCOMPARE(only->text, QStringLiteral("Выбор"));
        QVERIFY2(result->warnings.isEmpty(), "the fallback branch is not read and gives no warning");
    }

    void inlineNestingBoundedForEveryWrapper_data() {
        QTest::addColumn<QByteArray>("opening");
        QTest::addColumn<QByteArray>("closing");

        QTest::newRow("hyperlink") << QByteArray("<w:hyperlink r:id=\"rId9\">")
                                   << QByteArray("</w:hyperlink>");
        QTest::newRow("ins") << QByteArray("<w:ins w:id=\"1\" w:author=\"a\">") << QByteArray("</w:ins>");
        QTest::newRow("moveTo") << QByteArray("<w:moveTo w:id=\"1\" w:author=\"a\">")
                                << QByteArray("</w:moveTo>");
        QTest::newRow("smartTag") << QByteArray("<w:smartTag w:uri=\"urn:x\" w:element=\"n\">")
                                  << QByteArray("</w:smartTag>");
        QTest::newRow("fldSimple") << QByteArray("<w:fldSimple w:instr=\" X \">")
                                   << QByteArray("</w:fldSimple>");
        QTest::newRow("sdt") << QByteArray("<w:sdt><w:sdtContent>") << QByteArray("</w:sdtContent></w:sdt>");
        QTest::newRow("customXml") << QByteArray("<w:customXml w:element=\"e\" w:uri=\"u\">")
                                   << QByteArray("</w:customXml>");
        QTest::newRow("AlternateContent") << QByteArray("<mc:AlternateContent><mc:Choice Requires=\"wps\">")
                                          << QByteArray("</mc:Choice></mc:AlternateContent>");
    }

    void inlineNestingBoundedForEveryWrapper() {
        QFETCH(QByteArray, opening);
        QFETCH(QByteArray, closing);
        const QByteArray run = "<w:r><w:t>x</w:t></w:r>";
        auto nest = [&opening, &closing, &run](int levels) {
            QByteArray xml = run;
            for (int i = 0; i < levels; ++i)
                xml = opening + xml + closing;
            return QByteArray("<w:p>") + xml + QByteArray("</w:p>");
        };

        // 64 levels are accepted and keep the text; 65 levels are refused without a result (T-02-08).
        auto accepted = documents::readDocxContent(partsWithBody(nest(64)));
        QVERIFY2(accepted.has_value(), "64 levels of one wrapper must be accepted");
        const documents::Paragraph *only = paragraphAt(*accepted, 0);
        QVERIFY2(only != nullptr, "the block is a paragraph");
        QCOMPARE(only->text, QStringLiteral("x"));

        auto refused = documents::readDocxContent(partsWithBody(nest(65)));
        QVERIFY2(!refused.has_value(), "65 levels of one wrapper must be refused");
        QCOMPARE(refused.error(), documents::DocxError::Limit);
    }

    void symbolElementsMappedOrCounted_data() {
        QTest::addColumn<QByteArray>("runXml");
        QTest::addColumn<QString>("expected");
        QTest::addColumn<int>("warnings");

        QTest::newRow("noBreakHyphen maps to U+2011")
            << QByteArray("<w:r><w:t>а</w:t><w:noBreakHyphen/><w:t>б</w:t></w:r>") << QStringLiteral("а‑б")
            << 0;
        QTest::newRow("softHyphen maps to U+00AD")
            << QByteArray("<w:r><w:t>а</w:t><w:softHyphen/><w:t>б</w:t></w:r>") << QStringLiteral("а­б") << 0;
        QTest::newRow("ptab maps to a tab")
            << QByteArray("<w:r><w:t>а</w:t><w:ptab w:relativeTo=\"margin\" w:alignment=\"right\" "
                          "w:leader=\"none\"/><w:t>б</w:t></w:r>")
            << QStringLiteral("а\tб") << 0;
        QTest::newRow("sym gives no text and one warning")
            << QByteArray("<w:r><w:t>а</w:t><w:sym w:font=\"Symbol\" w:char=\"F0B7\"/><w:t>б</w:t></w:r>")
            << QStringLiteral("аб") << 1;
    }

    void symbolElementsMappedOrCounted() {
        QFETCH(QByteArray, runXml);
        QFETCH(QString, expected);
        QFETCH(int, warnings);
        auto result =
            documents::readDocxContent(partsWithBody(QByteArray("<w:p>") + runXml + QByteArray("</w:p>")));
        QVERIFY2(result.has_value(), "a symbol element must not refuse the document");
        const documents::Paragraph *only = paragraphAt(*result, 0);
        QVERIFY2(only != nullptr, "the block is a paragraph");
        QCOMPARE(only->text, expected);
        QCOMPARE(result->warnings.count(documents::DocxWarning::OtherSkipped), qsizetype{warnings});
        QCOMPARE(result->warnings.size(), qsizetype{warnings});
    }
};

QTEST_GUILESS_MAIN(DocxContentTest)
#include "DocxContentTest.moc"
