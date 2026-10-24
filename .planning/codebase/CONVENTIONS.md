---
last_mapped_commit: 21afce5aefddf6147b4486c8eb0a485474093e56
last_mapped_at: 2026-10-24
---
# Coding Conventions

**Analysis Date:** 2026-10-24

## Naming Patterns

**Files:**
- Source and header pairs share a base name in PascalCase: `src/storage/ResumeRepository.h` / `src/storage/ResumeRepository.cpp`, `src/application/ResumeJson.h` / `src/application/ResumeJson.cpp`, `src/web/WebServer.h` / `src/web/WebServer.cpp`.
- Test sources are `tests/<Name>Test.cpp` (`tests/ResumeJsonTest.cpp`, `tests/WebApiTest.cpp`) or `tests/ResumeE2ETest.cpp`. Test helpers used by Node live in `tests/e2e/` (`WebE2EServer.cpp`, `web_e2e.mjs`).
- Layer directories are lowercase single words: `src/domain`, `src/application`, `src/storage`, `src/ui`, `src/web`, `src/documents` and `src/llm` (the last two currently contain only `README.md`).
- Web assets are lowercase: `src/web/index.html`.

**Types:**
- Classes and structs are PascalCase: `ResumeRepository`, `WebServer`, `MainWindow`, `CareerStage`, `Resume`.
- Plain data aggregates are `struct` with public members and default member initializers, e.g. `src/domain/Resume.h`:
  ```cpp
  struct CareerStage {
      qint64 id = 0;
      QString position;
      ...
  };
  ```
- Stateful classes with invariants are `class` (`ResumeRepository`) or `final` (`WebServer`, `MainWindow`).
- Namespaces hold free-function groups: `namespace ResumeJson { QJsonObject toJson(...); }` in `src/application/ResumeJson.h`.

**Functions and methods:**
- camelCase for all methods and free functions: `toJson`, `fromJson`, `addStage`, `refreshList`, `selectStage`, `isTrustedRequest`, `handleSave`.
- Event handlers and UI actions use verb phrases: `openInBrowser`, `saveResume`, `updateSelectedStage`.
- Private HTTP handlers are prefixed `handle` (`handleIndex`, `handleLoad`) in `src/web/WebServer.h`.
- Helper functions that report failure use `fail(QString* error, const QString& message)` and return `false` (`src/storage/ResumeRepository.cpp`, `src/application/ResumeJson.cpp`). Keep this name for the same role.

**Variables and members:**
- Locals and parameters: camelCase (`databasePath`, `expectedPort`, `connectionName`).
- Data members: trailing underscore for members of classes: `databasePath_`, `connectionName_`, `repository_`, `webServer_`, `tcp_`, `http_`.
- Qt UI widget pointers are members initialised to `nullptr`: `QLineEdit* title_ = nullptr;` (`src/ui/MainWindow.h`).
- Constants: `k` prefix with camelCase: `kMaxShortText`, `kMaxDescription`, `kMaxStages`, `kMaxBodyBytes`, `kDefaultWebPort` (in anonymous namespaces or as local `constexpr`).
- Do not use the trailing underscore for struct members (`CareerStage::position`, `Resume::title`).

**Tests:**
- Test classes: `<Subject>Test` deriving from `QObject` with `Q_OBJECT` (`ResumeJsonTest`, `WebApiTest`, `ResumeE2ETest`).
- Test methods are `private slots:` with descriptive camelCase names that state behavior: `roundTrip`, `missingStagesMeansEmptyList`, `rejectsForeignHost`, `errorsDoNotLeakStorageDetails`.
- Data-driven tests pair a `<name>_data()` function with `<name>()` that uses `QFETCH` (`rejectsInvalidInput_data` / `rejectsInvalidInput`).

## Code Style

**Formatting:**
- Authoritative tool: `clang-format` with `/c/Users/Apostol/CLionProjects/AI-Resume-Assistant/.clang-format` (`BasedOnStyle: LLVM`, `IndentWidth: 4`, `ColumnLimit: 110`, `SortIncludes: CaseSensitive`).
- Indentation: 4 spaces. No tabs.
- Line limit: 110 columns.
- Continuation lines are aligned to the opening parenthesis or indented by 8 spaces, as in `src/web/WebServer.cpp`:
  ```cpp
  headers.append("Content-Security-Policy",
                 "default-src 'none'; script-src 'self' 'unsafe-inline'; "
  ```
