# Phase 1: Зависимость libzip и безопасное чтение ZIP - Pattern Map

**Mapped:** 2026-10-24
**Files analyzed:** 11 (новых и изменяемых)
**Analogs found:** 9 / 11

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `src/documents/ZipArchive.h` / `.cpp` (новый) | service (wrapper над libzip) | file-I/O (из памяти) + transform | `src/storage/ResumeRepository.h` | role-match (RAII, запрет копирования, `QString* error`) |
| `src/documents/DocxError.h` / `.cpp` (новый) | utility (enum кодов) | transform | `src/application/ResumeJson.h` | partial (namespace-API с ошибками) |
| `src/application/DocumentImport.h` / `.cpp` (новый) | service (application) | file-I/O + request-response | `src/application/ResumeJson.h` / `.cpp` | role-match (слой application, проверки и текст ошибок) |
| `src/documents/README.md` (заменить заглушку) | docs | — | `src/documents/README.md` (текущая заглушка) | exact (та же запись) |
| `tests/ZipArchiveTest.cpp` (новый) | test | batch (синтетические фикстуры) | `tests/ResumeJsonTest.cpp` | exact (Qt Test, `QTEST_GUILESS_MAIN`) |
| `tests/ZipArchiveTest.cpp`, фикстуры во временной папке | test | file-I/O | `tests/ResumeE2ETest.cpp` | role-match (`QTemporaryDir`) |
| `CMakeLists.txt` (изменить) | config | — | `CMakeLists.txt` (блоки `resume_core`, `resume_json_test`) | exact |
| `.github/workflows/ci.yml` (изменить) | config (CI) | — | `.github/workflows/ci.yml` (блоки `install:`) | exact |
| `cmake/DeployMinGWRuntime.cmake` (проверить, менять только при необходимости) | config (deploy) | — | сам файл | exact |
| `installer/AIResumeAssistant.iss` (проверить, менять только при необходимости) | config (installer) | — | сам файл | exact |
| `docs/memory.md` (обновить после задачи) | docs | — | `docs/memory.md` | exact |

## Pattern Assignments

### `src/documents/ZipArchive.h` / `.cpp` (service, file-I/O из памяти)

**Analog:** `src/storage/ResumeRepository.h` (RAII-класс с владением ресурсом)

**Класс с владением ресурсом и запретом копирования** (`src/storage/ResumeRepository.h`, строки 7-12):
```cpp
class ResumeRepository {
public:
    explicit ResumeRepository(QString databasePath);
    ~ResumeRepository();
    ResumeRepository(const ResumeRepository&) = delete;
    ResumeRepository& operator=(const ResumeRepository&) = delete;
```
Копировать для `ZipArchive`: конструктор принимает `QByteArray` по значению, деструктор закрывает `zip_t*` до освобождения буфера, оба оператора копирования удалены.

**Приватные члены и статический helper ошибки** (`src/storage/ResumeRepository.h`, строки 16-20):
```cpp
private:
    QString databasePath_;
    QString connectionName_;
    QSqlDatabase db_;
    static bool fail(QString* error, const QString& message);
```
Использовать суффикс `_` для членов (как здесь), а не `m_` из RESEARCH.md. Это расхождение с RESEARCH.md: в проекте уже есть `databasePath_`, поэтому нужно сохранить стиль `_`. Helper `fail(QString*, ...)` переиспользовать для возврата ошибки без исключений.

**Порядок деструкции (из RESEARCH.md, Pattern 1):** члены объявляются так, чтобы `QByteArray` жил дольше `zip_t*`. Деструктор явно закрывает архив (`zip_discard`) до того, как буфер уничтожается. Это соответствует правилу владения из libzip (`zip_source_buffer`), см. Pitfall 7.

**Core pattern для libzip** (без аналога в репозитории; взять из RESEARCH.md, раздел Code Examples, с пометкой ASSUMED):
```cpp
zip_error_t err;
zip_error_init(&err);
zip_source_t* src = zip_source_buffer_create(m_bytes.constData(), m_bytes.size(), 0, &err);
m_archive = zip_open_from_source(src, ZIP_RDONLY | ZIP_CHECKCONS, &err);
```
Имена `zip_open_from_source`, `ZIP_CHECKCONS`, `zip_error_init` не проверены по `zip.h`. Сверить после установки пакета (A2 в RESEARCH.md).

**Error handling:** не использовать `zip_error_strerror()` в текстах (D-04, Pitfall в RESEARCH.md). Возвращать коды из `DocxError`, а не строки libzip.

---

### `src/documents/DocxError.h` / `.cpp` (utility, enum кодов)

**Analog:** `src/application/ResumeJson.h` (namespace с публичным API и опциональным выходом ошибки)

