#include "documents/DocxContent.h"
#include <QStringView>
#include <QXmlStreamReader>
#include <expected>
#include <utility>

namespace documents {

namespace {

// WordprocessingML namespace (confirmed by a local probe, 02-RESEARCH.md). Elements are compared
// by namespaceUri() and local name(); a prefix is never used.
constexpr QStringView kWmlNamespace = u"http://schemas.openxmlformats.org/wordprocessingml/2006/main";
constexpr QStringView kBody = u"body";
constexpr QStringView kParagraph = u"p";
constexpr QStringView kText = u"t";

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

// Running totals for one readDocxContent call. They are not per paragraph: text and nodes of
// tables count toward the same totals in plan 02-03.
struct ParseBudget {
    qint64 textChars = 0; // UTF-16 units of all accepted text (D-03)
    qint64 nodes = 0;     // model nodes created so far (OQ9, T-02-02)
};

// Reads one w:p up to its end tag. Text comes only from w:t; every other child is skipped whole.
// Called right after the StartElement of w:p. Text that would pass kMaxTextChars is refused
// before it is appended, so no partial text escapes.
std::expected<Paragraph, DocxError> readParagraph(PartReader &reader, ParseBudget &budget) {
    Paragraph paragraph;
    qint64 depth = 0;      // open elements inside w:p; 0 means the w:p itself is current
    qint64 textDepth = -1; // depth of the open w:t, -1 when no w:t is open
    while (true) {
        auto token = reader.next();
        if (!token)
            return std::unexpected(token.error());
        switch (*token) {
        case QXmlStreamReader::StartElement:
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

// Protective pass over a part whose content is not read in this plan (styles, numbering; 02-02).
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

    // The two optional parts pass the same checks before the body is read (D-10: no content used).
    if (auto styles = checkPartIsSafe(parts.stylesXml); !styles)
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
                auto paragraph = readParagraph(reader, budget);
                if (!paragraph)
                    return std::unexpected(paragraph.error());
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