- Braces: K&R style, opening brace on the same line. Single-statement bodies are written with or without braces; both occur (see Error Handling below). Prefer braces when the statement wraps.
- Pointer and reference alignment is inconsistent in the existing code. `src/ui/MainWindow.cpp` uses `QWidget *parent` and `const auto &stage`, while `src/storage/ResumeRepository.h` and `src/web/WebServer.h` use `Resume& resume` and `const QHttpServerRequest& request`. The `.clang-format` (LLVM base) places `*` and `&` on the right, so new code should follow `Type* name` only where it matches the surrounding file and otherwise `Type *name`. Run `clang-format` on touched files rather than hand-aligning.
- Trailing `// comment` comments explain intent, not mechanics. Example: `// the id is never taken from the body` in `tests/ResumeJsonTest.cpp`.
- Non-ASCII user-facing strings are Russian: UI labels (`"Название резюме"`, `"Добавить этап"`, `"Сохранено. ID: %1"`), and the test fixture data (`"Тестовое резюме"`). Keep new user-facing text in Russian to match the existing UI; keep identifiers, code comments and log strings in English.

**Linting:**
- `clang-tidy` configuration: `/c/Users/Apostol/CLionProjects/AI-Resume-Assistant/.clang-tidy`.
  - Enabled groups: `bugprone-*`, `performance-*`, `modernize-*`.
  - `HeaderFilterRegex: 'src/.*'` restricts header diagnostics to `src/`.
  - `WarningsAsErrors` is empty, so findings are advisory.
- Run per file with the compilation database: `clang-tidy <file> -p build/mingw-debug` (see `AGENTS.md` section 2). The compilation database is produced by `CMAKE_EXPORT_COMPILE_COMMANDS ON` in `/c/Users/Apostol/CLionProjects/AI-Resume-Assistant/CMakeLists.txt`.
- No clang-format or clang-tidy run result is recorded in this document; treat any formatting claim as unverified until the commands are run.
- Hook: `/c/Users/Apostol/CLionProjects/AI-Resume-Assistant/.claude/hooks/check_cpp_format.py` runs `clang-format --dry-run --Werror` after every edit of `.cpp`, `.h`, `.hpp`, `.cc`. It is non-destructive and blocks on failure.

## Language Standard