**Стиль заголовка** (`src/application/ResumeJson.h`, строки 1-13):
```cpp
#pragma once
#include "domain/Resume.h"
#include <QJsonObject>
#include <QString>
#include <optional>

namespace ResumeJson {

QJsonObject toJson(const Resume& resume);
// Validates untrusted input; the resume id is never read from the body.
std::optional<Resume> fromJson(const QJsonObject& object, QString* error = nullptr);

} // namespace ResumeJson
```
Для `DocxError` использовать `#pragma once`, `enum class DocxError { NotZip, Corrupt, Encrypted, LegacyDoc, TooLarge, Limit, MissingPart }` в namespace. Тексты здесь не хранить (D-04 задаёт их в application-слое). Комментарий-«контракт» в стиле строки 10 (`// Validates untrusted input ...`).

---

### `src/application/DocumentImport.h` / `.cpp` (service, file-I/O + request-response)

**Analog:** `src/application/ResumeJson.h` / `.cpp` (application-слой, проверка входа, сообщения)

**Заголовок и единый стиль ошибки** (`src/application/ResumeJson.cpp`, строки 1-15):
```cpp
#include "application/ResumeJson.h"
#include <QJsonArray>
#include <QJsonValue>

namespace {

constexpr int kMaxShortText = 200;
constexpr int kMaxDescription = 10000;
constexpr int kMaxStages = 200;

bool fail(QString* error, const QString& message) {
    if (error)
        *error = message;
    return false;
}
```
Перенять: `constexpr` константы в анонимном namespace (лимиты D-01, D-02, D-03 — `kMaxDocxBytes`, `kMaxUnpackedBytes`, `kMaxResumeChars`), функцию `fail` в анонимном namespace. Не использовать `#include "..."` со своим именем для Qt-классов: Qt-заголовки подключать как `<QFile>`.

**Проверка входа с понятным сообщением** (`src/application/ResumeJson.cpp`, строки 17-34, сокращённо):
```cpp
bool readText(const QJsonObject& object, const QString& key, int maxLength, bool required, QString& out,
              QString* error) {
    ...
    if (out.size() > maxLength)
        return fail(error, QString("Field '%1' is too long").arg(key));
```
Перенять шаблон проверки длины с `return fail(error, ...)`. Внимание: существующие сообщения в `ResumeJson` написаны по-английски (`"Field '%1' is required"`). D-04 требует русских текстов для DOCX. Не копировать язык сообщений, только структуру (короткое сообщение с подстановкой имени поля). Имя файла подставлять через `QFileInfo(path).fileName()` (D-05), а не полный путь.

**Core pattern для DocumentImport** (чтение файла, без аналога в репозитории):
- `QFileInfo(path).size()` до чтения: если больше `kMaxDocxBytes`, сразу `TooLarge` (D-01).
- `QFile` в режиме `ReadOnly`, `read(kMaxDocxBytes + 1)`; путь с кириллицей работает через `QString` (wide API на Windows). Передавать в `ZipArchive` уже `QByteArray`, а не путь.
- Коды `DocxError` сопоставлять с русскими текстами здесь же (D-04, D-06).

**Существующий образец хранения (для справки):** `src/storage/ResumeRepository.h`, строки 13-14 — `bool open(QString* error = nullptr); bool save(Resume& resume, QString* error = nullptr);`. Для `DocumentImport` использовать ту же сигнатуру: результат + `QString* error`, либо `std::expected`, если выбран C++23-вариант из RESEARCH.md. Выбрать один стиль и держаться его в фазе; в проекте сейчас используется `std::optional` + `QString* error`.

---

### `tests/ZipArchiveTest.cpp` (test, batch со синтетическими фикстурами)

**Analog:** `tests/ResumeJsonTest.cpp` (Qt Test, unit, без UI)

**Шаблон целиком** (`tests/ResumeJsonTest.cpp`, строки 1-8 и 73-74):
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
Для `ZipArchiveTest`: тот же каркас, класс `ZipArchiveTest`, `#include "documents/ZipArchive.h"`, `#include "ZipArchiveTest.moc"`. `CMAKE_AUTOMOC ON` включён в `CMakeLists.txt` (строка 6), `.moc` подключать вручную, как в образце.

**Проверки через QVERIFY/QCOMPARE и данные QTest:** `tests/ResumeJsonTest.cpp`, строки 42-64 — `rejectsInvalidInput_data()` с `QTest::addColumn` и `QTest::newRow`, затем `QFETCH`. Использовать для набора «не-ZIP / обрезанный ZIP / CFB с EncryptedPackage / CFB без него / MissingPart»:
```cpp
void rejectsInvalidInput() {
    QFETCH(QJsonObject, json);
    QString error;
    QVERIFY(!ResumeJson::fromJson(json, &error).has_value());
    QVERIFY(!error.isEmpty());
}
```

