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

// Owns the QXmlStreamReader over one part. next() is the only read of tokens in this file.
class PartReader {
  public:
    explicit PartReader(const QByteArray &bytes) : m_xml(bytes) { m_xml.setNamespaceProcessing(true); }

    // A parser error becomes a code only; its text is never read (SAFE-05).
    std::expected<QXmlStreamReader::TokenType, DocxError> next() {
        const QXmlStreamReader::TokenType token = m_xml.readNext();
        if (m_xml.hasError())
            return std::unexpected(DocxError::Corrupt);
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

// Reads one w:p up to its end tag. Text comes only from w:t; every other child is skipped whole.
// Called right after the StartElement of w:p.
std::expected<Paragraph, DocxError> readParagraph(PartReader &reader) {
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
            if (textDepth >= 0)
                paragraph.text.append(reader.xml().text());
            break;
        case QXmlStreamReader::EndDocument:
            return std::unexpected(DocxError::Corrupt);
        default:
            break;
        }
    }
}

} // namespace

std::expected<DocxDocument, DocxError> readDocxContent(const DocxParts &parts) {
    if (parts.documentXml.isEmpty())
        return std::unexpected(DocxError::Corrupt);

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

    // Direct children of w:body. Each child is consumed whole, so an EndElement seen here
    // at this level is the end of w:body.
    while (true) {
        auto token = reader.next();
        if (!token)
            return std::unexpected(token.error());
        switch (*token) {
        case QXmlStreamReader::StartElement:
            if (isWml(reader.xml(), kParagraph)) {
                auto paragraph = readParagraph(reader);
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
