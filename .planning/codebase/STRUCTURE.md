---
last_mapped_commit: 21afce5aefddf6147b4486c8eb0a485474093e56
last_mapped_at: 2026-10-24
---
# Codebase Structure

**Analysis Date:** 2026-10-24

## Directory Layout

```
AI-Resume-Assistant/
├── src/                     # Application source (C++23)
│   ├── main.cpp             # Desktop entry point
│   ├── domain/              # Plain data model (Resume, CareerStage)
│   ├── application/         # JSON validation and mapping (ResumeJson)
│   ├── storage/             # SQLite persistence (ResumeRepository)
│   ├── ui/                  # Qt Widgets UI (MainWindow)
│   ├── web/                 # Loopback HTTP server and embedded HTML page
│   ├── documents/           # Placeholder: PDF/DOCX import and anonymization (README only)
│   └── llm/                 # Placeholder: cloud LLM adapters (README only)
├── tests/                   # Qt Test executables and browser e2e helper
│   └── e2e/                 # Node/Playwright browser test and C++ server helper
├── cmake/                   # CMake helper scripts (MinGW runtime deployment)
├── installer/               # Inno Setup script (AIResumeAssistant.iss)
├── docs/                    # Plan, memory, verification and about documents (Russian)
├── screenshots/             # Images referenced by docs and README
├── .github/workflows/       # CI workflow (ci.yml)
├── .claude/                 # Claude Code settings, hooks, rules and skills
├── .playwright-mcp/         # Playwright MCP session artifacts (committed)
├── CMakeLists.txt           # Root build definition
├── CMakePresets.json        # mingw-debug / mingw-release presets
├── AGENTS.md, CLAUDE.md     # Agent instructions
└── README.md
```

Build output (not committed): `build/` and `cmake-build-*/`, per `.gitignore`.

## Directory Purposes

**`src/domain/`:**
- Purpose: Core data types with no I/O.
- Contains: `Resume.h`.
- Key files: `src/domain/Resume.h`

**`src/application/`:**
- Purpose: Conversion and validation of untrusted JSON to and from domain types.
- Contains: `ResumeJson.h`, `ResumeJson.cpp`, `README.md` (planned services note).
- Key files: `src/application/ResumeJson.cpp`

**`src/storage/`:**
- Purpose: SQLite persistence through Qt SQL.
- Contains: `ResumeRepository.h`, `ResumeRepository.cpp`.
- Key files: `src/storage/ResumeRepository.cpp` (schema in `open()`, transactional `save()`, `load()`).

**`src/ui/`:**
- Purpose: Qt Widgets desktop window.
- Contains: `MainWindow.h`, `MainWindow.cpp`.
- Key files: `src/ui/MainWindow.cpp`

**`src/web/`:**
- Purpose: Local web UI and JSON API.
- Contains: `WebServer.h`, `WebServer.cpp`, `index.html` (embedded via Qt resources, prefix `/web`).
- Key files: `src/web/WebServer.cpp` (routes, trust checks), `src/web/index.html` (front end).

**`src/documents/` and `src/llm/`:**
- Purpose: Reserved for future features. Contain only `README.md`; no code yet.
- Generated: No. Committed: Yes.

**`tests/`:**
- Purpose: Qt Test executables registered with CTest.
- Contains: `ResumeE2ETest.cpp`, `ResumeJsonTest.cpp`, `WebApiTest.cpp`.
- Key files: `tests/WebApiTest.cpp` (HTTP API against a real `WebServer`).

**`tests/e2e/`:**
- Purpose: Browser end-to-end test for the web UI.
- Contains: `web_e2e.mjs` (Playwright-core scenario), `WebE2EServer.cpp` (prints `PORT=` on stdout), `package.json`, `package-lock.json`.
- Key files: `tests/e2e/web_e2e.mjs`. Requires `npm install` in `tests/e2e`; otherwise CMake skips the test.

**`cmake/`:**
- Purpose: Deployment helper.
- Key files: `cmake/DeployMinGWRuntime.cmake` (copies MinGW runtime DLLs for `cmake --install`).

**`installer/`:**
- Purpose: Windows installer definition.
- Key files: `installer/AIResumeAssistant.iss`; built by the `installer` CMake target when Inno Setup is found.

**`docs/`:**
- Purpose: Project plan and memory for agents and owner.
- Key files: `docs/plan.md`, `docs/memory.md`, `docs/verification.md`, `docs/about.md`.

**`.claude/`:**
- Purpose: Claude Code configuration.
- Contains: `settings.json` (permissions and hooks), `hooks/` (format and guard scripts), `rules/llm-security.md`, `skills/write-tests/SKILL.md`, `skills/review-changes/SKILL.md`.

## Key File Locations