- C++23 is required. `CMakeLists.txt` sets `CMAKE_CXX_STANDARD 23`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`. Do not lower it.
- Use `std::optional` for "value or absent" returns (`ResumeRepository::load`, `ResumeJson::fromJson`).
- Use `std::unique_ptr` for owned children that are not `QObject`s (`MainWindow::repository_`, `MainWindow::webServer_`, test fixtures in `tests/WebApiTest.cpp`).
- Use `constexpr` for constants and `static` helpers inside an anonymous namespace (`src/web/WebServer.cpp`, `src/application/ResumeJson.cpp`).
- Use `auto` for iterator types and for values where the type is obvious from the initializer (`const auto resume = repository_.load(id, &error);`).
- Use `std::move` when transferring ownership into members (`databasePath_(std::move(databasePath))`).
- Use `qsizetype` for Qt sizes in new code (`kMaxBodyBytes` in `src/web/WebServer.cpp`).

## Qt and Object Ownership

- `QObject`-derived objects are owned through the parent tree. Child widgets are created with the parent as the argument: `new QWidget(this)`, `new QLineEdit(central)`, `new QPushButton("...", central)` (`src/ui/MainWindow.cpp`).
- Qt-owned objects that are not in a parent tree are owned by `std::unique_ptr` or stack objects. Example: `auto* tcp = new QTcpServer(this);` in `WebServer::listen`, with `delete tcp` on the failure path.
- Non-`QObject` C++ resources use RAII. `ResumeRepository` closes and removes its `QSqlDatabase` connection in its destructor (`src/storage/ResumeRepository.cpp`).
- Non-copyable classes delete copy operations explicitly: `ResumeRepository(const ResumeRepository&) = delete;`.
- `final` is applied to concrete `QObject` classes that are not meant to be subclassed: `WebServer`, `MainWindow`.
- Use the new-style `connect(sender, &Class::signal, this, &Class::slot)` syntax (`src/ui/MainWindow.cpp`). Do not introduce string-based `SIGNAL`/`SLOT` macros.
- Use `Q_OBJECT` on every class with signals or slots, including test classes.
- Modern Qt APIs in use: `QHttpServer` routing, `QHttpServerResponse`, `QHttpHeaders`, `QStandardPaths::writableLocation`, `QJsonObject` initializer lists.

## Error Handling

**Pattern A: out-parameter error string (the dominant pattern).** Functions that can fail take `QString* error = nullptr` as the last parameter, fill it on failure, and return `bool` or `std::optional`:

```cpp
// src/storage/ResumeRepository.h
bool open(QString* error = nullptr);
bool save(Resume& resume, QString* error = nullptr);
std::optional<Resume> load(qint64 id, QString* error = nullptr) const;
```

Callers pass `&error` and check the return value. Examples: `src/ui/MainWindow.cpp` (`repository_->open(&error)`), `src/web/WebServer.cpp` (`repository_.load(id, &error)`).

**Pattern B: `fail` helper.** Private static helper that writes the message if the pointer is non-null and returns `false`:

```cpp
bool ResumeRepository::fail(QString* error, const QString& message) {
    if (error) *error = message;
    return false;
}
```

Use `return fail(error, "...");` inside functions that return `bool`.

**Pattern C: validation of untrusted input.** Input from the HTTP body is validated field by field, and each failure produces a user-readable message without a stack or internal detail. See `readText` and `fromJson` in `src/application/ResumeJson.cpp`. Limits are named constants (`kMaxShortText = 200`, `kMaxDescription = 10000`, `kMaxStages = 200`).

**Pattern D: transactions with rollback.** Writes that touch multiple rows run in `db_.transaction()` with a local `rollback` lambda that rolls back and forwards the message through `fail` (`ResumeRepository::save`):

```cpp
auto rollback = [&](const QString& message) { db_.rollback(); return fail(error, message); };
```

**Error leakage rules (enforced by tests):**
- HTTP responses never expose storage internals. Storage errors are mapped to `"Storage error"` with status 500, and missing rows to `"Resume not found"` with status 404. See `WebServer::handleLoad` and `WebServer::handleSave`.
- `tests/WebApiTest.cpp::errorsDoNotLeakStorageDetails` asserts that the response body does not contain `sqlite`.
- Desktop error dialogs go through one helper, `MainWindow::reportError`, which shows a `QMessageBox::critical`.
- Some error strings compared by content (`error == "Resume not found"`) to choose the HTTP status. Keep such comparisons in sync with the repository messages.

**Error-handling inconsistency to be aware of:** `ResumeRepository::load` and `open` use `fail(...)` returns, while `handleSave` branches on message text. Prefer a typed error code over string comparison for new code paths.

## Control Flow Style

- Early returns are the default. Guard clauses come first (`if (!db_.isOpen()) return fail(...);` in `src/storage/ResumeRepository.cpp`).
- Single-line guards without braces are used in the storage and UI code: `if (error) *error = message;`, `if (!web_) { reportError(...); return; }`. Multi-statement blocks always use braces.
- Compound boolean conditions that span lines are broken before the operator with aligned continuation (`src/application/ResumeJson.cpp`).

## Logging

- No logging framework is in use. There is no `qDebug`, `qWarning` or `std::cerr` in production code.
- User-visible status is shown in the status bar: `statusBar()->showMessage("Сохранено. ID: %1", 8000)` in `src/ui/MainWindow.cpp`.
- The test helper `tests/e2e/WebE2EServer.cpp` writes a single machine-readable line `PORT=<n>` to stdout for the Node e2e runner to parse. Do not add other stdout output to it.
- Never log resume content, titles, names, or any user data (see `.claude/rules/llm-security.md` for the stricter rule that applies under `src/llm/**`).

## Comments

**When to comment:**
- Explain security decisions and non-obvious constraints, not what the code does. Existing examples:
  - `// Defends the loopback API against DNS rebinding (Host check) and cross-site` in `src/web/WebServer.cpp`.
  - `// Validates untrusted input; the resume id is never read from the body.` in `src/application/ResumeJson.h`.
  - `// Locator wait, not waitForFunction: the page CSP forbids eval.` in `tests/e2e/web_e2e.mjs`.
- Header files use `//` comments above the declaration to describe the contract.
- Section comments are used only for files that have several parts. Example: `// Deployment: ...` in `CMakeLists.txt`.

**JSDoc/TSDoc:** Not applicable to C++. Doxygen-style comments are not used.

## Function Design

**Size:** Most functions are short (under 40 lines). Larger UI setup in `MainWindow::MainWindow` is a single constructor. New logic should be split into named helpers in an anonymous namespace in the `.cpp` file (see `readText` in `src/application/ResumeJson.cpp`).

**Parameters:**
- Pass `const T&` for read-only Qt and domain types (`const QJsonObject& object`, `const QHttpServerRequest& request`).
- Pass by value and `std::move` into members for owned strings (`QString databasePath`).
- Output parameters are either a non-const reference (`Resume& resume` in `save`, which writes the new `id` back) or a trailing `QString* error`.

**Return values:**
- `bool` for commands with a single error message through an out-parameter.
- `std::optional<T>` for queries that may be empty; `std::nullopt` plus error text on failure.
- HTTP handlers return `QHttpServerResponse` from a `jsonReply` or `errorReply` factory (`src/web/WebServer.cpp`). Always use these factories so the security headers are added.

## Module Design

**Layering (from `AGENTS.md`):** UI (Widgets and web) -> domain/application -> storage/LLM/documents. Business rules are not placed in `QWidget` subclasses or in JavaScript.
- `src/domain/Resume.h`: plain data types only, no Qt widgets. Includes `QString` and `QVector` only.
- `src/application/ResumeJson.*`: conversion and validation between domain types and `QJsonObject`.
- `src/storage/ResumeRepository.*`: all SQL. Only this class issues queries.
- `src/web/WebServer.*` and `src/ui/MainWindow.*`: adapters. They call `ResumeJson` and `ResumeRepository` and must not contain SQL or validation rules.

**Exports and linkage:**
- Each library is a static target with public include dir `src/`, so includes use the path from `src/`: `#include "storage/ResumeRepository.h"`, `#include "application/ResumeJson.h"`.
- Headers start with `#pragma once`. Do not use include guards.

**Include order (as used in the code):**
1. The file's own header first in `.cpp` files (`#include "ui/MainWindow.h"`, `#include "web/WebServer.h"`).
2. Project headers with quotes: `"domain/Resume.h"`, `"storage/ResumeRepository.h"`.
3. Qt headers with angle brackets, one per line, alphabetical (`<QAction>`, `<QDesktopServices>`, `<QDir>`...).
4. Standard library headers last (`<optional>`, `<utility>`).

**Barrel files:** Not used. Include the specific header.

**SQL and security conventions:**
- All SQL with user data uses prepared statements and `addBindValue`. The only non-prepared statements are fixed DDL and `PRAGMA` strings in `ResumeRepository::open` (`src/storage/ResumeRepository.cpp`).
- Identifiers and table names are literals, never built from input.
- Foreign keys are enabled with `PRAGMA foreign_keys = ON` in `open`.

## Example: Writing a New Method in the Existing Style

Based on `ResumeRepository::load`, a new read method should:

```cpp
// header
std::optional<Resume> listResumes(QString* error = nullptr) const;

// source
std::optional<Resume> ResumeRepository::listResumes(QString* error) const {
    if (!db_.isOpen()) {
        fail(error, "Database is not open");
        return std::nullopt;
    }
    QSqlQuery q(db_);
    q.prepare("SELECT id,title FROM resumes ORDER BY id");
    if (!q.exec()) {
        fail(error, q.lastError().text());
        return std::nullopt;
    }
    // ...
}
```

Add a matching `QTest` case in `tests/` and register nothing new in CMake if it fits the existing test executable.

---

*Convention analysis: 2026-10-24*
