---
last_mapped_commit: 21afce5aefddf6147b4486c8eb0a485474093e56
last_mapped_at: 2026-10-24
---
# Codebase Concerns

**Analysis Date:** 2026-10-24

## Tech Debt

**Stage IDs are not stable across saves:**
- Issue: `ResumeRepository::save` deletes all `career_stages` rows for a resume and re-inserts them on every update, so every stage gets a new `id`. Clients that cache stage IDs (Qt list selection, web editor state) hold stale values after a save.
- Files: `src/storage/ResumeRepository.cpp` (save, UPDATE branch), `src/application/ResumeJson.cpp` (fromJson ignores incoming stage `id`), `src/web/index.html`, `src/ui/MainWindow.cpp`
- Impact: Any future feature that references a stage by ID (interview answers, fact confirmation, LLM suggestions tied to a stage) will break silently after the first save. This conflicts with the requirement to keep candidate facts separate from AI suggestions.
- Fix approach: Upsert stages by ID (`UPDATE ... WHERE id = ? AND resume_id = ?`), delete only rows missing from the payload, and keep the incoming `id` in `ResumeJson::fromJson`.

**Schema management is ad hoc:**
- Issue: `ResumeRepository::open` runs `CREATE TABLE IF NOT EXISTS` statements inline with no schema version (`PRAGMA user_version`) and no migration mechanism.
- Files: `src/storage/ResumeRepository.cpp`
- Impact: The planned Sprint 2+ features (documents, anonimized payloads, interview records, LLM provider metadata) will need schema changes; without versioning, upgrades of existing user databases are unsafe.
- Fix approach: Introduce `PRAGMA user_version` with ordered migration steps before adding new tables.

**Repository connection lifecycle is fragile:**
- Issue: `ResumeRepository::open` assigns `db_` before `db_.open()`. On failure the invalid/closed connection stays registered under `connectionName_`; a later call to `open` retries with `addDatabase` under the same name only if `db_` is not valid. The destructor only removes the connection when `db_.isValid()`.
- Files: `src/storage/ResumeRepository.cpp` (open, destructor)
- Impact: Repeated open attempts can leak named connections; the caller in `MainWindow` continues after a failed open (`reportError` only), leaving `webServer_` serving requests against an unopened repository, which returns "Storage error" for every call.
- Fix approach: Only register the connection after a successful open, or remove it on failure; make `MainWindow` disable the editor and web server when `open` fails.

**Hard-coded resume ID range in the Qt UI:**
- Issue: `MainWindow::openResume` limits the input dialog to IDs 1..999999 (`QInputDialog::getInt(..., 1, 1, 999999, 1, ...)`), while the web API accepts any `qint64`.
- Files: `src/ui/MainWindow.cpp` (openResume), `src/web/WebServer.cpp` (routes `/api/resumes/<arg>`)
- Impact: Resumes with IDs above 999999 can be created via web but cannot be opened from the desktop UI.
- Fix approach: Replace the ID dialog with a resume list (already on the Sprint 1 plan as "Улучшить выбор резюме") and remove the numeric cap.

**Tracked artifacts despite `.gitignore`:**
- Issue: `.gitignore` lists `.playwright-mcp/`, yet `git ls-files` reports `.playwright-mcp/*.yml` and a `.png` as tracked files.
- Files: `.playwright-mcp/` (tracked), `.gitignore`
- Impact: Browser-tool artifacts are committed to the repository and will keep appearing in diffs.
- Fix approach: Run `git rm -r --cached .playwright-mcp` and keep the ignore rule.

**Toolchain paths are hard-coded:**
- Issue: `CMakePresets.json` hard-codes `C:/msys64/mingw64`, and CI relies on the runner's preinstalled `C:\msys64` (`.github/workflows/ci.yml`).
- Files: `CMakePresets.json`, `.github/workflows/ci.yml`
- Impact: Building on a machine with MSYS2 installed elsewhere requires editing the tracked presets.
- Fix approach: Move the paths into `CMakeUserPresets.json` (already git-ignored) or use environment variables with defaults.

## Known Bugs

**`clang-format --Werror` fails on existing files:**
- Symptoms: The format check from `AGENTS.md` fails on older sources because of pointer alignment (`QString* error` in the existing code versus the `.clang-format` default of right-aligned pointers). Newer files use `PointerAlignment: Left`.
- Files: `.clang-format`, `src/storage/ResumeRepository.cpp`, `src/storage/ResumeRepository.h`, `src/ui/MainWindow.cpp`, `src/ui/MainWindow.h`
- Trigger: `clang-format --dry-run --Werror` on any of the files listed above.
- Workaround: Format the changed files with the repository config and accept the diff, or set `PointerAlignment` consistently in `.clang-format`.

