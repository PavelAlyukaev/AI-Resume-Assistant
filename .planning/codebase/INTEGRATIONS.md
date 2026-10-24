---
last_mapped_commit: 21afce5aefddf6147b4486c8eb0a485474093e56
last_mapped_at: 2026-10-24
---
# External Integrations

**Analysis Date:** 2026-10-24

## APIs & External Services

**LLM / AI providers:**
- Not integrated. OpenAI, Anthropic and Gemini are planned for Sprint 3 (`docs/plan.md`) through a provider interface. `src/llm/` contains only `src/llm/README.md`; no SDK, HTTP client or API key handling exists in code.
  - SDK/Client: none
  - Auth: none (no provider API key variable references in source)
  - Security rules that will apply once implemented: `.claude/rules/llm-security.md` (path-scoped to `src/llm/**`), and the privacy rule in `README.md` (personal data must be anonymized locally before any API call)

**Document conversion services:**
- Not integrated. PDF/DOCX import and DOCX/PDF export are planned (`docs/plan.md` Sprints 2 and 4). `src/documents/` contains only `src/documents/README.md`.

**Outbound network calls from the application:**
- None detected. The application does not open outbound HTTP, HTTPS or socket connections in `src/`. `src/web/index.html` declares a Content-Security-Policy and references no external resources (`README.md`: the page does not access external resources).

## Data Storage

**Databases:**
- SQLite, local file only
  - Connection: `QSqlDatabase::addDatabase("QSQLITE", <uuid connection name>)` with database path `<AppDataLocation>/resumes.sqlite` (`src/storage/ResumeRepository.cpp`, `src/ui/MainWindow.cpp`)
  - Client: Qt SQL (`Qt6::Sql`) with the `QSQLITE` driver; no ORM. Queries are built in `src/storage/ResumeRepository.cpp`.
  - Location on Windows: `%APPDATA%` application data folder (kept on uninstall per `installer/AIResumeAssistant.iss`)
  - Git ignores `*.sqlite`, `*.sqlite-shm`, `*.sqlite-wal` (`.gitignore`)

**File Storage:**
- Local filesystem only. No cloud storage. Resume data is persisted in the SQLite file above; no document upload or export files are produced yet.

**Caching:**
- None

## Authentication & Identity

**Auth Provider:**
- None. The app is single-user and local. No login, OAuth, JWT or session logic exists.
  - Local web API protection (not authentication): `src/web/WebServer.cpp` `isTrustedRequest()` validates the `Host` header, the `Origin` header for state-changing requests, and requires `application/json` for JSON writes; responses carry security headers including `Content-Security-Policy` and `Referrer-Policy: no-referrer`.

## Local HTTP Interface (inbound, loopback only)

**Embedded web server:**
- Qt HttpServer (`Qt6::HttpServer`) bound to `QHostAddress::LocalHost` (127.0.0.1) via `QTcpServer` in `src/web/WebServer.cpp`
- Started from the desktop app (`src/ui/MainWindow.cpp`), default port `8765`, falls back to an ephemeral port if busy; the address is shown in the status bar
- Opened from the menu "Файл → Открыть веб-версию" via `QDesktopServices` (`src/ui/MainWindow.cpp`)

**Routes (`src/web/WebServer.cpp`):**
- `GET /` - serves the embedded `src/web/index.html` (Qt resource prefix `/web`, defined in `CMakeLists.txt` via `qt_add_resources`)
- `GET /api/resumes/<id>` - load a resume as JSON
- `POST /api/resumes` - create a resume (JSON body)
- `PUT /api/resumes/<id>` - update a resume (JSON body)

**Not exposed:** the server does not listen on non-loopback interfaces; it is unreachable from the network per `README.md` and `docs/memory.md`.

## Monitoring & Observability

**Error Tracking:**
- None

**Logs:**
- No logging framework; errors are shown in Qt dialogs and the status bar (`src/ui/MainWindow.cpp`).

## CI/CD & Deployment

**Hosting:**
- No hosting. Distributed as a Windows installer built locally or on CI.

**CI Pipeline:**
- GitHub Actions, `.github/workflows/ci.yml`
  - Triggers: push and pull requests to `main`
  - Runner: `windows-latest` with MSYS2 MINGW64, installing `mingw-w64-x86_64-qt6-httpserver` among other packages
  - Steps: configure, build and `ctest` for presets `mingw-debug` and `mingw-release`
  - Job `Inno Setup installer`: builds `setup.exe`, installs it silently, checks that the app starts without MSYS2 in PATH, uploads artifact `AIResumeAssistant-setup`
  - Browser e2e (`web_e2e`) is skipped in CI (no Node or Chrome step, per `docs/memory.md`)

**Developer tooling integrations (not runtime):**
- Claude Code: `CLAUDE.md`, `AGENTS.md`, `.claude/settings.json`, `.claude/hooks/*.py`, `.claude/skills/*`, `.claude/rules/llm-security.md`
- MCP: `.mcp.json` (Playwright MCP server for browser checks; `.playwright-mcp/` holds its snapshot output and is git-ignored)
- Playwright-core 1.49.1 with installed Chrome for `tests/e2e/web_e2e.mjs`
- Inno Setup 6 (`iscc`) for the `installer` CMake target

## Environment Configuration

**Required env vars:**
- None read by application code.
- Developer shell: `C:/msys64/mingw64/bin` must be on `PATH` for builds and for running the executable (Qt DLLs), per `README.md` and `CMakePresets.json`.
- Future LLM keys will need an environment or secret store. A checked-in template exists at the repo root; its contents were not read by this analysis (access to environment files is blocked by the project's guard hook).

**Secrets location:**
- No secrets are stored or required today. Never commit API keys (`README.md` Privacy section; `AGENTS.md` section 4). Local environment files are git-ignored (`.gitignore`).

## Webhooks & Callbacks

**Incoming:**
- None. The local HTTP API above is for the app's own browser UI only.

**Outgoing:**
- None

---

*Integration audit: 2026-10-24*
