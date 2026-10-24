#include "documents/DocxContent.h"
#include <QHash>
#include <QStringView>
#include <QXmlStreamReader>
#include <expected>
#include <utility>

namespace documents {

namespace {

// WordprocessingML namespace (confirmed by a local probe, 02-RESEARCH.md). Elements and attributes
// are compared by namespaceUri() and local name(); a prefix is never used.
constexpr QStringView kWmlNamespace = u"http://schemas.openxmlformats.org/wordprocessingml/2006/main";
constexpr QStringView kBody = u"body";
constexpr QStringView kParagraph = u"p";
constexpr QStringView kParagraphProperties = u"pPr";
constexpr QStringView kParagraphStyle = u"pStyle";
constexpr QStringView kOutlineLevel = u"outlineLvl";
constexpr QStringView kText = u"t";
constexpr QStringView kStyle = u"style";
constexpr QStringView kStyleId = u"styleId";
constexpr QStringView kStyleName = u"name";
constexpr QStringView kBasedOn = u"basedOn";
constexpr QStringView kValue = u"val";

// Encodings the OPC rule allows (OQ3, A10): UTF-8 and UTF-16 in any letter case. A document with
// no declaration is also accepted; any other declared encoding is refused.
constexpr QStringView kUtf8 = u"UTF-8";
constexpr QStringView kUtf16 = u"UTF-16";

bool isAcceptedEncoding(QStringView encoding) {
    return encoding.isEmpty() || encoding.compare(kUtf8, Qt::CaseInsensitive) == 0 ||
           encoding.compare(kUtf16, Qt::CaseInsensitive) == 0;
}

// Owns the QXmlStreamReader over one part. next() is the only read of tokens in this file.
class PartReader {
  public:
    explicit PartReader(const QByteArray &bytes) : m_xml(bytes) { m_xml.setNamespaceProcessing(true); }

    // A parser error becomes a code only; its text is never read (SAFE-05). A DOCTYPE is refused
    // at its own token, before any entity is expanded (SAFE-03, T-02-01, T-02-02).
    std::expected<QXmlStreamReader::TokenType, DocxError> next() {
        const QXmlStreamReader::TokenType token = m_xml.readNext();
        if (m_xml.hasError())
            return std::unexpected(DocxError::Corrupt);
        if (token == QXmlStreamReader::DTD)
            return std::unexpected(DocxError::UnsafeXml);
        if (token == QXmlStreamReader::StartDocument && !isAcceptedEncoding(m_xml.documentEncoding()))
            return std::unexpected(DocxError::UnsupportedEncoding);
        return token;
    }

    QXmlStreamReader &xml() { return m_xml; }

