#pragma once

#include "documents/DocxTypes.h"
#include <QList>
#include <QString>
#include <QtGlobal>
#include <variant>

// Model of the document body, held in memory only. This is the contract phase 3 draws from.
namespace documents {

// Bullet markers and ordered numbers; the kind is taken from numbering.xml in plan 02-02.
enum class ListKind { Bullet, Ordered };

// Contract: the text is exact (no trimming); styles and numbering fill the optional fields.
struct Paragraph {
    QString text;
    int headingLevel = 0;
    int listLevel = -1;
    ListKind listKind = ListKind::Bullet;
    QString listMarker;
};

struct Block;

// Contract: a cell owns its nested blocks in document order; startColumn is the grid column.
struct TableCell {
    int startColumn = 0;
    int columnSpan = 1;
    int rowSpan = 1;
    QList<Block> blocks;
};

// Contract: a row lists only the cells that start in it; cells are in grid order.
struct TableRow {
    QList<TableCell> cells;
};

// Contract: gridColumns is the width of the table grid; rows are in document order.
struct Table {
    int gridColumns = 0;
    QList<TableRow> rows;
};

// Contract: one block is one paragraph or one table; blocks keep the order of the document.
struct Block {
    std::variant<Paragraph, Table> content;
};

// Contract: blocks are in document order; warnings holds one code per skipped or approximated
// item, so its size is the count of such items. Neither field carries a path or an error text.
struct DocxDocument {
    QList<Block> blocks;
    QList<DocxWarning> warnings;
};

} // namespace documents