**Desktop save path may fail silently after storage errors:**
- Symptoms: Repository errors (`Database is not open`) are returned to the web client as the generic "Storage error", while the Qt UI shows raw `QSqlError` text. Raw SQLite messages can include the database file path.
- Files: `src/web/WebServer.cpp` (handleLoad, handleSave), `src/ui/MainWindow.cpp` (reportError), `src/storage/ResumeRepository.cpp`
- Trigger: Open failure (read-only directory, locked file) followed by any save or load.
- Workaround: None; the path leak is limited to the local user.

## Security Considerations

**Unauthenticated local API exposes all resumes:**
- Risk: Any process running under the same Windows account can call `http://127.0.0.1:8765/api/resumes/<id>` and read or overwrite any resume. The Host and Origin checks defend against DNS rebinding and cross-site browser requests, but not against local processes or other local users.
- Files: `src/web/WebServer.cpp` (isTrustedRequest), `src/web/WebServer.h`
- Current mitigation: Bind to `QHostAddress::LocalHost`, strict Host header check, JSON-only Origin check on writes, 1 MB body limit.
- Recommendations: Add a per-session random token (passed through the URL fragment when `openInBrowser` opens the page and sent as a custom header), and document the threat model in `docs/`.

**Weak Content Security Policy:**
- Risk: The CSP in `addSecurityHeaders` allows `script-src 'self' 'unsafe-inline'` and `style-src 'self' 'unsafe-inline'`. Any injected markup in the editor would execute.
- Files: `src/web/WebServer.cpp` (addSecurityHeaders), `src/web/index.html`
- Current mitigation: `default-src 'none'`, `frame-ancestors 'none'`, `form-action 'none'`, and no `eval` (the e2e test had to be adapted to this CSP).
- Recommendations: Move the inline script into a separate resource served from `:/web/` and drop `'unsafe-inline'` for scripts; use a nonce or hash if inline is kept.

**Plaintext personal data at rest:**
- Risk: `resumes.sqlite` in the Qt `AppDataLocation` stores names, employers, and periods unencrypted. `.gitignore` excludes `*.sqlite`, which prevents accidental commits but not local exposure.
- Files: `src/ui/MainWindow.cpp` (data directory setup), `src/storage/ResumeRepository.cpp`
- Current mitigation: Database is local-only; `.gitignore` excludes `*.sqlite*`.
- Recommendations: Decide before Sprint 2 whether to encrypt (SQLCipher or application-level encryption) and document the decision in `docs/memory.md`.

**Error messages from storage are exposed to the UI:**
- Risk: `reportError` displays `QSqlError::text()` directly, which may include file paths or SQL fragments, contrary to the AGENTS.md rule to show errors without private data.
- Files: `src/ui/MainWindow.cpp` (reportError), `src/storage/ResumeRepository.cpp` (fail calls)
- Current mitigation: Web API maps errors to fixed messages.
- Recommendations: Log detailed errors to a local log file and show a generic message in the UI.

**Future LLM path has no code yet but rules are in place:**
- Risk: The planned `src/llm/` layer must not send personal data before anonymization and user confirmation (`docs/memory.md`). No guard exists in code yet.
- Files: `src/llm/README.md`, `.claude/rules/llm-security.md`
- Current mitigation: Documented rules only.
- Recommendations: Implement a payload-review gate before any network client is added.

## Performance Bottlenecks

**Full rewrite of stages on each save:**
- Problem: Each save deletes and reinserts all stages in one transaction.
- Files: `src/storage/ResumeRepository.cpp` (save)
- Cause: Lack of ID-based upsert (see Tech Debt).
- Improvement path: Upsert changed rows only; the current limit of 200 stages (`kMaxStages` in `src/application/ResumeJson.cpp`) keeps this acceptable for now.

**Per-statement prepare in loops:**
- Problem: `save` calls `q.prepare` for every stage inside the loop.
- Files: `src/storage/ResumeRepository.cpp` (save)
- Cause: Prepared statement is rebuilt per iteration.
- Improvement path: Prepare once before the loop and rebind values.

**Synchronous web requests on the GUI thread:**
- Problem: `QHttpServer` and `ResumeRepository` run on the Qt main thread, so a slow SQLite operation blocks the desktop UI.
- Files: `src/web/WebServer.cpp`, `src/storage/ResumeRepository.cpp`
- Cause: Single-threaded design (`docs/memory.md`: one module process).
- Improvement path: Acceptable for a single local user at Sprint 1 scale; revisit when LLM calls are added (AGENTS.md: UI must not block on network).

## Fragile Areas