**Entry Points:**
- `src/main.cpp`: Desktop `main`, creates `QApplication` and `MainWindow`.
- `src/web/WebServer.cpp`: HTTP route table (constructor).

**Configuration:**
- `CMakeLists.txt`: Targets `resume_core`, `resume_web`, `AIResumeAssistant`, `resume_e2e_test`, install and installer rules.
- `CMakePresets.json`: Presets `mingw-debug` and `mingw-release`; toolchain at `C:/msys64/mingw64`.
- `.clang-format`, `.clang-tidy`: Formatting and lint configuration.
- `.mcp.json`: MCP server configuration for the agent.

**Core Logic:**
- `src/storage/ResumeRepository.cpp`: Persistence.
- `src/application/ResumeJson.cpp`: JSON validation.
- `src/ui/MainWindow.cpp`: Widgets behavior.

**Testing:**
- `tests/`: C++ Qt Test sources.
- `tests/e2e/`: Browser e2e.

**CI:**
- `.github/workflows/ci.yml`: Builds and runs `ctest` on `windows-latest` with MSYS2 MINGW64.

## Naming Conventions

**Files:**
- C++ headers and sources: PascalCase matching the primary class or namespace, e.g. `ResumeRepository.h`, `WebServer.cpp`, `ResumeJson.h`.
- Tests: `<Subject>Test.cpp` in `tests/` (`ResumeJsonTest.cpp`, `WebApiTest.cpp`) or `<Subject>E2ETest.cpp` for end-to-end.
- Browser test: `snake_case` (`web_e2e.mjs`).
- Documentation: lowercase `README.md` in `src/*/` subdirectories; uppercase `AGENTS.md`, `CLAUDE.md`; lowercase `docs/*.md`.
- Each `src/<layer>/` directory contains a header/source pair per class.

**Directories:**
- Lowercase single words for layers: `domain`, `application`, `storage`, `ui`, `web`, `documents`, `llm`.
- Lowercase for other folders: `tests`, `docs`, `cmake`, `installer`.

**Namespaces and includes:**
- Include paths are rooted at `src/` (`target_include_directories(resume_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)`), so write `#include "storage/ResumeRepository.h"`, not relative paths.
- Namespace used for free-function groups: `ResumeJson`.

## Where to Add New Code

**New domain type or business rule:**
- Primary code: `src/domain/<Name>.h`; logic that operates on it goes in `src/application/`.
- Tests: `tests/<Name>Test.cpp`, registered in `CMakeLists.txt` with `add_executable` plus `add_test` and linked to `resume_core` and `Qt6::Test`.

**New persistence behavior (table, query, migration):**
- Implementation: `src/storage/` (extend `ResumeRepository` or add a sibling class that takes a `QSqlDatabase` connection name).
- Tests: extend `tests/ResumeE2ETest.cpp` or add a new test target.

**New JSON endpoint or field:**
- Mapping: `src/application/ResumeJson.cpp` (`toJson` / `fromJson`).
- Route: `src/web/WebServer.cpp` (constructor route table and a `handle*` method, declared in `WebServer.h`).
- Front end: `src/web/index.html`. Keep the CSP compatible (no `eval`).
- Tests: `tests/WebApiTest.cpp`; browser flow in `tests/e2e/web_e2e.mjs` if the UI changes.

**New Widgets screen or control:**
- Implementation: `src/ui/<Name>.h` and `src/ui/<Name>.cpp`, then add the sources to the `AIResumeAssistant` target in `CMakeLists.txt`.
- Keep logic in `src/application/`, not in the widget class.

**New document import or anonymization:**
- Implementation: `src/documents/` (currently only `README.md`). Add a CMake library target if it needs linking to Qt or third-party parsers.

**New LLM adapter:**
- Implementation: `src/llm/`. Follow `.claude/rules/llm-security.md` for every file in this directory (applies to `src/llm/**`).

**Utilities:**
- No shared utility directory exists. Add small helpers beside their only caller, or create `src/common/` if a second layer needs them.

## Special Directories

**`build/` and `cmake-build-*/`:**
- Purpose: CMake build trees (`build/mingw-debug`, `build/mingw-release`, CLion builds).
- Generated: Yes.
- Committed: No (listed in `.gitignore`).

**`.playwright-mcp/`:**
- Purpose: Playwright MCP snapshots (`*.yml`, `*.png`) written during browser sessions.
- Generated: Yes.
- Committed: Yes (present in the tracked file list).

**`screenshots/`:**
- Purpose: Images referenced from `docs/` and `README.md`.
- Generated: No (manually captured).
- Committed: Yes.

**`.planning/`:**
- Purpose: GSD planning and codebase map documents (this directory).
- Generated: Yes.
- Committed: Per project workflow.

---

*Structure analysis: 2026-10-24*
