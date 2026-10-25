#pragma once

#include "documents/DocxContainer.h"
#include "documents/DocxModel.h"
#include <expected>

namespace documents {

// Reads the body of word/document.xml into the model. Takes the bytes of the parts, never a
// path (D-05). Returns a code and no partial result on any failure (D-07). No XML error text
// is returned, so nothing from the document reaches the caller through an error (SAFE-05).
std::expected<DocxDocument, DocxError> readDocxContent(const DocxParts &parts);

} // namespace documents
