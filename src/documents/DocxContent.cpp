#include "documents/DocxContent.h"
#include <QHash>
#include <QSet>
#include <QStringView>
#include <QXmlStreamReader>
#include <array>
#include <cstddef>
#include <expected>
#include <optional>
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
constexpr QStringView kNumPr = u"numPr";
constexpr QStringView kNumId = u"numId";
constexpr QStringView kIlvl = u"ilvl";
constexpr QStringView kText = u"t";
constexpr QStringView kStyle = u"style";
constexpr QStringView kStyleId = u"styleId";
constexpr QStringView kStyleName = u"name";
constexpr QStringView kBasedOn = u"basedOn";
constexpr QStringView kAbstractNum = u"abstractNum";
constexpr QStringView kAbstractNumId = u"abstractNumId";
constexpr QStringView kNum = u"num";
constexpr QStringView kLevel = u"lvl";
constexpr QStringView kStart = u"start";
constexpr QStringView kNumFmt = u"numFmt";
constexpr QStringView kLvlText = u"lvlText";
constexpr QStringView kValue = u"val";
constexpr QStringView kBullet = u"bullet";
constexpr QStringView kDecimal = u"decimal";
constexpr QStringView kDecimalLvlText = u"%1.";
constexpr QStringView kTable = u"tbl";
constexpr QStringView kTableGrid = u"tblGrid";
constexpr QStringView kGridColumn = u"gridCol";
constexpr QStringView kRow = u"tr";
constexpr QStringView kCell = u"tc";
constexpr QStringView kCellProperties = u"tcPr";

// Encodings the OPC rule allows (OQ3, A10): UTF-8 and UTF-16 in any letter case. A document with
// no declaration is also accepted; any other declared encoding is refused.
constexpr QStringView kUtf8 = u"UTF-8";
constexpr QStringView kUtf16 = u"UTF-16";

// Nine list levels, w:ilvl 0 to 8 (ISO/IEC 29500-1).
constexpr int kListLevels = 9;

// Largest start value kept for a numbered level; a larger one is replaced by it (T-02-07).
constexpr qint64 kMaxListStart = 1000000;

// Marker of a bullet level: U+2022. The text of w:lvlText is never copied as a marker (A5).
constexpr char16_t kBulletCharacter = u'•';

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

// Heading number that follows the word part of a styleId or a name: exactly one digit 1-9, otherwise
// 0. The match is exact: "heading 10" and "Заголовок" without a digit are not headings.
int headingDigit(QStringView text) {
    if (text.size() != 1 || text.front() < u'1' || text.front() > u'9')
        return 0;
    return text.front().unicode() - u'0';
}

// The reference of a w:numPr: the numId (absent when it is not a number) and the level.
struct NumPr {
    std::optional<qint64> numId;
    int ilvl = 0; // 0 when absent or not a number, 8 at most (T-02-07)
};

// w:ilvl of a numPr: 0 when absent, not a number or negative; 8 when above 8.
int readIlvl(const QString &text) {
    bool ok = false;
    const qint64 value = text.toLongLong(&ok);
    if (!ok || value < 0)
        return 0;
    return value > kListLevels - 1 ? kListLevels - 1 : static_cast<int>(value);
}

// Reads w:numPr right after its StartElement: w:numId and w:ilvl, both with w:val.
std::expected<void, DocxError> readNumPr(PartReader &reader, NumPr &numPr) {
    return forEachChild(reader, [&numPr](PartReader &child) -> std::expected<void, DocxError> {
        if (isWml(child.xml(), kNumId)) {
            bool ok = false;
            const qint64 value = wmlAttribute(child.xml(), kValue).toLongLong(&ok);
            if (ok)
                numPr.numId = value;
            else
                numPr.numId.reset();
        } else if (isWml(child.xml(), kIlvl)) {
            numPr.ilvl = readIlvl(wmlAttribute(child.xml(), kValue));
        }
        return skipElement(child);
    });
}