**Фикстуры на диске (если нужны для проверки лимита размера 10 MB и пути с кириллицей):** `tests/ResumeE2ETest.cpp`, строки 9-12:
```cpp
void createSaveReopenAndUpdate() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("test.sqlite");
```
Для кириллического пути создавать `QTemporaryDir`, затем вложенный каталог с кириллическим именем. Не класть `.docx`/`.zip` в `tests/` (Pitfall 10 в RESEARCH.md). Синтетические ZIP генерировать в памяти через libzip (`zip_source_buffer` с `ZIP_CREATE`), CFB собирать байтами.

**Проверка «ничего не записано»:** снимок `QDir` до и после (RESEARCH.md, раздел Validation Architecture). Образца в репозитории нет: `[no analog]`.

---

### `CMakeLists.txt` (config, изменить)

**Analog:** сам файл. Два блока-образца.

**Новая статическая библиотека** (`CMakeLists.txt`, строки 10-18):
```cmake
add_library(resume_core STATIC
    src/domain/Resume.h
    src/application/ResumeJson.h
    src/application/ResumeJson.cpp
    src/storage/ResumeRepository.h
    src/storage/ResumeRepository.cpp
)
target_link_libraries(resume_core PUBLIC Qt6::Core Qt6::Sql)
target_include_directories(resume_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
```
Добавить `resume_documents` по этому же образцу: список файлов `src/documents/*.h/.cpp`, `target_link_libraries(resume_documents PUBLIC Qt6::Core)`, затем `find_package(libzip CONFIG REQUIRED)` и `target_link_libraries(resume_documents PUBLIC ... libzip::zip)` (имя цели `libzip::zip` не проверено, A6). `target_include_directories` уже даёт `src/` всем потребителям через `PUBLIC`, отдельно добавлять не нужно.

**Цель-тест** (`CMakeLists.txt`, строки 82-84):
```cmake
qt_add_executable(resume_json_test tests/ResumeJsonTest.cpp)
target_link_libraries(resume_json_test PRIVATE resume_core Qt6::Test)
add_test(NAME resume_json COMMAND resume_json_test)
```
Новая цель по образцу: `qt_add_executable(zip_archive_test tests/ZipArchiveTest.cpp)`, `target_link_libraries(zip_archive_test PRIVATE resume_documents Qt6::Test)`, `add_test(NAME zip_archive COMMAND zip_archive_test)`. Имя теста `zip_archive` совпадает с командой из RESEARCH.md (`ctest --preset debug -R zip_archive`).

**Место для `find_package(libzip)`:** рядом с `find_package(Qt6 ...)` (`CMakeLists.txt`, строка 8). Проект ищет зависимости через `CMAKE_PREFIX_PATH: C:/msys64/mingw64` из `CMakePresets.json`, поэтому `CONFIG`-поиск работает без pkg-config.

---

### `.github/workflows/ci.yml` (config, изменить)

**Analog:** сам файл. Список пакетов MSYS2 повторяется в двух местах: джоб `build-test` (строки 39-44) и джоб `installer` (строки 70-75).

**Образец блока** (строки 39-44):
```yaml
          install: >-
            mingw-w64-x86_64-gcc
            mingw-w64-x86_64-cmake
            mingw-w64-x86_64-ninja
            mingw-w64-x86_64-qt6-base
            mingw-w64-x86_64-qt6-httpserver
```
Добавить строку `mingw-w64-x86_64-libzip` после `mingw-w64-x86_64-qt6-httpserver` в ОБА блока (строки 44 и 75). Одну правку в одном блоке недостаточно: это причина Pitfall 1 в RESEARCH.md. Существующий smoke-test установщика (строки 93-107) менять не нужно: он уже проверяет запуск без MSYS2 в PATH.

---

### `cmake/DeployMinGWRuntime.cmake` (config, проверить)

**Analog:** сам файл. Менять, скорее всего, не нужно.

**Механизм разрешения DLL** (строки 29-40):
```cmake
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES ${executables}
    LIBRARIES ${plugins}
    DIRECTORIES "${prefix}/bin" ${AIRA_RUNTIME_DIRS}
    ...
    RESOLVED_DEPENDENCIES_VAR resolved
```
`libzip.dll` попадёт в `resolved`, если `AIResumeAssistant.exe` линкуется с libzip через `resume_documents` (транзитивно). Маска по имени не нужна (Pitfall 9). Строки 47-52 копируют найденные DLL в `${prefix}/bin`. Поменять файл только если после пробной установки `libzip.dll` не появится в `bin/`.

### `installer/AIResumeAssistant.iss` (config, проверить)

**Analog:** сам файл. Менять, скорее всего, не нужно.

