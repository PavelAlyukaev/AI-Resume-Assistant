---
last_mapped_commit: 21afce5aefddf6147b4486c8eb0a485474093e56
last_mapped_at: 2026-10-24
---
<!-- refreshed: 2026-10-24 -->

# Architecture

**Analysis Date:** 2026-10-24

## System Overview

```text
┌──────────────────────────────────────────────────────────────────────┐
│                     Single desktop process (Qt 6)                     │
├───────────────────────────────┬──────────────────────────────────────┤
│  Qt Widgets UI                │  Local web UI (browser)               │
│  `src/ui/MainWindow.*`        │  `src/web/index.html` (embedded)      │
│  `src/main.cpp` entry         │  HTTP on 127.0.0.1 via Qt HttpServer  │
│                               │  `src/web/WebServer.*`                │
└──────────────┬────────────────┴──────────────────┬───────────────────┘
               │                                   │
               ▼                                   ▼
┌──────────────────────────────────────────────────────────────────────┐
│  Application layer: JSON validation / mapping                          │
│  `src/application/ResumeJson.*`                                       │
└──────────────────────────────────┬───────────────────────────────────┘
                                   │
                                   ▼
┌──────────────────────────────────────────────────────────────────────┐
│  Domain model (plain structs, no I/O)                                 │
│  `src/domain/Resume.h`                                                │
└──────────────────────────────────┬───────────────────────────────────┘
                                   │
                                   ▼
┌──────────────────────────────────────────────────────────────────────┐
│  Storage: `ResumeRepository` (Qt SQL, QSQLITE)                        │
│  `src/storage/ResumeRepository.*`  → `resumes.sqlite` in app data dir │
└──────────────────────────────────────────────────────────────────────┘

Planned, not implemented: `src/documents/` (PDF/DOCX import, anonymization),
`src/llm/` (cloud LLM adapters). See `src/*/README.md`.
```

## Pattern Overview

**Overall:** Modular monolith, single desktop process. Two front ends (Qt Widgets and a loopback-only web UI) share one repository object and one domain model. Layering is by directory and CMake static library, not by framework.

**Key Characteristics:**
- One executable `AIResumeAssistant` linking two static libraries: `resume_core` (domain, application, storage) and `resume_web` (HTTP front end). Defined in `CMakeLists.txt`.
- No network exposure beyond `127.0.0.1`: `WebServer::listen` binds loopback only and the `Host` header is checked (`WebServer::isTrustedRequest` in `src/web/WebServer.cpp`).
- Persistence is synchronous, inside the GUI thread. Qt HttpServer dispatches requests on the main event loop, so both UIs touch the repository from one thread.
- The domain model is plain data (`CareerStage`, `Resume` in `src/domain/Resume.h`); there is no ORM and no business-rule class layer yet.
- Sprint 1 scope only: career stage editing and SQLite save/load. The layering rule in `AGENTS.md` (no business logic in QWidget or JS) is the target; currently `MainWindow` and `index.html` hold some editing logic directly.

## Layers

**Domain:**
- Purpose: Data shape of a resume and its career stages.
- Location: `src/domain/`
- Contains: `Resume.h` (`struct Resume`, `struct CareerStage`), no I/O.
- Depends on: Qt value types (`QString`, `QVector`) only.
- Used by: `storage`, `application`, `ui`, `web`, tests.

**Application:**
- Purpose: Convert between JSON and domain objects and validate untrusted input.
- Location: `src/application/`
- Contains: `ResumeJson.h` / `ResumeJson.cpp` (namespace `ResumeJson`: `toJson`, `fromJson`). `fromJson` never reads the resume id from the body.
- Depends on: `domain`.
- Used by: `src/web/WebServer.cpp`, `tests/ResumeJsonTest.cpp`.

**Storage:**
- Purpose: Persist resumes in SQLite through Qt SQL.
- Location: `src/storage/`
- Contains: `ResumeRepository.h` / `ResumeRepository.cpp`. Owns a named `QSqlDatabase` connection (`connectionName_`), creates the schema in `open()`, and saves with transactions in `save()`.
- Depends on: `domain`, Qt SQL (`QSQLITE`).
- Used by: `MainWindow` (owned via `std::unique_ptr`), `WebServer` (held by reference), `tests/ResumeE2ETest.cpp`, `tests/WebApiTest.cpp`.

**Presentation (Widgets):**
- Purpose: Desktop editing UI.
- Location: `src/ui/MainWindow.h` / `src/ui/MainWindow.cpp`
- Contains: `MainWindow` (`QMainWindow`), which holds the working `Resume resume_`, the `ResumeRepository` and the `WebServer`. Creates the repository at `<AppData>/resumes.sqlite` (`MainWindow.cpp` line ~64).
- Depends on: `resume_core`, `resume_web`, Qt Widgets.
- Used by: `src/main.cpp` only.

**Presentation (Web):**
- Purpose: Browser UI and JSON API for the same data.
- Location: `src/web/WebServer.h` / `src/web/WebServer.cpp`; front end `src/web/index.html`, embedded as a Qt resource with prefix `/web` (`qt_add_resources(resume_web web_assets ...)` in `CMakeLists.txt`).
- Contains: `WebServer` (`QObject`, `Q_OBJECT`) with an internal `QHttpServer http_` and optional `QTcpServer* tcp_`.
- Depends on: `storage`, `application`, Qt HttpServer, Qt Network.
- Used by: `MainWindow` (menu "Открыть веб-версию" opens `WebServer::url()`), `tests/WebApiTest.cpp`, `tests/e2e/WebE2EServer.cpp`.

**Planned layers (empty placeholders):**
- `src/documents/` (`README.md` only): planned PDF/DOCX import and anonymization.
- `src/llm/` (`README.md` only): planned cloud LLM adapters. Security rules apply: `.claude/rules/llm-security.md`.

## Data Flow

### Save from Widgets UI

1. User edits fields in `MainWindow` and clicks save; `MainWindow::saveResume` (`src/ui/MainWindow.cpp:126`) runs.
2. `MainWindow` calls `ResumeRepository::save(resume_, &error)` (`src/storage/ResumeRepository.cpp:41`).
3. Repository opens a transaction, inserts or updates `resumes`, deletes and re-inserts `career_stages`, commits, and writes the new id back into `Resume::id`.
4. Errors go to `MainWindow::reportError` (`src/ui/MainWindow.cpp:163`).

### Save from web UI (PUT or POST)

1. Browser calls `POST /api/resumes` or `PUT /api/resumes/<id>`; routes are registered in `WebServer::WebServer` (`src/web/WebServer.cpp:28`).
2. `WebServer::handleSave` checks trust (`isTrustedRequest`, requires JSON) and calls `ResumeJson::fromJson`.
3. Validated `Resume` is passed to `ResumeRepository::save`; the id from the URL is applied server-side.
4. Response is JSON via `WebServer::jsonReply`; errors via `WebServer::errorReply` (no stack traces or internal paths).

### Load

1. Widgets: `MainWindow::openResume` (`src/ui/MainWindow.cpp:137`) calls `ResumeRepository::load`.
2. Web: `GET /api/resumes/<id>` → `WebServer::handleLoad` (`src/web/WebServer.cpp:106`) → `ResumeRepository::load` → `ResumeJson::toJson`.
3. Index page: `GET /` → `WebServer::handleIndex` (`src/web/WebServer.cpp:94`) serves the embedded `index.html`.

**State Management:**
- Widgets hold the editing state in `MainWindow::resume_`. Stage edits are staged in widgets and written to `resume_` in `updateSelectedStage`.
- The web UI holds state in the browser page; the server is stateless per request apart from the shared database.
- No cross-UI cache: both front ends read the database on demand, so a save from one is visible to the other on next load.

## Key Abstractions

**`Resume` / `CareerStage`:**
- Purpose: Persistent resume aggregate (title plus ordered career stages).
- Examples: `src/domain/Resume.h`
- Pattern: Plain value structs with default-initialized `id = 0` meaning "not yet saved".

**`ResumeRepository`:**
- Purpose: Single persistence gateway for resumes.
- Examples: `src/storage/ResumeRepository.h`, `src/storage/ResumeRepository.cpp`
- Pattern: Non-copyable RAII owner of a named Qt SQL connection. Errors are returned through an optional `QString* error` out-parameter and a `bool` / `std::optional` result, not exceptions. `fail()` is the single error-reporting helper.

**`ResumeJson`:**
- Purpose: JSON boundary for untrusted input and output.
- Examples: `src/application/ResumeJson.h`, `src/application/ResumeJson.cpp`
- Pattern: Free functions in a namespace; `fromJson` returns `std::optional<Resume>`.

**`WebServer`:**
- Purpose: Loopback HTTP adapter over the repository.
- Examples: `src/web/WebServer.h`, `src/web/WebServer.cpp`
- Pattern: Qt `QObject` with routes registered in the constructor; lambdas forward to private `handle*` methods. Port 0 selects a free port (used by tests).

## Entry Points

**Desktop application:**
- Location: `src/main.cpp`
- Triggers: User launches `build/mingw-debug/AIResumeAssistant.exe`.
- Responsibilities: Create `QApplication`, construct `MainWindow`, show it, run the event loop.

**Web HTTP routes:**
- Location: `src/web/WebServer.cpp` (constructor, lines 28-38)
- Triggers: Browser requests to `http://127.0.0.1:<port>/`.
- Responsibilities: `GET /`, `GET /api/resumes/<id>`, `POST /api/resumes`, `PUT /api/resumes/<id>`.

**Test entry points:**
- `tests/ResumeE2ETest.cpp` (ctest `resume_e2e`), `tests/ResumeJsonTest.cpp`, `tests/WebApiTest.cpp`, `tests/e2e/WebE2EServer.cpp` (helper process printing `PORT=` for the browser test in `tests/e2e/web_e2e.mjs`). Targets are defined in `CMakeLists.txt`.

## Architectural Constraints

- **Threading:** Single GUI thread. `QHttpServer` and `QSqlDatabase` are used from the main event loop; no worker threads exist. Any future background work (LLM calls, document parsing) must not block the UI thread (per `AGENTS.md`).
- **Global state:** None at namespace scope in `src/`. The `QSqlDatabase` connection is a process-wide named connection registered by `QSqlDatabase::addDatabase` in `ResumeRepository::open`.
- **Circular imports:** None observed. Dependency direction is `ui`/`web` → `storage`/`application` → `domain`.
- **Library boundary:** `resume_web` depends on `resume_core`, not the reverse. Qt Widgets is linked only into the executable.
- **Standard:** C++23 is required (`CMAKE_CXX_STANDARD 23` in `CMakeLists.txt`); do not lower it.
- **Qt version:** Qt 6.7 minimum (`find_package(Qt6 6.7 ...)`) because `QHttpHeaders` is used in `WebServer.cpp`.

## Anti-Patterns

### Widget-level data ownership

**What happens:** `MainWindow` owns the working `Resume resume_` and performs field-to-struct mapping in `updateSelectedStage` (`src/ui/MainWindow.cpp:113`).
**Why it's wrong:** Editing rules (validation, stage ordering) live in the UI class, so the web front end cannot reuse them and must duplicate logic in `index.html`.
**Do this instead:** Put validation and mutation of `Resume` in `src/application/` as free functions or a service class, and call it from both `MainWindow` and `WebServer`. Keep `ResumeJson` as the JSON adapter only.

### Repository reuse without a lock across front ends

**What happens:** `MainWindow` and `WebServer` share one `ResumeRepository` instance by reference, and each keeps its own copy of the resume being edited.
**Why it's wrong:** A web save does not refresh the widget's `resume_`, and a later widget save can overwrite web changes (last write wins, no version check).
**Do this instead:** Re-load before editing, or add an optimistic version column in `resumes` and reject stale saves in `ResumeRepository::save`.

### Schema creation inside `open()`

**What happens:** `ResumeRepository::open` runs `CREATE TABLE IF NOT EXISTS` statements directly (`src/storage/ResumeRepository.cpp:24-39`).
**Why it's wrong:** There is no migration mechanism, so future schema changes (new columns, indexes) have no versioned path for existing user databases.
**Do this instead:** Add a `PRAGMA user_version` check and ordered migration steps inside `open()` before adding columns.

## Error Handling

**Strategy:** Return-value and out-parameter errors. No exceptions are thrown across module boundaries.

**Patterns:**
- Repository methods return `bool` or `std::optional` and fill `QString* error` (nullable). Messages come from `QSqlQuery::lastError()`.
- Save uses a `rollback` lambda that calls `db_.rollback()` and then `fail()` (`ResumeRepository.cpp`).
- Web responses map failures to HTTP status codes through `jsonReply` / `errorReply`.
- UI errors are shown via `MainWindow::reportError` as message boxes or status text.

## Cross-Cutting Concerns

**Logging:** No logging framework. Errors are returned to callers; no file logging is present.
**Validation:** Input validation for JSON happens in `ResumeJson::fromJson`. Widgets rely on field edits and `QLineEdit` contents.
**Authentication:** None. Access control is the loopback bind plus `Host`-header checking in `WebServer::isTrustedRequest`. Any future remote access requires a design change.
**Security headers:** `addSecurityHeaders` in `src/web/WebServer.cpp` (anonymous namespace, line 14) sets response headers for the HTML page, including a CSP that forbids `eval`.

---

*Architecture analysis: 2026-10-24*
