# Phase 2: Чтение содержимого DOCX - Pattern Map

**Mapped:** 2026-10-24
**Files analyzed:** 11 (новых и изменяемых, по 02-RESEARCH.md)
**Analogs found:** 10 / 11 (модель блоков и разбор XML аналога в репозитории нет)

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `src/documents/DocxModel.h` (новый) | model (типы блоков) | transform | `src/documents/DocxTypes.h` + `src/domain/Resume.h` | role-match (header-only типы, namespace `documents`) |
| `src/documents/DocxContent.h` / `.cpp` (новый) | service (разбор XML в модель) | transform (streaming XML) | `src/documents/DocxContainer.h` / `.cpp` | role-match (namespace-функция, `std::expected`, коды без текста) |
| `src/documents/DocxTypes.h` (изменить: новые коды, `kMaxTextChars`) | utility (коды и лимиты) | transform | `src/documents/DocxTypes.h` (сам файл) | exact |
| `src/documents/DocxContainer.h` / `.cpp` (изменить: вызов разбора, перечень частей) | service | transform + file-I/O | сам файл | exact |
| `src/documents/ZipArchive.h` / `.cpp` (изменить: `entryNames()`, если выбран вариант колонтитулов) | service (wrapper libzip) | file-I/O (из памяти) | сам файл (`hasEntry`, `readEntry`) | exact |
| `src/application/DocumentImport.h` / `.cpp` (изменить: поле модели, тексты новых кодов) | service (application) | request-response | сам файл (`ImportedDocx`, `errorText`, `warningText`) | exact |
| `tests/DocxContentTest.cpp` (новый) | test | batch (синтетические фикстуры) | `tests/ZipArchiveTest.cpp` / `tests/ResumeJsonTest.cpp` | role-match (Qt Test, `QTEST_GUILESS_MAIN`) |
| `tests/DocxFixtures.h` (изменить: билдеры styles/numbering/document) | test utility | batch | сам файл (`buildDocxLike`, `buildStoredZip`) | exact |
| `tests/DocumentImportTest.cpp` (изменить: счётчик текстов, строка 179) | test | file-I/O | сам файл | exact |
| `CMakeLists.txt` (изменить: `resume_documents` и `docx_content_test`) | config | — | сам файл (блоки `resume_core`, `resume_json_test`) | exact |
| `src/documents/README.md` (обновить таблицу кодов) | docs | — | `src/documents/README.md` | exact |

## Pattern Assignments

### `src/documents/DocxModel.h` (model, transform)

**Analog:** `src/documents/DocxTypes.h` (стиль заголовка и namespace), `src/documents/DocxContainer.h` (структура с `QList`).

**Заголовок и namespace** (`src/documents/DocxTypes.h`, строки 1-8):
```cpp
#pragma once

#include <QtGlobal>

// Error and warning codes plus limit constants of the documents layer. This file holds
// codes and numbers only: user-facing texts live in the application layer (D-04, D-05),
// never here.
namespace documents {
```

**Структура с контейнерами** (`src/documents/DocxContainer.h`, строки 1-17):
```cpp
#pragma once

#include "documents/DocxTypes.h"
#include <QByteArray>
#include <QList>
#include <expected>

namespace documents {

// The three DOCX parts this layer reads, held in memory. Only these parts are read (D-10).
struct DocxParts {
    QByteArray documentXml;
    QByteArray stylesXml;
    QByteArray numberingXml;
    // One entry per missing optional part (D-08, D-09); the size is the count.
    QList<DocxWarning> warnings;
};
```

Перенять: `#pragma once`, `namespace documents`, комментарий-контракт над типом, `QList` для коллекций. Модель: `Paragraph` (текст, `headingLevel`, `listKind`, `listLevel`, `listMarker`), `Table`/`TableRow`/`TableCell` (`startColumn`, `columnSpan`, `rowSpan`, вложенные `QList<Block>`), `Block` (`std::variant<Paragraph, Table>`), `DocxDocument` (`QList<Block>`, `QList<DocxWarning>`). Рекурсия `Cell { QList<Block> }` компилируется (RESEARCH.md, раздел Code Examples). Заголовки и списки в модели хранятся как готовые поля, без XML.

---

### `src/documents/DocxContent.h` / `.cpp` (service, transform streaming XML)

**Analog:** `src/documents/DocxContainer.h` / `.cpp` (namespace-функция, `std::expected`, анонимный namespace с helper-ами).

**Сигнатура публичной функции** (`src/documents/DocxContainer.h`, строки 19-26):
```cpp
// Classifies the input by its leading bytes, then reads the three parts from memory.
// Takes bytes, never a path (Pitfall 2, D-05). Returns a code and no partial result on
// any failure (D-07).
std::expected<DocxParts, DocxError> readDocxParts(QByteArray bytes,
                                                  qint64 unpackedBudget = kMaxUnpackedBytes);
```
Для `DocxContent`: `std::expected<DocxDocument, DocxError> readDocxContent(const DocxParts& parts);`. Тот же контракт D-07: ни частичного результата, ни текста ошибки.