**Single shared `ResumeRepository` across Qt and web:**
- Files: `src/ui/MainWindow.cpp`, `src/web/WebServer.cpp`, `src/storage/ResumeRepository.cpp`
- Why fragile: Both front ends mutate the same resume object model without locking or versioning. A Qt edit and a web edit to the same resume are last-write-wins, and the Qt in-memory `resume_` is not refreshed after a web save.
- Safe modification: Keep all repository calls on the main thread; add an `updatedAt` or revision column and reject stale writes if concurrency grows.
- Test coverage: `tests/WebApiTest.cpp` and `tests/ResumeE2ETest.cpp` do not test cross-front-end conflicts.

**Stage IDs used as list keys in the UI:**
- Files: `src/ui/MainWindow.cpp` (refreshList, updateSelectedStage), `src/domain/Resume.h`
- Why fragile: Selection is index-based and stages are rebuilt on save (see Tech Debt), so the selected row can point to a different logical stage after a reload.
- Safe modification: Reload selection by stored ID once stable IDs are implemented.
- Test coverage: Gaps; no test verifies selection after save.

**Port selection in the web server:**
- Files: `src/ui/MainWindow.cpp` (constructor, `kDefaultWebPort = 8765`), `src/web/WebServer.cpp` (listen)
- Why fragile: The Host header check compares against the actual bound port. If the fixed port is taken, the server falls back to a random port, and any bookmark or external script expecting 8765 fails silently. The status bar message is shown for 8 seconds only.
- Safe modification: Always pass the real port through `WebServer::url()` and avoid hard-coding it in the page or docs.

## Scaling Limits

**Single-user, single-file database:**
- Current capacity: One local SQLite file; up to 200 stages and 10000-character descriptions per resume per request (`src/application/ResumeJson.cpp`).
- Limit: No multi-user or sync support; resume list has no paging, and loading all resumes is not implemented.
- Scaling path: Add listing endpoints with paging before the Sprint 2 import of many documents.

## Dependencies at Risk

**Qt HttpServer module:**
- Risk: `QHttpServer` is a separate Qt module (`mingw-w64-x86_64-qt6-httpserver` in CI, minimum Qt 6.7 per `docs/memory.md`) and is less mature than core modules; it also requires `QHttpHeaders`.
- Impact: The web UI cannot build if the package is missing from MSYS2; CI and local setup need the extra package.
- Migration plan: If the module causes trouble, replace the transport with a minimal `QTcpServer` parser limited to the four routes, keeping `ResumeJson` and `ResumeRepository` unchanged.

## Missing Critical Features

**No resume deletion and no stage deletion:**
- Problem: The repository and API have no delete operations; the Qt UI can only create and edit stages.
- Blocks: Cleanup of test data and removal of incorrect facts from the candidate record.

**No document import or anonymization:**
- Problem: PDF/DOCX import, local anonymization, and payload preview (Sprint 2 in `docs/plan.md`) are not implemented.
- Blocks: All LLM, interview, and export features in Sprints 3-4.

## Test Coverage Gaps

**Browser e2e is skipped in CI:**
- What's not tested: `web_e2e` needs Node.js and Chrome; `.github/workflows/ci.yml` has no such step, so the test is skipped on `windows-latest`. The release configuration with this test is also unverified (`docs/memory.md`).
- Files: `tests/e2e/web_e2e.mjs`, `tests/e2e/WebE2EServer.cpp`, `.github/workflows/ci.yml`
- Risk: Regressions in the HTML editor or CSP are caught only on the developer PC.
- Priority: Medium

**Repository failure paths:**
- What's not tested: Behavior when `open` fails, the `Resume not found` branch in `save` (UPDATE with unknown ID), and rollback after a failed stage insert.
- Files: `src/storage/ResumeRepository.cpp`, `tests/ResumeE2ETest.cpp`
- Risk: Partial writes or leaked connections go unnoticed.
- Priority: High

**Security controls in the web server:**
- What's not tested: Oversized bodies (1 MB limit), wrong Origin with valid JSON, `Content-Type` variants, and rejection of non-loopback Host values beyond the existing `tests/WebApiTest.cpp` cases.
- Files: `src/web/WebServer.cpp`, `tests/WebApiTest.cpp`
- Risk: Regressions in the DNS-rebinding and CSRF defenses.
- Priority: High

**Desktop UI:**
- What's not tested: No automated tests for `src/ui/MainWindow.cpp`; behavior is verified only by manual runs.
- Files: `src/ui/MainWindow.cpp`, `src/ui/MainWindow.h`
- Risk: Widget-level regressions in stage editing.
- Priority: Low

---

*Concerns audit: 2026-10-24*
