#pragma once

// Error codes of the documents layer. This file holds codes only: user-facing
// texts live in the application layer (D-04, D-05), never here.
namespace documents {

enum class DocxError {
    NotZip,
    Corrupt,
    Encrypted,
    LegacyDoc,
    TooLarge,
    Limit,
    MissingPart,
};

} // namespace documents