**Анонимный namespace с константами и helper-ами** (`src/documents/DocxContainer.cpp`, строки 10-14, 24-30):
```cpp
namespace documents {

namespace {

constexpr std::string_view kDocumentEntry = "word/document.xml";
constexpr std::string_view kStylesEntry = "word/styles.xml";
constexpr std::string_view kNumberingEntry = "word/numbering.xml";

// Compares the leading bytes with an explicit length, so a short or empty input stays in
// bounds and a NUL inside the signature is compared as data, not as a terminator.
template <std::size_t N>
bool startsWithSignature(const QByteArray &bytes, const std::array<char, N> &signature) {
```

**Ранний отказ без частичного результата** (`src/documents/DocxContainer.cpp`, строки 86-89 и 93-97):
```cpp
    auto document = readPart(kDocumentEntry);
    if (!document)
        return std::unexpected(document.error());
    parts.documentXml = std::move(*document);
```
Перенять для каждой части: `if (!part) return std::unexpected(part.error());`. Так `hasError()` и лимиты возвращают код без текста.

**Ядро разбора (нет аналога в репозитории).** Скелет из RESEARCH.md, Pattern 1 (`QXmlStreamReader`, `setNamespaceProcessing(true)`, отказ на `DTD`, `hasError()` без `errorString()`). Ветки сравнивать по `namespaceUri()` и `name()`, префикс не использовать. Текст `w:t` собирать из всех `Characters` без `isWhitespace()` и `trimmed()` (RESEARCH.md, Pattern 3). Проверка `kMaxTextChars` при накоплении текста, превышение — `DocxError::Limit`.

Структура кода: одна функция обхода с стеком элементов; для таблиц и `sdt` предел глубины (предложение 64, `[ASSUMED]`, A9).

---

### `src/documents/DocxTypes.h` (utility, коды и лимиты; изменить)

**Analog:** сам файл. Новые коды добавлять в существующие `enum class`, а не заводить новый файл.

**Текущие коды и лимит с меткой фазы** (`src/documents/DocxTypes.h`, строки 10-25 и 47-48):
```cpp
enum class DocxError {
    NotZip,
    Corrupt,
    Encrypted,
    LegacyDoc,
    TooLarge,
    Limit,
    MissingPart,
};

// Non-fatal findings. ...
enum class DocxWarning {
    StylesPartMissing,
    NumberingPartMissing,
};
...
// D-03, enforced in Phase 2.
inline constexpr qint64 kMaxTextChars = 200000;
```
Добавить предложенные коды (`UnsafeXml` для DTD, по решению владельца также `UnsupportedEncoding`) и предупреждения пропусков (`TextBoxSkipped`, `ImageSkipped`, `HeaderFooterSkipped`, `OtherSkipped`). Пометку «D-03, enforced in Phase 2» обновить, когда проверка появится. Новые коды обязательно добавить в `switch` в `DocumentImport.cpp` (см. ниже).

---

### `src/documents/DocxContainer.h` / `.cpp` и `src/documents/ZipArchive.h` / `.cpp` (изменить)

**Analog:** сам файл.

**Класс `ZipArchive`, метод чтения по имени с общим бюджетом** (используется в `src/documents/DocxContainer.cpp`, строки 76-82):
```cpp
    qint64 remaining = unpackedBudget;
    auto readPart = [&archive, &remaining](std::string_view name) {
        auto part = archive.readEntry(name, remaining);
        if (part)
            remaining -= part->size();
        return part;
    };
```

**Пропуск отсутствующей необязательной части — предупреждение** (`src/documents/DocxContainer.cpp`, строки 93-100):
```cpp
    if (archive.hasEntry(kStylesEntry)) {
        ...
    } else {
        parts.warnings.append(DocxWarning::StylesPartMissing);
    }
```
Для колонтитулов: `ZipArchive::entryNames()` (только имена, уже читаются в `checkEntryStructure`, `src/documents/ZipArchive.cpp`, строки 40-48, но не сохраняются) и подсчёт записей `word/header*.xml` и `word/footer*.xml`; на каждую — `DocxWarning` (по одному на экземпляр, SAFE-04). Владелец решает по Open Question 2 в RESEARCH.md.

---

### `src/application/DocumentImport.h` / `.cpp` (service, request-response; изменить)

**Analog:** сам файл.

