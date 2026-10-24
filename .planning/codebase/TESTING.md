---
last_mapped_commit: 21afce5aefddf6147b4486c8eb0a485474093e56
last_mapped_at: 2026-10-24
---
# Testing Patterns

**Analysis Date:** 2026-10-24

## Test Framework

**Runner:**
- Qt Test (`Qt6::Test`), the `QTEST_GUILESS_MAIN` entry point, Qt 6.7 or newer (`find_package(Qt6 6.7 ...)` in `/c/Users/Apostol/CLionProjects/AI-Resume-Assistant/CMakeLists.txt`).
- Orchestration: CTest via `enable_testing()` and `add_test` in `CMakeLists.txt`.
- Configuration: `/c/Users/Apostol/CLionProjects/AI-Resume-Assistant/CMakePresets.json` defines the `debug` and `release` test presets with `"outputOnFailure": true`.

**Assertion Library:**
- Qt Test macros only: `QCOMPARE`, `QVERIFY`, `QVERIFY2`, `QFETCH`, `QTest::addColumn`, `QTest::newRow`, `QSignalSpy`.
- No GoogleTest, Catch2 or Boost.Test.

**Browser end-to-end:**
- Node.js script `/c/Users/Apostol/CLionProjects/AI-Resume-Assistant/tests/e2e/web_e2e.mjs` using `playwright-core`, Node's `node:assert/strict`, and a locally installed Chrome (`channel: "chrome"`, overridable with `E2E_BROWSER_CHANNEL`).
- Dependencies are declared in `/c/Users/Apostol/CLionProjects/AI-Resume-Assistant/tests/e2e/package.json` and locked in `tests/e2e/package-lock.json`. Install with `npm install` inside `tests/e2e`.

**Run Commands:**

```bash
cmake --preset mingw-debug                     # Configure (required before ctest in a fresh tree)
cmake --build --preset debug                   # Build all test executables
ctest --preset debug                           # Run all registered tests
ctest --preset debug -R resume_e2e             # Run one test by name (from AGENTS.md)
ctest --preset debug -R web_api                # Run the web API C++ tests
ctest --preset debug -R web_e2e                # Run the browser e2e (only if Node and node_modules exist)
ctest --preset release                         # Run the Release preset
```

Direct execution of a single test executable with a single case, useful for debugging:

```bash
build/mingw-debug/tests/web_api_test rejectsForeignHost
```

(Qt Test accepts a test method name as the argument. The executable path is the target output path; verify it after configure.)

Coverage: no coverage tool or command is configured in the repository.

## Registered Tests

Declared in `CMakeLists.txt`:

| CTest name | Executable target | Source | Scope |
|------------|-------------------|--------|-------|
| `resume_e2e` | `resume_e2e_test` | `tests/ResumeE2ETest.cpp` | SQLite persistence across two repository instances |
| `resume_json` | `resume_json_test` | `tests/ResumeJsonTest.cpp` | JSON validation and round trip |
| `web_api` | `web_api_test` | `tests/WebApiTest.cpp` | Loopback HTTP API, security headers, Host/Origin/Content-Type checks |
| `web_e2e` | `web_e2e_server` (+ Node) | `tests/e2e/WebE2EServer.cpp`, `tests/e2e/web_e2e.mjs` | Browser flow: create, save, reload, reopen by ID |

`web_e2e` is registered only when `node` is on PATH and `tests/e2e/node_modules/playwright-core` exists. Otherwise CMake prints `web_e2e skipped` and the test is absent from `ctest` output. Do not report it as passing when it was skipped.

`resume_e2e` runs with `ENVIRONMENT "QT_QPA_PLATFORM=offscreen"`. Add the same property to any new test that might create a widget.

## Test File Organization

**Location:**
- All C++ tests live in the top-level `tests/` directory, separate from `src/`. There are no co-located test files.
- The browser test support code lives in `tests/e2e/`.

**Naming:**
- `tests/<Subject>Test.cpp` for unit and component tests.
- `tests/<Subject>E2ETest.cpp` for multi-step tests that cross storage boundaries (`ResumeE2ETest.cpp`). Note the naming is asymmetric with `WebApiTest.cpp`; new tests follow the `*Test.cpp` form.
- Test executable target name is the snake_case form of the CTest name, suffixed `_test` (`resume_json_test`). Browser helper targets use `_server`.

**Structure:**

```
tests/
├── ResumeE2ETest.cpp       # repository create/save/reopen/update
├── ResumeJsonTest.cpp      # ResumeJson validation rules
├── WebApiTest.cpp          # raw HTTP client + WebServer
└── e2e/
    ├── WebE2EServer.cpp    # prints PORT=<n> for the Node runner
    ├── web_e2e.mjs         # Playwright-core browser scenario
    ├── package.json
    └── package-lock.json
```

Each new test file must be added to `CMakeLists.txt` with `qt_add_executable`, `target_link_libraries(... Qt6::Test)`, `add_test`, and, if it is a GUI test, the `QT_QPA_PLATFORM=offscreen` environment property. No test discovery is used.

## Test Structure

**Class skeleton (used in every C++ test):**