**Копирование всей папки развёртывания** (строка 44):
```ini
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
```
Любая DLL, которую положили в `bin/`, попадает в установщик автоматически. Список файлов в `.iss` не нужен. Вопрос о тексте лицензий gnutls/nettle (LGPL) остаётся открытым и решается отдельно (RESEARCH.md, Open Question 6).

---

### `src/documents/README.md` (docs, заменить заглушку)

**Analog:** текущая заглушка (строка 1): `Планируемый модуль импорта PDF/DOCX и анонимизации.`
Заменить её описанием слоя: назначение `ZipArchive` и `DocxError`, правило «только `QByteArray`, без путей», ссылка на `AGENTS.md` (слои). Одна строка заглушки недостаточна для будущего планировщика.

---

## Shared Patterns

### Проверка входа и сообщение об ошибке без приватных данных
**Source:** `src/application/ResumeJson.cpp`, строки 11-15 (`fail`) и 17-34 (`readText`)
**Apply to:** `DocumentImport`, `ZipArchive`
```cpp
bool fail(QString* error, const QString& message) {
    if (error)
        *error = message;
    return false;
}
```
Правило для фазы: сообщение содержит только имя файла (`QFileInfo::fileName()`), не путь и не фрагменты текста резюме (D-05, SAFE-05). Коды ошибок из libzip не пробрасывать в строку.

### RAII-владение и запрет копирования
**Source:** `src/storage/ResumeRepository.h`, строки 9-12
**Apply to:** `ZipArchive` (буфер и `zip_t*`)
```cpp
explicit ResumeRepository(QString databasePath);
~ResumeRepository();
ResumeRepository(const ResumeRepository&) = delete;
ResumeRepository& operator=(const ResumeRepository&) = delete;
```

### Тестовый шаблон Qt Test
**Source:** `tests/ResumeJsonTest.cpp`, строки 1-8 и 73-74
**Apply to:** `tests/ZipArchiveTest.cpp`
```cpp
class ResumeJsonTest : public QObject {
    Q_OBJECT
  private slots:
    ...
};
QTEST_GUILESS_MAIN(ResumeJsonTest)
#include "ResumeJsonTest.moc"
```

### Ограничения по размеру как константы в анонимном namespace
**Source:** `src/application/ResumeJson.cpp`, строки 7-9
**Apply to:** `DocumentImport.cpp` (D-01, D-03), `ZipArchive.cpp` (D-02)
```cpp
constexpr int kMaxShortText = 200;
constexpr int kMaxDescription = 10000;
constexpr int kMaxStages = 200;
```

### Форматирование
**Source:** `.clang-format` — `BasedOnStyle: LLVM`, `IndentWidth: 4`, `ColumnLimit: 110`, `SortIncludes: CaseSensitive`.
**Apply to:** все новые `.cpp` / `.h`
Заметка: в `docs/memory.md` записано `PointerAlignment: Left` (RESEARCH.md), но в `.clang-format` такой строки нет. Для новых файлов сверить по `clang-format --dry-run --Werror` и не полагаться на память. Выбрать один вариант и записать его в `docs/memory.md`.

---

## No Analog Found

Файлы без близкого аналога в кодебазе. Планировщик должен опираться на RESEARCH.md.

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| `src/documents/ZipArchive.cpp` (ядро чтения libzip) | service | file-I/O (из памяти) | В репозитории нет ни одного кода работы с ZIP или libzip |
| Распознавание CFB по байтам (`EncryptedPackage` в UTF-16LE) | utility | transform | Нет разбора бинарных форматов; только RESEARCH.md (Pitfall 4, A1) |
| Лимит распаковки (running budget поверх `zip_fread`) | utility | streaming | Нет аналога стримингового чтения с бюджетом |
| Чтение файла с кириллическим путём через `QFile` | service | file-I/O | В `src/` нет чтения файлов с диска (только SQLite по пути `QString` в `ResumeRepository`) |

---

## Metadata

**Analog search scope:** `src/`, `tests/`, `cmake/`, `installer/`, `.github/workflows/`, корень `CMakeLists.txt`, `CMakePresets.json`, `.clang-format`
**Files scanned:** 14 (отслеживаемые git-файлы, включая все аналоги выше)
**Tracked-source check:** все пути аналогов проверены через `git ls-files`: `src/application/ResumeJson.h`, `src/documents/README.md`, `tests/ResumeJsonTest.cpp` — tracked. Остальные аналоги (`src/storage/ResumeRepository.h`, `tests/ResumeE2ETest.cpp`, `CMakeLists.txt`, `ci.yml`, `cmake/DeployMinGWRuntime.cmake`, `installer/AIResumeAssistant.iss`) входят в список tracked-файлов `git ls-files` этой сессии. Gitignored-зеркал не использовано.
**Pattern extraction date:** 2026-10-24