**Структура результата** (`src/application/DocumentImport.h`, строки 10-17):
```cpp
// The three DOCX parts read from one file. warnings holds one Russian text per missing
// optional part (D-08, D-09); its size is the count of such parts.
struct ImportedDocx {
    QByteArray documentXml;
    QByteArray stylesXml;
    QByteArray numberingXml;
    QStringList warnings;
};
```
Добавить поле `documents::DocxDocument document;` (Open Question 6: три поля байтов оставить до фазы 3).

**Тексты по коду без `default`** (`src/application/DocumentImport.cpp`, строки 31-60 и 62-75):
```cpp
// One text per DocxError code. No default branch, so -Wswitch can flag a new code without a
// text. The project sets no warning flags (no -Wall), so this is a review point, not a guarantee.
QString errorText(documents::DocxError code, const QString &fileName) {
    switch (code) {
    case documents::DocxError::NotZip:
        return QStringLiteral("Файл «%1» не является DOCX: это не ZIP-архив. ...").arg(fileName);
    ...
    }
    return {};
}
```
Новый код добавлять сюда и в `warningText`. Текст строить только из `fileName`, без текста документа (SAFE-05).

**Сообщения с именем файла, без пути** (`src/application/DocumentImport.cpp`, строки 79-82):
```cpp
std::expected<ImportedDocx, QString> readDocxFile(const QString &path) {
    // Every message uses the file name only (D-05). The path is not stored past this scope.
    const QFileInfo info(path);
    const QString fileName = info.fileName();
```

---

### `tests/DocxContentTest.cpp` (test, batch; новый)

**Analog:** `tests/ResumeJsonTest.cpp` (каркас Qt Test), `tests/ZipArchiveTest.cpp` (фикстуры DOCX через `fixtures::`).

**Каркас** (`tests/ResumeJsonTest.cpp`, строки 1-8 и 73-74, сокращённо):
```cpp
#include "application/ResumeJson.h"
#include <QJsonArray>
#include <QtTest>

class ResumeJsonTest : public QObject {
    Q_OBJECT
  private slots:
    void roundTrip() {
```
```cpp
QTEST_GUILESS_MAIN(ResumeJsonTest)
#include "ResumeJsonTest.moc"
```
Для `DocxContentTest`: тот же вид, класс `DocxContentTest`, `#include "documents/DocxContent.h"`, `#include "DocxContentTest.moc"`.

**Данные теста через `_data()`** (`tests/ResumeJsonTest.cpp`, строки 42-64): `QTest::addColumn` + `QTest::newRow` + `QFETCH`. Использовать для таблицы DOCX-03/04/08 и SAFE-03.

**Перехват сообщений для SAFE-05** (нет аналога в репозитории; API в RESEARCH.md, раздел Code Examples): `qInstallMessageHandler`, маркер из фикстуры (в тексте, в имени сущности, в атрибуте) не должен попасть в сообщения.

---

### `tests/DocxFixtures.h` (test utility, изменить)

**Analog:** сам файл (`buildDocxLike`, строки 124-130; `buildStoredZip`, строки 53-112).

**Сборка DOCX-подобного архива** (`tests/DocxFixtures.h`, строки 124-130):
```cpp
// A DOCX-shaped archive: [Content_Types].xml plus word/document.xml with the given body.
inline QByteArray buildDocxLike(const QByteArray &documentXml) {
    return buildStoredZip({
        ZipEntry{"[Content_Types].xml", "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Types/>"},
        ZipEntry{"word/document.xml", documentXml},
    });
}
```
Новые билдеры: `buildDocxParts(document, styles, numbering)` по этому образцу, с `ZipEntry{"word/styles.xml", ...}` и `ZipEntry{"word/numbering.xml", ...}`. Строки XML — через `QByteArray` литералы, исходники UTF-8. Бинарных файлов не коммитить (Pitfall 10 фазы 1).

---

### `tests/DocumentImportTest.cpp` (test, изменить)

**Analog:** сам файл. Счётчик текстов жёстко зафиксирован (`tests/DocumentImportTest.cpp`, строка 179):
```cpp
QCOMPARE(texts.size(), qsizetype{5});
```
При добавлении кода обновить и `switch` в `DocumentImport.cpp`, и это число в одном коммите (RESEARCH.md, Pitfall 10).

---

### `CMakeLists.txt` (config, изменить)

**Analog:** сам файл, блоки `resume_core` и `resume_json_test`.

**Библиотека** (образец по `CMakeLists.txt`, строки 10-18):
```cmake
add_library(resume_core STATIC
    src/domain/Resume.h
    src/application/ResumeJson.h
    ...
)
target_link_libraries(resume_core PUBLIC Qt6::Core Qt6::Sql)
target_include_directories(resume_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
```
Добавить в `resume_documents` файлы `src/documents/DocxModel.h`, `src/documents/DocxContent.h`, `src/documents/DocxContent.cpp`.