```cpp
#include "application/ResumeJson.h"
#include <QJsonArray>
#include <QtTest>

class ResumeJsonTest : public QObject {
    Q_OBJECT
  private slots:
    void roundTrip() {
        // arrange
        Resume resume;
        resume.title = "Тестовое резюме";
        // act
        const QJsonObject json = ResumeJson::toJson(resume);
        // assert
        QString error;
        const auto parsed = ResumeJson::fromJson(json, &error);
        QVERIFY2(parsed.has_value(), qPrintable(error));
        QCOMPARE(parsed->title, resume.title);
    }
};
QTEST_GUILESS_MAIN(ResumeJsonTest)
#include "ResumeJsonTest.moc"
```

Notes on this skeleton, all taken from the existing files:
- `#include "X.moc"` is the last line of the file, after `QTEST_GUILESS_MAIN`. CMake's `CMAKE_AUTOMOC ON` generates the `.moc`.
- Test methods are `private slots:` so Qt Test discovers them.
- Use `QTEST_GUILESS_MAIN` for tests that do not create widgets (all current C++ tests). Use `QTEST_MAIN` only for widget tests; none exist yet.

**Setup and teardown:**
- `init()` and `cleanup()` run before and after each test method. Used in `tests/WebApiTest.cpp`:
  ```cpp
  void init() {
      dir_ = std::make_unique<QTemporaryDir>();
      QVERIFY(dir_->isValid());
      repo_ = std::make_unique<ResumeRepository>(dir_->filePath("web.sqlite"));
      QString error;
      QVERIFY2(repo_->open(&error), qPrintable(error));
      server_ = std::make_unique<WebServer>(*repo_);
      QVERIFY2(server_->listen(0, &error), qPrintable(error));
  }

  void cleanup() {
      server_.reset();
      repo_.reset();
      dir_.reset();
  }
  ```
  Order matters: the server is released before the repository it references, and the repository before the temporary directory that holds its file.
- Per-test temporary state uses `QTemporaryDir` (`tests/ResumeE2ETest.cpp`, `tests/e2e/WebE2EServer.cpp`). Do not write to a fixed path.
- Scoped blocks `{ ... }` are used to force the destructor of a repository to run before the test reopens the same file (`ResumeE2ETest::createSaveReopenAndUpdate`).

**Patterns:**
- Use `QVERIFY2(cond, qPrintable(error))` whenever a function returns an error string, so the failure message shows the reason.
- Use `QCOMPARE` for value comparisons. Compare `QString` with a `QString(...)` or a string literal that Qt can convert. Compare integers with integer literals; `qint64(0)` is used when the type width matters (`QCOMPARE(parsed->id, qint64(0));`).
- Data-driven tests: a `<name>_data()` function registers rows with `QTest::addColumn<T>("name")` and `QTest::newRow("label") << value;`. The test body uses `QFETCH(T, name);`. See `rejectsInvalidInput_data` in `tests/ResumeJsonTest.cpp`. Row labels are human-readable phrases ("no title", "stage without position").
- Helper lambdas inside `_data()` build repeated fixtures: `const auto stage = [](const QJsonObject& o) { ... };`.

## Mocking

**Framework:** None. No mocking library is used.

**Approach:** Use real collaborators with temporary storage.
- `ResumeRepository` is always backed by a real SQLite file inside `QTemporaryDir`.
- `WebServer` is run for real on `127.0.0.1` with port `0` (any free port). `server_->port()` gives the chosen port.
- HTTP is exercised through a raw `QTcpSocket` client in `tests/WebApiTest.cpp::send(...)`, so the test controls the `Host`, `Origin` and `Content-Type` headers exactly. This is the only way to test the DNS rebinding and CSRF defences.

**What to Mock:**
- Nothing in the current suite. If a future LLM client must be tested, put a fake HTTP responder behind an interface in `src/llm/`, not inside the production classes.

**What NOT to Mock:**
- SQLite. Tests open a real database file in a temp directory.
- The HTTP stack. `WebApiTest` drives the real `QHttpServer` over loopback.

**Test payload rules (from `.claude/rules/llm-security.md` and `AGENTS.md`):**
- Use synthetic, deterministic data only. Fixture names and text in the existing tests are generic ("Тестовое резюме", "Тестовая компания", "Инженер", "Acme" in the browser test). Do not add real names, e-mails, phone numbers, real résumés, or API keys.

## Fixtures and Factories

**Test Data:**
Inline literals are the norm. Shared request bodies are built with a helper function in an anonymous namespace (`resumeBody` in `tests/WebApiTest.cpp`):

```cpp
QByteArray resumeBody(const QString& title, const QString& position) {
    return QJsonDocument(QJsonObject{{"title", title},
                                     {"stages", QJsonArray{QJsonObject{{"position", position}, ...}}}})
        .toJson(QJsonDocument::Compact);
}
```

Helpers live in the same test file. There is no shared fixture header yet. If two test files need the same helper, move it to `tests/support/` and add it to every test target that uses it.

**Location:**
- Fixtures are defined inside the test `.cpp` file.
- Browser fixtures are inline in `tests/e2e/web_e2e.mjs`.

