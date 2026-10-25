#include "application/DocumentImport.h"
#include "documents/DocxContainer.h"
#include "documents/DocxTypes.h"
#include <QFile>
#include <QFileInfo>
#include <utility>

namespace DocumentImport {

namespace {

// Russian texts (D-04): a short cause and a hint. Each names only the file (D-05). The
// texts are owner-pending: they are shown in the UI in phase 3 and must be reviewed there.
// Each message is built from the file name only; no path and no document content is used.

QString unreadableText(const QString &fileName) {
    return QStringLiteral("Не удалось открыть файл «%1». Проверьте, что он существует и доступен для чтения.")
        .arg(fileName);
}

// Two placeholders: the multi-argument arg() replaces both in one pass, so a file name that
// contains "%2" is not replaced a second time.
QString tooLargeText(const QString &fileName) {
    constexpr qint64 bytesPerMb = qint64{1024} * 1024;
    const qint64 limitMb = documents::kMaxFileBytes / bytesPerMb;
    return QStringLiteral(
               "Файл «%1» больше %2 МБ. Уменьшите файл (например, удалите изображения) и повторите.")
        .arg(fileName, QString::number(limitMb));
}

// One text per DocxError code. No default branch, so -Wswitch can flag a new code without a
// text. The project sets no warning flags (no -Wall), so this is a review point, not a guarantee.
QString errorText(documents::DocxError code, const QString &fileName) {
    switch (code) {
    case documents::DocxError::NotZip:
        return QStringLiteral("Файл «%1» не является DOCX: это не ZIP-архив. Сохраните резюме в Word как "
                              ".docx и повторите.")
            .arg(fileName);
    case documents::DocxError::Corrupt:
        return QStringLiteral("Файл «%1» повреждён или обрезан. Сохраните резюме заново в Word и повторите.")
            .arg(fileName);
    case documents::DocxError::Encrypted:
        return QStringLiteral("Файл «%1» зашифрован. Сохраните резюме без пароля и повторите.").arg(fileName);
    case documents::DocxError::LegacyDoc:
        return QStringLiteral("Файл «%1» в старом формате .doc, а не .docx. Откройте его в Word, сохраните "
                              "как .docx и повторите.")
            .arg(fileName);
    case documents::DocxError::TooLarge:
        return tooLargeText(fileName);
    case documents::DocxError::Limit:
        return QStringLiteral("Файл «%1» отклонён: после распаковки содержимое слишком большое или состоит "
                              "из слишком многих частей. Сохраните резюме заново в Word и повторите.")
            .arg(fileName);
    case documents::DocxError::MissingPart:
        return QStringLiteral(
                   "В файле «%1» не найден текст резюме. Сохраните его как .docx в Word и повторите.")
            .arg(fileName);
    case documents::DocxError::UnsafeXml:
        return QStringLiteral(
                   "Файл «%1» содержит DTD или внешние сущности XML, поэтому не открыт из соображений "
                   "безопасности. Сохраните резюме в Word как .docx заново и повторите.")
            .arg(fileName);
    case documents::DocxError::UnsupportedEncoding:
        return QStringLiteral(
                   "Файл «%1» записан в кодировке XML, которую приложение не читает (поддерживаются UTF-8 и "
                   "UTF-16). Сохраните резюме в Word как .docx заново и повторите.")
            .arg(fileName);
    }
    return {};
}

// One text per DocxWarning code. Same rule as errorText: no default branch.
QString warningText(documents::DocxWarning code, const QString &fileName) {
    switch (code) {
    case documents::DocxWarning::StylesPartMissing:
        return QStringLiteral(
                   "В файле «%1» нет описания стилей: заголовки могут отображаться как обычный текст.")
            .arg(fileName);
    case documents::DocxWarning::NumberingPartMissing:
        return QStringLiteral(
                   "В файле «%1» нет описания нумерации: списки могут отображаться без маркеров и номеров.")
            .arg(fileName);
    case documents::DocxWarning::ListFormatApproximated:
        return QStringLiteral(
                   "В файле «%1» есть список с нумерацией, которую приложение показывает упрощённо: номера "
                   "показаны в виде «1.», «2.».")
            .arg(fileName);
    }
    return {};
}

} // namespace

std::expected<ImportedDocx, QString> readDocxFile(const QString &path) {
    // Every message uses the file name only (D-05). The path is not stored past this scope.
    const QFileInfo info(path);
    const QString fileName = info.fileName();

    // 1. Only an existing regular file can be imported.
    if (!info.isFile())
        return std::unexpected(unreadableText(fileName));

    // 2. Size check before any open (D-01, T-01-09): an oversized file is never read.
    if (info.size() > documents::kMaxFileBytes)
        return std::unexpected(tooLargeText(fileName));

    // 3. Read-only open. Nothing is created, changed or deleted on disk (T-01-11).
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::unexpected(unreadableText(fileName));

    // 4. Bounded read: one byte past the limit detects a file that grew after the size check.
    QByteArray bytes = file.read(documents::kMaxFileBytes + 1);
    const bool readFailed = file.error() != QFileDevice::NoError;
    file.close();
    if (readFailed)
        return std::unexpected(unreadableText(fileName));
    if (bytes.size() > documents::kMaxFileBytes)
        return std::unexpected(tooLargeText(fileName));

    // 5. The container layer takes bytes, never a path (plan 01-02). Its error code becomes a text.
    auto parts = documents::readDocxParts(std::move(bytes));
    if (!parts)
        return std::unexpected(errorText(parts.error(), fileName));

    ImportedDocx result;
    result.documentXml = std::move(parts->documentXml);
    result.stylesXml = std::move(parts->stylesXml);
    result.numberingXml = std::move(parts->numberingXml);
    for (const documents::DocxWarning code : parts->warnings)
        result.warnings.append(warningText(code, fileName));
    return result;
}

} // namespace DocumentImport