**Цель-тест** (`CMakeLists.txt`, строки 82-84):
```cmake
qt_add_executable(resume_json_test tests/ResumeJsonTest.cpp)
target_link_libraries(resume_json_test PRIVATE resume_core Qt6::Test)
add_test(NAME resume_json COMMAND resume_json_test)
```
Новая: `qt_add_executable(docx_content_test tests/DocxContentTest.cpp)`, `target_link_libraries(docx_content_test PRIVATE resume_documents Qt6::Test)`, `add_test(NAME docx_content COMMAND docx_content_test)`.

---

### `src/documents/README.md` (docs, обновить)

**Analog:** сам файл. Обновить таблицу кодов `DocxError`/`DocxWarning` и правило «текст не попадает в слой documents» (AGENTS.md, раздел 1: слои).

---

## Shared Patterns

### Коды без текста и тексты в application-слое
**Source:** `src/documents/DocxTypes.h`, строки 5-7; `src/application/DocumentImport.cpp`, строки 31-75
**Apply to:** `DocxContent.*`, `DocxModel.h`, `DocumentImport.*`
Слой `documents` возвращает только коды (`DocxError`, `DocxWarning`). Русские тексты строятся только в `DocumentImport.cpp` через `switch` без `default`.

### Ошибка без содержимого документа (SAFE-05)
**Source:** `src/application/DocumentImport.cpp`, строки 79-82 (только `fileName()`); `src/application/ResumeJson.cpp`, строки 11-15 (`fail`)
**Apply to:** все новые функции разбора и импорта
```cpp
QString errorText(documents::DocxError code, const QString &fileName)
```
Не передавать `QXmlStreamReader::errorString()` наружу: при неизвестной сущности там имя из документа (RESEARCH.md, Pitfall 2).

### Отказ без частичного результата (D-07)
**Source:** `src/documents/DocxContainer.cpp`, строки 46-48, 66-68, 86-88
**Apply to:** `readDocxContent`, лимит текста, отказ по DTD
```cpp
auto opened = ZipArchive::open(std::move(bytes));
if (!opened)
    return std::unexpected(opened.error());
```

### Лимиты как `inline constexpr` в `namespace documents`
**Source:** `src/documents/DocxTypes.h`, строки 29-48
**Apply to:** `DocxContent.cpp` (`kMaxTextChars`, предел глубины — новая константа в этом же файле, если только она не нужна других слоям)

### Владение и отсутствие пути в слое documents
**Source:** `src/documents/DocxContainer.h`, строка 20 («Takes bytes, never a path»)
**Apply to:** `readDocxContent` принимает `DocxParts` (байты), не путь.

### Qt Test
**Source:** `tests/ResumeJsonTest.cpp`, строки 73-74; `tests/DocxFixtures.h`, строки 14-15 (фикстуры в коде)
**Apply to:** `tests/DocxContentTest.cpp`

### Форматирование
**Source:** `.clang-format`: `BasedOnStyle: LLVM`, `IndentWidth: 4`, `ColumnLimit: 110`
**Apply to:** все новые `.cpp` / `.h`; проверка `clang-format --dry-run --Werror <файлы>`.

---

## No Analog Found

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| `src/documents/DocxContent.cpp` (ядро обхода `QXmlStreamReader`) | service | streaming transform | В репозитории нет разбора XML; использовать RESEARCH.md, Pattern 1-5 (с пометками `[ASSUMED]`) |
| `src/documents/DocxModel.h` (блоки, таблицы с `startColumn`/`columnSpan`/`rowSpan`) | model | transform | Нет моделей документа с вложенными блоками; ориентироваться на RESEARCH.md, раздел Code Examples |
| Нумерация списков (`numId` → `abstractNum` → `numFmt`) | service | transform | Нет разбора `numbering.xml`; RESEARCH.md, Pattern 4 |

---

## Metadata

**Analog search scope:** `src/documents/`, `src/application/`, `tests/`, корень `CMakeLists.txt`
**Files scanned:** 11 (прочитаны полностью или по нужным строкам; `ZipArchive.h`/`.cpp` только через ссылки в `DocxContainer.cpp` и RESEARCH.md)
**Tracked-source check:** все пути аналогов из `git ls-files -- src/documents src/application tests` (вывод этой сессии): `src/documents/DocxContainer.{h,cpp}`, `DocxTypes.h`, `ZipArchive.{h,cpp}`, `README.md`; `src/application/DocumentImport.{h,cpp}`, `ResumeJson.{h,cpp}`; `tests/DocxFixtures.h`, `DocumentImportTest.cpp`, `ZipArchiveTest.cpp`, `ResumeJsonTest.cpp`. Gitignored-зеркал не использовано.
**Не читались:** содержимое реальных резюме (не требовалось, фикстуры синтетические).
**Pattern extraction date:** 2026-10-24