  private:
    QXmlStreamReader m_xml;
};

bool isWml(const QXmlStreamReader &xml, QStringView localName) {
    return xml.namespaceUri() == kWmlNamespace && xml.name() == localName;
}

// Value of a WordprocessingML attribute of the current element (w:val, w:styleId, ...); empty when
// the attribute is absent.
QString wmlAttribute(const QXmlStreamReader &xml, QStringView name) {
    return xml.attributes().value(kWmlNamespace, name).toString();
}

// Skips one element with its whole subtree. Called right after its StartElement was read.
std::expected<void, DocxError> skipElement(PartReader &reader) {
    qint64 depth = 0;
    while (true) {
        auto token = reader.next();
        if (!token)
            return std::unexpected(token.error());
        switch (*token) {
        case QXmlStreamReader::StartElement:
            ++depth;
            break;
        case QXmlStreamReader::EndElement:
            if (depth == 0)
                return {};
            --depth;
            break;
        case QXmlStreamReader::EndDocument:
            return std::unexpected(DocxError::Corrupt);
        default:
            break;
        }
    }
}

// Calls handler once for each direct child element of the element whose StartElement was just
// read, and returns at that element's EndElement. The handler is called right after the child's
// StartElement and must consume the child whole (skipElement, or a reader of its own).
template <typename Handler>
std::expected<void, DocxError> forEachChild(PartReader &reader, Handler &&handler) {
    while (true) {
        auto token = reader.next();
        if (!token)
            return std::unexpected(token.error());
        switch (*token) {
        case QXmlStreamReader::StartElement: {
            auto handled = handler(reader);
            if (!handled)
                return std::unexpected(handled.error());
            break;
        }
        case QXmlStreamReader::EndElement:
            return {};
        case QXmlStreamReader::EndDocument:
            return std::unexpected(DocxError::Corrupt);
        default:
            break;
        }
    }
}

// Moves to the root element of a part (the next StartElement). Its children are read by the caller.
std::expected<void, DocxError> enterRoot(PartReader &reader) {
    while (true) {
        auto token = reader.next();
        if (!token)
            return std::unexpected(token.error());
        if (*token == QXmlStreamReader::StartElement)
            return {};
        if (*token == QXmlStreamReader::EndDocument)
            return std::unexpected(DocxError::Corrupt);
    }
}

// Reads the rest of a part, so that a DOCTYPE, a parser error or trailing damage after the root
// is found; the content is not kept.
std::expected<void, DocxError> readToEnd(PartReader &reader) {
    while (true) {
        auto token = reader.next();
        if (!token)
            return std::unexpected(token.error());
        if (*token == QXmlStreamReader::EndDocument)
            return {};
    }
}

// The paragraph properties this plan reads from w:pPr: the style id of the paragraph and the
// outline level set in the element itself. The same record holds the values of a style's w:pPr.
struct ParagraphProps {
    QString styleId;       // w:pStyle, empty when absent
    int outlineLevel = -1; // w:outlineLvl, -1 when absent or outside 0..9
};

// Reads w:pPr right after its StartElement. Only direct children are read: w:pPrChange (earlier
// formatting) and every other child are skipped whole.
std::expected<void, DocxError> readParagraphProps(PartReader &reader, ParagraphProps &props) {
    return forEachChild(reader, [&props](PartReader &child) -> std::expected<void, DocxError> {
        if (isWml(child.xml(), kParagraphStyle)) {
            props.styleId = wmlAttribute(child.xml(), kValue);
        } else if (isWml(child.xml(), kOutlineLevel)) {
            bool ok = false;
            const int level = wmlAttribute(child.xml(), kValue).toInt(&ok);
            props.outlineLevel = ok && level >= 0 && level <= 9 ? level : -1;
        }
        return skipElement(child);
    });
}

// One w:style of word/styles.xml: the fields this plan reads (02-RESEARCH.md, Pattern 3).
struct StyleInfo {
    QString styleId;
    QString name;          // w:name, empty when absent
    QString basedOn;       // w:basedOn, empty when absent
    int outlineLevel = -1; // w:pPr/w:outlineLvl, -1 when absent or outside 0..9
};

// Styles by styleId. A part is read once, before the body, so this table is complete when used.
using StyleTable = QHash<QString, StyleInfo>;

// Reads one w:style right after its StartElement. A style without styleId is consumed and dropped.
// A repeated styleId keeps the first definition.
std::expected<void, DocxError> readStyle(PartReader &reader, StyleTable &styles) {
    StyleInfo style;
    style.styleId = wmlAttribute(reader.xml(), kStyleId);
    auto children = forEachChild(reader, [&style](PartReader &child) -> std::expected<void, DocxError> {
        if (isWml(child.xml(), kStyleName)) {
            style.name = wmlAttribute(child.xml(), kValue);
        } else if (isWml(child.xml(), kBasedOn)) {
            style.basedOn = wmlAttribute(child.xml(), kValue);
        } else if (isWml(child.xml(), kParagraphProperties)) {
            ParagraphProps props;
            if (auto read = readParagraphProps(child, props); !read)
                return read;
            style.outlineLevel = props.outlineLevel;
            return {};
        }
        return skipElement(child);
    });
    if (!children)
        return children;
    if (!style.styleId.isEmpty() && !styles.contains(style.styleId))
        styles.insert(style.styleId, style);
    return {};
}

// Reads word/styles.xml into the style table. Every token is checked, so a DOCTYPE or a damaged
// part is refused here, before the body is read (D-07, SAFE-03). An empty part has no styles.
std::expected<StyleTable, DocxError> readStyles(const QByteArray &bytes) {
    StyleTable styles;
    if (bytes.isEmpty())
        return styles;
    PartReader reader(bytes);
    if (auto root = enterRoot(reader); !root)
        return std::unexpected(root.error());
    auto children = forEachChild(reader, [&styles](PartReader &child) -> std::expected<void, DocxError> {
        if (isWml(child.xml(), kStyle))
            return readStyle(child, styles);
        return skipElement(child);
    });
    if (!children)
        return std::unexpected(children.error());
    if (auto end = readToEnd(reader); !end)
        return std::unexpected(end.error());
    return styles;
}

// Heading number from the text after "Heading" in a styleId: exactly one digit 1-9, otherwise 0.
int headingDigit(QStringView text) {
    if (text.size() != 1 || text.front() < u'1' || text.front() > u'9')
        return 0;
    return text.front().unicode() - u'0';
}

// Task 1 rule: the paragraph's styleId names a style of the table and is "Heading" plus one digit.
// Task 2 replaces this with the full order (direct outlineLvl, basedOn chain, name, outline level).
int headingLevel(const ParagraphProps &props, const StyleTable &styles) {
    if (props.styleId.isEmpty() || !styles.contains(props.styleId))
        return 0;
    const QStringView styleId = props.styleId;
    if (!styleId.startsWith(u"Heading"))
        return 0;
    return headingDigit(styleId.sliced(7));
}

// Running totals for one readDocxContent call. They are not per paragraph: text and nodes of
// tables count toward the same totals in plan 02-03.
struct ParseBudget {
    qint64 textChars = 0; // UTF-16 units of all accepted text (D-03)
    qint64 nodes = 0;     // model nodes created so far (OQ9, T-02-02)
};

// Reads one w:p up to its end tag. Text comes only from w:t; every other child is skipped whole,
// except a w:pPr directly under w:p, whose properties go to props. Called right after the
// StartElement of w:p. Text that would pass kMaxTextChars is refused before it is appended, so
// no partial text escapes.
std::expected<Paragraph, DocxError> readParagraph(PartReader &reader, ParseBudget &budget,
                                                  ParagraphProps &props) {
    Paragraph paragraph;
    qint64 depth = 0;      // open elements inside w:p; 0 means the w:p itself is current
    qint64 textDepth = -1; // depth of the open w:t, -1 when no w:t is open
    while (true) {
        auto token = reader.next();
        if (!token)
            return std::unexpected(token.error());
        switch (*token) {
        case QXmlStreamReader::StartElement:
            if (depth == 0 && isWml(reader.xml(), kParagraphProperties)) {
                // The properties are consumed whole; their children hold no text.
                if (auto read = readParagraphProps(reader, props); !read)
                    return std::unexpected(read.error());
                break;
            }
            ++depth;
            if (textDepth < 0 && isWml(reader.xml(), kText))
                textDepth = depth;
            break;
        case QXmlStreamReader::EndElement:
            if (depth == 0)
                return paragraph;
            if (depth == textDepth)
                textDepth = -1;
            --depth;
            break;
        case QXmlStreamReader::Characters:
            if (textDepth >= 0) {
                // Every character token is kept as is, whitespace and CDATA included (DOCX-06).
                const QStringView chunk = reader.xml().text();
                budget.textChars += chunk.size();
                if (budget.textChars > documents::kMaxTextChars)
                    return std::unexpected(DocxError::Limit);
                paragraph.text.append(chunk);
            }
            break;
        case QXmlStreamReader::EndDocument:
            return std::unexpected(DocxError::Corrupt);
        default:
            break;
        }
    }
}

// Protective pass over a part whose content is not read in this plan (numbering; 02-02, Task 3).
// It reads to the end, so a DOCTYPE, a bad encoding or a damaged part is refused with no partial
// result (D-07). An empty part has nothing to check.
std::expected<void, DocxError> checkPartIsSafe(const QByteArray &bytes) {
    if (bytes.isEmpty())
        return {};
    PartReader reader(bytes);
    while (true) {
        auto token = reader.next();
        if (!token)
            return std::unexpected(token.error());
        if (*token == QXmlStreamReader::EndDocument)
            return {};
    }
}

} // namespace

std::expected<DocxDocument, DocxError> readDocxContent(const DocxParts &parts) {
    if (parts.documentXml.isEmpty())
        return std::unexpected(DocxError::Corrupt);

    // Styles are read before the body (Task 1 of 02-02); numbering passes only the safety check
    // until its content is read (Task 3 of 02-02).
    auto styles = readStyles(parts.stylesXml);
    if (!styles)
        return std::unexpected(styles.error());
    if (auto numbering = checkPartIsSafe(parts.numberingXml); !numbering)
        return std::unexpected(numbering.error());

    PartReader reader(parts.documentXml);

    // Find w:body. Anything before it (the w:document root and its other children) is not read.
    bool bodyFound = false;
    while (!bodyFound) {
        auto token = reader.next();
        if (!token)
            return std::unexpected(token.error());
        if (*token == QXmlStreamReader::EndDocument)
            return std::unexpected(DocxError::Corrupt);
        bodyFound = *token == QXmlStreamReader::StartElement && isWml(reader.xml(), kBody);
    }

    DocxDocument document;
    document.warnings = parts.warnings;
    ParseBudget budget;

    // Direct children of w:body. Each child is consumed whole, so an EndElement seen here
    // at this level is the end of w:body.
    while (true) {
        auto token = reader.next();
        if (!token)
            return std::unexpected(token.error());
        switch (*token) {
        case QXmlStreamReader::StartElement:
            if (isWml(reader.xml(), kParagraph)) {
                // The node is counted before it is read, so a flood of empty paragraphs stops at the limit.
                if (++budget.nodes > documents::kMaxModelNodes)
                    return std::unexpected(DocxError::Limit);
                ParagraphProps props;
                auto paragraph = readParagraph(reader, budget, props);
                if (!paragraph)
                    return std::unexpected(paragraph.error());
                paragraph->headingLevel = headingLevel(props, *styles);
                document.blocks.append(Block{std::move(*paragraph)});
            } else {
                // Other body children (tables, drawings, fields) are not read in this plan (02-03).
                auto skipped = skipElement(reader);
                if (!skipped)
                    return std::unexpected(skipped.error());
            }
            break;
        case QXmlStreamReader::EndElement:
            return document;
        case QXmlStreamReader::EndDocument:
            return std::unexpected(DocxError::Corrupt);
        default:
            break;
        }
    }
}

} // namespace documents