## Coverage

**Requirements:** None enforced. No coverage threshold is set in `CMakeLists.txt`, `CMakePresets.json` or CI.

**View Coverage:** Not configured. There is no `gcov`, `lcov` or `gcovr` target.

**Gaps (current):**
- `src/ui/MainWindow.cpp` has no tests. The Widgets UI is verified only by manual run.
- `src/storage/ResumeRepository.cpp::save` error paths (rollback on failed insert, `numRowsAffected != 1`) are not exercised directly.
- `ResumeRepository::open` failure (unwritable path) is not tested.
- `src/documents/`, `src/llm/` contain no code yet. Their planned tests are not present.

## Test Types

**Unit Tests:**
- `tests/ResumeJsonTest.cpp`: pure functions, no I/O. Covers round trip, defaults for missing optional fields, rejection of each invalid case, the stage count limit.
- Scope: validation rules in `src/application/ResumeJson.cpp`.

**Integration Tests:**
- `tests/ResumeE2ETest.cpp`: real SQLite file, two separate repository instances to prove persistence.
- `tests/WebApiTest.cpp`: real `WebServer` with real `ResumeRepository`, over TCP loopback. Covers routing, status codes, security headers, Host and Origin checks, Content-Type enforcement, and the rule that storage details are not leaked.

**E2E Tests:**
- `tests/e2e/web_e2e.mjs`: runs `web_e2e_server` as a child process, reads `PORT=<n>` from its stdout, drives Chrome via `playwright-core` headless, and asserts on the DOM and the API. Also writes a screenshot to `screenshots/web-e2e-sprint1.png`.
- Constraints: the page's Content Security Policy forbids `eval`, so Playwright `waitForFunction` cannot be used. Use `locator(...).waitFor()` instead.
- Browser console errors and page errors fail the test (`pageErrors` must be empty).
- The script kills the server in a `finally` block; keep that pattern for any new process-based test.

## Common Patterns

**Asynchronous (socket) testing:**
The raw HTTP client in `tests/WebApiTest.cpp` uses `waitForConnected`, `QSignalSpy` on `disconnected`, and a bounded polling loop instead of an unbounded wait:

```cpp
QSignalSpy closed(&socket, &QTcpSocket::disconnected);
QObject::connect(&socket, &QTcpSocket::readyRead, [&] { raw += socket.readAll(); });
for (int i = 0; i < 50 && socket.state() != QAbstractSocket::UnconnectedState; ++i)
    closed.wait(100);
```

Rules for new async tests:
- Always bound the wait with a timeout (`waitForConnected(3000)`, loop limit of 50 x 100 ms).
- Prefer `QSignalSpy::wait(ms)` over `QTest::qWait` and busy loops.
- Do not use `sleep` or fixed `QThread::msleep`.

**Error testing (negative cases):**
- Data-driven rejection tests: `rejectsInvalidInput` iterates a table of bad JSON objects and asserts `!has_value()` and a non-empty `error` (`tests/ResumeJsonTest.cpp`).
- HTTP status tests: each rejected request is checked with `QCOMPARE(send(...).status, <code>)`. Mapping in use: 400 invalid body, 403 forbidden Host or Origin, 404 unknown resume, 415 wrong Content-Type.
- Information-leak test: `errorsDoNotLeakStorageDetails` asserts the response body does not contain `sqlite`.
- Boundary tests: exact limits are tested with one value past the limit (`QString(201, 'a')` for the 200-character title, `QString(10001, 'a')` for the 10 000-character description, 201 stages for the 200-stage limit).

**Persistence and restart testing:**
Close the first repository object in an inner scope, open a new one on the same file, and verify the data (`tests/ResumeE2ETest.cpp`). Follow the same pattern for any new storage feature.

## Writing a New Test

Follow the project skill `.claude/skills/write-tests/SKILL.md`:
1. Read the target component and existing tests; identify behavior and edge cases.
2. Add focused Qt Test cases with deterministic, synthetic data; no external APIs or secrets.
3. Cover happy path, invalid input, and persistence/restart where applicable.
4. Run `cmake --build --preset debug` and `ctest --preset debug`.
5. Report exact commands and results; if unavailable, mark tests as NOT RUN.
6. Do not silently change production behavior to make tests pass.

Checklist for a new test case:
- New method in the existing `private slots:` section of the right `*Test.cpp`, or a new `tests/<Subject>Test.cpp` with the skeleton above.
- Data-driven when there are more than two similar inputs.
- Temporary files through `QTemporaryDir`; no writes to the repository or the user's AppData.
- Failure messages through `qPrintable(error)`.
- If a new executable is added, register it in `CMakeLists.txt` with `add_test`, link `Qt6::Test`, and set `QT_QPA_PLATFORM=offscreen` if it touches widgets.
- Run the commands above. Never state a test passed unless it was executed in this session.

## Test Status Notes

- The test commands above are documented, not verified in this analysis. This mapping was produced by reading the sources; no build or `ctest` run was performed as part of writing this document.

---

*Testing analysis: 2026-10-24*