// The paragraph properties this plan reads from w:pPr: the style id, the outline level and the
// numbering reference set in the element itself. The same record holds the values of a style's w:pPr.
struct ParagraphProps {
    QString styleId;            // w:pStyle, empty when absent
    int outlineLevel = -1;      // w:outlineLvl, -1 when absent or outside 0..9
    std::optional<NumPr> numPr; // w:numPr, absent when the element is not there
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
        } else if (isWml(child.xml(), kNumPr)) {
            NumPr numPr;
            if (auto read = readNumPr(child, numPr); !read)
                return read;
            props.numPr = numPr;
            return {};
        }
        return skipElement(child);
    });
}

// One w:style of word/styles.xml: the fields this plan reads (02-RESEARCH.md, Pattern 3 and 4).
struct StyleInfo {
    QString styleId;
    QString name;                // w:name, empty when absent
    QString basedOn;             // w:basedOn, empty when absent
    int outlineLevel = -1;       // w:pPr/w:outlineLvl, -1 when absent or outside 0..9
    std::optional<qint64> numId; // w:pPr/w:numPr/w:numId; the level of that numPr is ignored (A13)
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
            if (props.numPr)
                style.numId = props.numPr->numId;
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

// Longest basedOn chain that is followed (T-02-06). A deeper chain is cut at this many styles.
constexpr int kMaxStyleChainDepth = 16;

// Styles of the chain that starts at startId: the style itself, then the style it is based on, and
// so on. The walk ends at a missing style, at kMaxStyleChainDepth styles, or at a style that is
// already in the chain. A cycle in basedOn is bad style data, not broken markup, so it ends the
// walk without an error.
QList<const StyleInfo *> styleChain(const StyleTable &styles, QString startId) {
    QList<const StyleInfo *> chain;
    QString id = std::move(startId);
    while (!id.isEmpty() && chain.size() < kMaxStyleChainDepth) {
        const auto found = styles.constFind(id);
        if (found == styles.constEnd())
            break;
        const StyleInfo *style = &found.value();
        if (chain.contains(style))
            break;
        chain.append(style);
        id = style->basedOn;
    }
    return chain;
}

// Heading level from a styleId: "Heading" followed by one digit 1-9 (the English internal id), else 0.
int headingFromStyleId(QStringView styleId) {
    if (!styleId.startsWith(u"Heading"))
        return 0;
    return headingDigit(styleId.sliced(7));
}

// Heading level from a style name, compared in lower case: "heading N" (English internal name) or
// "Заголовок N" (Russian). Assumption A1: the Russian form is checked only with synthetic fixtures.
int headingFromName(const QString &name) {
    const QString lower = name.toLower();
    const QStringView text = lower;
    if (text.startsWith(u"heading "))
        return headingDigit(text.sliced(8));
    if (text.startsWith(u"заголовок "))
        return headingDigit(text.sliced(10));
    return 0;
}

// Heading level from an outline level: 0-8 gives the level plus one, and 9 means "not a heading",
// which gives 0. nullopt when the outline level is absent, so the search goes on.
std::optional<int> headingFromOutline(int outlineLevel) {
    if (outlineLevel < 0)
        return std::nullopt;
    if (outlineLevel == 9)
        return 0;
    return outlineLevel + 1;
}

// The answer of one style in the chain: a level 1-9, 0 for "not a heading" (outline level 9), or
// nullopt when the style does not decide, so the next style in the chain is asked.
std::optional<int> headingOfStyle(const StyleInfo &style) {
    if (const int byId = headingFromStyleId(style.styleId); byId > 0)
        return byId;
    if (const int byName = headingFromName(style.name); byName > 0)
        return byName;
    return headingFromOutline(style.outlineLevel);
}

// Heading level of a paragraph (DOCX-03). The checks run in this order:
// 1. the paragraph's own w:outlineLvl: 0-8 gives the level plus one, 9 gives 0 (direct formatting wins);
// 2. the chain of its style, one style at a time: styleId, then name, then outlineLvl; the first style
//    that decides wins, and a style with outline level 9 decides 0;
// 3. otherwise 0.
// Assumption A1 (02-RESEARCH.md): the Russian name "Заголовок N" is checked only with synthetic
// fixtures in this plan. A check on a file saved by Word is planned for phase 3 (TEST-02).
int headingLevel(const ParagraphProps &props, const StyleTable &styles) {
    if (const auto direct = headingFromOutline(props.outlineLevel))
        return *direct;
    for (const StyleInfo *style : styleChain(styles, props.styleId)) {
        if (const auto decided = headingOfStyle(*style))
            return *decided;
    }
    return 0;
}

// One w:lvl of an abstract numbering definition: what the marker of its paragraphs looks like.
struct NumLevel {
    bool bullet = false;       // numFmt is "bullet"
    bool approximated = false; // the number format is not reproduced exactly (OQ5)
    qint64 start = 1;          // first number of the level, clamped (T-02-07)
    QString pStyle;            // w:pStyle: the paragraph style linked to this level, empty when absent
};

// An abstract numbering definition with nine levels. A level that the part does not describe keeps
// the defaults of NumLevel: decimal, start 1, not approximated.
struct AbstractNum {
    std::array<NumLevel, kListLevels> levels{};
};

// word/numbering.xml: w:num maps a numId to an abstractNumId, w:abstractNum holds the levels.
struct NumberingTable {
    QHash<qint64, qint64> numToAbstract;
    QHash<qint64, AbstractNum> abstracts;
};

// Start value of a numbered level: a negative or non-numeric value becomes 1, and a value above
// kMaxListStart becomes kMaxListStart (T-02-07).
qint64 readStart(const QString &text) {
    bool ok = false;
    const qint64 value = text.toLongLong(&ok);
    if (!ok || value < 0)
        return 1;
    return value > kMaxListStart ? kMaxListStart : value;
}

// Reads one w:lvl of an abstractNum right after its StartElement. A level whose w:ilvl is not 0 to
// 8 is consumed and dropped. w:lvlText is read only to decide whether the format is approximated
// (A5): it is never kept, and a bullet level never uses it.
std::expected<void, DocxError> readLevel(PartReader &reader, AbstractNum &abstract) {
    bool validIlvl = false;
    const qint64 ilvl = wmlAttribute(reader.xml(), kIlvl).toLongLong(&validIlvl);
    if (!validIlvl || ilvl < 0 || ilvl >= kListLevels)
        return skipElement(reader);

    NumLevel level;
    std::optional<QString> numFmt;
    std::optional<QString> lvlText;
    auto children = forEachChild(reader, [&](PartReader &child) -> std::expected<void, DocxError> {
        if (isWml(child.xml(), kStart)) {
            level.start = readStart(wmlAttribute(child.xml(), kValue));
        } else if (isWml(child.xml(), kNumFmt)) {
            numFmt = wmlAttribute(child.xml(), kValue);
        } else if (isWml(child.xml(), kLvlText)) {
            lvlText = wmlAttribute(child.xml(), kValue);
        } else if (isWml(child.xml(), kParagraphStyle)) {
            level.pStyle = wmlAttribute(child.xml(), kValue);
        }
        return skipElement(child);
    });
    if (!children)
        return children;

    // A missing w:numFmt is decimal. The format is reproduced for bullet and for decimal with the
    // "%1." text. Any other format is approximated, and so is decimal with another text (OQ5).
    const QStringView format = numFmt ? QStringView(*numFmt) : kDecimal;
    level.bullet = format == kBullet;
    const bool decimal = format == kDecimal;
    level.approximated = !level.bullet && (!decimal || (lvlText && QStringView(*lvlText) != kDecimalLvlText));
    abstract.levels[static_cast<std::size_t>(ilvl)] = level;
    return {};
}

// Reads one w:abstractNum right after its StartElement. Its w:lvl children fill its levels. An
// abstractNumId that is not a number makes the definition unusable, so it is dropped.
std::expected<void, DocxError> readAbstractNum(PartReader &reader, NumberingTable &numbering) {
    bool validId = false;
    const qint64 id = wmlAttribute(reader.xml(), kAbstractNumId).toLongLong(&validId);
    AbstractNum abstract;
    auto children = forEachChild(reader, [&abstract](PartReader &child) -> std::expected<void, DocxError> {
        if (isWml(child.xml(), kLevel))
            return readLevel(child, abstract);
        return skipElement(child);
    });
    if (!children)
        return children;
    if (validId && !numbering.abstracts.contains(id))
        numbering.abstracts.insert(id, abstract);
    return {};
}

// Reads one w:num right after its StartElement: its numId and the abstractNumId it refers to.
// w:lvlOverride and w:startOverride are not read in this plan.
std::expected<void, DocxError> readNum(PartReader &reader, NumberingTable &numbering) {
    bool validId = false;
    const qint64 numId = wmlAttribute(reader.xml(), kNumId).toLongLong(&validId);
    std::optional<qint64> abstractId;
    auto children = forEachChild(reader, [&abstractId](PartReader &child) -> std::expected<void, DocxError> {
        if (isWml(child.xml(), kAbstractNumId)) {
            bool ok = false;
            const qint64 value = wmlAttribute(child.xml(), kValue).toLongLong(&ok);
            if (ok)
                abstractId = value;
        }
        return skipElement(child);
    });
    if (!children)
        return children;
    if (validId && abstractId && !numbering.numToAbstract.contains(numId))
        numbering.numToAbstract.insert(numId, *abstractId);
    return {};
}

// Reads word/numbering.xml into the numbering table. Every token is checked, as for styles.
// An empty part has no numbering.
std::expected<NumberingTable, DocxError> readNumbering(const QByteArray &bytes) {
    NumberingTable numbering;
    if (bytes.isEmpty())
        return numbering;
    PartReader reader(bytes);
    if (auto root = enterRoot(reader); !root)
        return std::unexpected(root.error());
    auto children = forEachChild(reader, [&numbering](PartReader &child) -> std::expected<void, DocxError> {
        if (isWml(child.xml(), kAbstractNum))
            return readAbstractNum(child, numbering);
        if (isWml(child.xml(), kNum))
            return readNum(child, numbering);
        return skipElement(child);
    });
    if (!children)
        return std::unexpected(children.error());
    if (auto end = readToEnd(reader); !end)
        return std::unexpected(end.error());
    return numbering;
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

// The numbering reference of a paragraph: the numId and level from its own numPr, or else from the
// numPr of its style chain (A13). fromStyle tells which of the two it is.
struct NumRef {
    qint64 numId;
    int ilvl;
    bool fromStyle;
};

// Own numPr with a numId first, then the first style in the chain that has a numId. nullopt when
// neither gives one.
std::optional<NumRef> listReference(const ParagraphProps &props, const StyleTable &styles) {
    if (props.numPr && props.numPr->numId)
        return NumRef{*props.numPr->numId, props.numPr->ilvl, false};
    for (const StyleInfo *style : styleChain(styles, props.styleId)) {
        if (style->numId)
            return NumRef{*style->numId, 0, true};
    }
    return std::nullopt;
}

// Level of a paragraph that takes its list from its style: the lowest level whose w:pStyle is the
// paragraph's style, or 0 when no level names it (A13, ISO/IEC 29500-1).
int levelForStyle(const AbstractNum &abstract, const QString &styleId) {
    if (styleId.isEmpty())
        return 0;
    for (int ilvl = 0; ilvl < kListLevels; ++ilvl) {
        if (abstract.levels[static_cast<std::size_t>(ilvl)].pStyle == styleId)
            return ilvl;
    }
    return 0;
}

// Counters of one numId: per level, the last number and whether the level has started since its
// last restart. Restart: a higher level (smaller ilvl) has an item, see advanceCounter.
struct ListCounter {
    std::array<qint64, kListLevels> value{};
    std::array<bool, kListLevels> started{};
};

// State of the lists of one readDocxContent call.
struct ListState {
    QHash<qint64, ListCounter> counters; // by numId
    QSet<qint64> warned;                 // numIds that already have their ListFormatApproximated
};

// One item at level ilvl: the first item after a restart takes the level's start value, later items
// add one. Every deeper level restarts (the next item there starts from its start value again).
void advanceCounter(ListCounter &counter, int ilvl, qint64 start) {
    if (counter.started[static_cast<std::size_t>(ilvl)]) {
        ++counter.value[static_cast<std::size_t>(ilvl)];
    } else {
        counter.value[static_cast<std::size_t>(ilvl)] = start;
        counter.started[static_cast<std::size_t>(ilvl)] = true;
    }
    for (int deeper = ilvl + 1; deeper < kListLevels; ++deeper)
        counter.started[static_cast<std::size_t>(deeper)] = false;
}

// Sets the list fields of a paragraph (DOCX-04): the kind, the level and the marker. A bullet level
// gets U+2022; any other format gets the decimal number of its counter and a dot (OQ5). The
// approximation warning is added once per numId, in the order the lists are met.
// The text of w:lvlText is never a marker (A5).
void applyList(Paragraph &paragraph, const ParagraphProps &props, const StyleTable &styles,
               const NumberingTable &numbering, ListState &state, QList<DocxWarning> &warnings) {
    const auto reference = listReference(props, styles);
    if (!reference || reference->numId == 0) // numId 0 means "no numbering" (A4)
        return;
    const auto abstractId = numbering.numToAbstract.constFind(reference->numId);
    if (abstractId == numbering.numToAbstract.constEnd())
        return;
    const auto found = numbering.abstracts.constFind(abstractId.value());
    if (found == numbering.abstracts.constEnd())
        return;
    const AbstractNum &abstract = found.value();

    const int ilvl = reference->fromStyle ? levelForStyle(abstract, props.styleId) : reference->ilvl;
    const NumLevel &level = abstract.levels[static_cast<std::size_t>(ilvl)];
    paragraph.listLevel = ilvl;
    paragraph.listKind = level.bullet ? ListKind::Bullet : ListKind::Ordered;

    ListCounter &counter = state.counters[reference->numId];
    advanceCounter(counter, ilvl, level.start);
    if (level.bullet)
        paragraph.listMarker = QString(QChar(kBulletCharacter));
    else
        paragraph.listMarker = QString::number(counter.value[static_cast<std::size_t>(ilvl)]) + u'.';

    if (level.approximated && !state.warned.contains(reference->numId)) {
        state.warned.insert(reference->numId);
        warnings.append(DocxWarning::ListFormatApproximated);
    }
}

// Everything one readDocxContent call keeps while it walks the body: the reader, the style and
// numbering tables, the list counters, the budgets and the warnings in document order.
struct ReadState {
    PartReader &reader;
    const StyleTable &styles;
    const NumberingTable &numbering;
    ListState lists;
    ParseBudget budget;
    QList<DocxWarning> warnings;
};

// Counts one model node (OQ9, T-02-02). False when the count passes kMaxModelNodes; the caller
// then refuses the document before the node is kept.
bool countNode(ReadState &st) { return ++st.budget.nodes <= documents::kMaxModelNodes; }

std::expected<void, DocxError> readBlockChild(ReadState &st, QList<Block> &blocks);

// Calls readBlockChild for each child of the current element up to its end tag.
std::expected<void, DocxError> readBlocks(ReadState &st, QList<Block> &blocks) {
    return forEachChild(st.reader, [&st, &blocks](PartReader &) { return readBlockChild(st, blocks); });
}

// Number of w:gridCol children of w:tblGrid, read right after its StartElement.
std::expected<int, DocxError> readGridColumns(PartReader &reader) {
    int columns = 0;
    auto children = forEachChild(reader, [&columns](PartReader &child) -> std::expected<void, DocxError> {
        if (isWml(child.xml(), kGridColumn))
            ++columns;
        return skipElement(child);
    });
    if (!children)
        return std::unexpected(children.error());
    return columns;
}

// Reads one w:tc right after its StartElement into row. Its blocks are read in document order; the
// cell starts at the row cursor, which moves past it by one column for now (gridSpan comes later).
std::expected<void, DocxError> readCell(ReadState &st, QList<TableCell> &cells, int &cursor) {
    if (!countNode(st))
        return std::unexpected(DocxError::Limit);
    TableCell cell;
    cell.startColumn = cursor;
    cell.columnSpan = 1;
    cell.rowSpan = 1;
    cursor += cell.columnSpan;
    auto children = forEachChild(st.reader, [&st, &cell](PartReader &) -> std::expected<void, DocxError> {
        if (isWml(st.reader.xml(), kCellProperties))
            return skipElement(st.reader);
        return readBlockChild(st, cell.blocks);
    });
    if (!children)
        return std::unexpected(children.error());
    cells.append(std::move(cell));
    return {};
}

// Reads one w:tr right after its StartElement and appends it to rows.
std::expected<void, DocxError> readRow(ReadState &st, QList<TableRow> &rows) {
    if (!countNode(st))
        return std::unexpected(DocxError::Limit);
    TableRow row;
    int cursor = 0;
    auto children =
        forEachChild(st.reader, [&st, &row, &cursor](PartReader &) -> std::expected<void, DocxError> {
            if (isWml(st.reader.xml(), kCell))
                return readCell(st, row.cells, cursor);
            return skipElement(st.reader);
        });
    if (!children)
        return std::unexpected(children.error());
    rows.append(std::move(row));
    return {};
}

// Reads one w:tbl right after its StartElement. Only w:tblGrid and w:tr are read here; the grid
// width is the number of w:gridCol, or the widest row when there is no grid.
std::expected<Table, DocxError> readTable(ReadState &st) {
    if (!countNode(st))
        return std::unexpected(DocxError::Limit);
    Table table;
    std::optional<int> gridColumns;
    auto children =
        forEachChild(st.reader, [&st, &table, &gridColumns](PartReader &) -> std::expected<void, DocxError> {
            if (isWml(st.reader.xml(), kTableGrid)) {
                auto columns = readGridColumns(st.reader);
                if (!columns)
                    return std::unexpected(columns.error());
                gridColumns = *columns;
                return {};
            }
            if (isWml(st.reader.xml(), kRow))
                return readRow(st, table.rows);
            return skipElement(st.reader);
        });
    if (!children)
        return std::unexpected(children.error());
    table.gridColumns = gridColumns.value_or(0);
    return table;
}

// One block child of the current element (w:body, w:tc or another block container), read whole:
// a paragraph or a table becomes a block; any other element is skipped for now.
std::expected<void, DocxError> readBlockChild(ReadState &st, QList<Block> &blocks) {
    if (isWml(st.reader.xml(), kParagraph)) {
        // The node is counted before it is read, so a flood of empty paragraphs stops at the limit.
        if (!countNode(st))
            return std::unexpected(DocxError::Limit);
        ParagraphProps props;
        auto paragraph = readParagraph(st.reader, st.budget, props);
        if (!paragraph)
            return std::unexpected(paragraph.error());
        paragraph->headingLevel = headingLevel(props, st.styles);
        applyList(*paragraph, props, st.styles, st.numbering, st.lists, st.warnings);
        blocks.append(Block{std::move(*paragraph)});
        return {};
    }
    if (isWml(st.reader.xml(), kTable)) {
        auto table = readTable(st);
        if (!table)
            return std::unexpected(table.error());
        blocks.append(Block{std::move(*table)});
        return {};
    }
    return skipElement(st.reader);
}

} // namespace

std::expected<DocxDocument, DocxError> readDocxContent(const DocxParts &parts) {
    if (parts.documentXml.isEmpty())
        return std::unexpected(DocxError::Corrupt);

    // Styles and numbering are read before the body (02-RESEARCH.md, Patterns 3 and 4). Each part is
    // checked token by token, so a DOCTYPE, a bad encoding or a damaged part is refused here (D-07).
    auto styles = readStyles(parts.stylesXml);
    if (!styles)
        return std::unexpected(styles.error());
    auto numbering = readNumbering(parts.numberingXml);
    if (!numbering)
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

    // Children of w:body up to its end tag; the end of w:body is the end of the walk.
    ReadState st{reader, *styles, *numbering, {}, {}, parts.warnings};
    DocxDocument document;
    if (auto body = readBlocks(st, document.blocks); !body)
        return std::unexpected(body.error());
    document.warnings = std::move(st.warnings);
    return document;
}

} // namespace documents
