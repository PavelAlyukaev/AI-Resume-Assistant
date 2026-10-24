---
last_mapped_commit: 21afce5aefddf6147b4486c8eb0a485474093e56
last_mapped_at: 2026-10-24
---
# Technology Stack

**Analysis Date:** 2026-10-24

## Languages

**Primary:**
- C++23 (minimum; `CMAKE_CXX_STANDARD 23`, extensions off, required) - all application, domain, storage, web and test code in `src/` and `tests/` (e.g. `src/main.cpp`, `src/storage/ResumeRepository.cpp`, `src/web/WebServer.cpp`)

**Secondary:**
- HTML/CSS/JavaScript (vanilla, no framework) - embedded local web UI in `src/web/index.html`, bundled as a Qt resource
- JavaScript (ES modules, Node.js) - browser e2e test driver in `tests/e2e/web_e2e.mjs`
- CMake language - build definitions in `CMakeLists.txt`, `cmake/DeployMinGWRuntime.cmake`
- Inno Setup script - Windows installer in `installer/AIResumeAssistant.iss`
- Python 3 - Claude Code hooks in `.claude/hooks/guard_env.py` and `.claude/hooks/check_cpp_format.py` (developer tooling, not shipped)
- SQL (SQLite dialect) - resume schema and queries in `src/storage/ResumeRepository.cpp`

## Runtime

**Environment:**
- Native Windows x64 desktop process, built with MSYS2 MinGW-w64 GCC (`C:/msys64/mingw64`); MSVC is not used (per `docs/memory.md`)
- Target OS: Windows 10/11 x64; installer declares `MinVersion=10.0` and `ArchitecturesAllowed=x64compatible` (`installer/AIResumeAssistant.iss`)
- Node.js: only needed for the optional browser e2e test (`tests/e2e`); not needed at runtime

**Package Manager:**
- CMake 3.25+ as the build system (`CMakeLists.txt`, `cmake_minimum_required(VERSION 3.25)`)
- Ninja as the generator (`CMakePresets.json`, `generator: Ninja`)
- Qt 6 and third-party C++ libraries come from the MSYS2 pacman environment, not a vendored package manager
- npm for `tests/e2e` only
  - Lockfile: present (`tests/e2e/package-lock.json`)
  - Root C++ dependencies: no lockfile (CMake `find_package`, no FetchContent/vcpkg/Conan manifest detected)

## Frameworks

**Core:**
- Qt 6.7+ (`find_package(Qt6 6.7 REQUIRED COMPONENTS Core Widgets Sql Network HttpServer Test)` in `CMakeLists.txt`) - application framework
  - Qt6::Core - event loop, `QStandardPaths`, `QJsonDocument`, `QUuid`, `QFile`
  - Qt6::Widgets - desktop UI in `src/ui/MainWindow.cpp` and `src/ui/MainWindow.h`
  - Qt6::Sql - SQLite access via the `QSQLITE` driver in `src/storage/ResumeRepository.cpp`
  - Qt6::Network - loopback socket binding (`QHostAddress::LocalHost`) in `src/web/WebServer.cpp`
  - Qt6::HttpServer - local JSON/HTML HTTP server in `src/web/WebServer.h` and `src/web/WebServer.cpp`
  - Qt6::Test - unit and end-to-end tests (`tests/*.cpp`)
- Qt Quick, QML and QtWebEngine are not used

**Testing:**
- Qt Test (`QtTest`) - C++ test executables `resume_e2e_test`, `resume_json_test`, `web_api_test`, `web_e2e_server` (`CMakeLists.txt`, `enable_testing()` and `add_test`)
- CTest - test runner, driven by `CMakePresets.json` test presets (`debug`, `release`) with `outputOnFailure`
- Playwright-core 1.49.1 (`tests/e2e/package.json`) - drives an installed Chrome for the `web_e2e` test; the test is skipped by CMake when Node or `tests/e2e/node_modules/playwright-core` is missing

**Build/Dev:**
- CMake 3.25+ with `qt_standard_project_setup`, `qt_add_executable`, `qt_add_resources`, `qt_generate_deploy_app_script` (Qt 6 CMake API) - `CMakeLists.txt`
- `CMAKE_AUTOMOC ON`, `CMAKE_EXPORT_COMPILE_COMMANDS ON` - `CMakeLists.txt`
- CMake presets `mingw-base` (hidden), `mingw-debug`, `mingw-release`, build presets `debug`/`release`, test presets `debug`/`release` - `CMakePresets.json`
- windeployqt (via `qt_generate_deploy_app_script`) and `cmake/DeployMinGWRuntime.cmake` - packaging of Qt and MinGW runtime DLLs
- Inno Setup 6 (`iscc`) - optional `installer` CMake target, found in standard install paths (`CMakeLists.txt`)
- clang-format - style check, `.clang-format` (LLVM base, `PointerAlignment: Left` in new files per `docs/memory.md`)
- clang-tidy - lint, `.clang-tidy` (short config, requires `build/mingw-debug/compile_commands.json`)
- GitHub Actions - `.github/workflows/ci.yml`

## Key Dependencies

**Critical (linked libraries):**
- Qt6::Core, Qt6::Widgets, Qt6::Sql, Qt6::Network, Qt6::HttpServer, Qt6::Test - declared in `CMakeLists.txt`
- SQLite (through Qt's `QSQLITE` plugin, provided by the MSYS2 Qt build) - sole database engine; `src/storage/ResumeRepository.cpp`
- Qt SQL driver plugin `qsqlite` - the installer keeps only this SQL driver (`cmake/DeployMinGWRuntime.cmake` removes other `sqldrivers` DLLs)

**Infrastructure / runtime support:**
- MSYS2 MinGW-w64 runtime DLLs (libgcc, libstdc++, libwinpthread) and Qt's third-party DLLs (ICU, zlib, zstd, freetype, harfbuzz, pcre2) - resolved by `cmake/DeployMinGWRuntime.cmake`
- MSYS2 packages required (from `README.md`): `mingw-w64-x86_64-gcc`, `-cmake`, `-ninja`, `-qt6-base`, `-qt6-httpserver`
- playwright-core 1.49.1 - dev-only, `tests/e2e/package.json`

**Planned, not implemented (do not treat as present):**
- PDF/DOCX import, OpenAI/Anthropic/Gemini provider clients (`src/llm/` contains only `README.md`), DOCX/PDF export. Per `docs/plan.md` (Sprints 2-4). `src/documents/` contains only `README.md`.

## Configuration

**Environment:**
- No runtime environment variables are read by the application code in `src/`; configuration is compile-time (CMake presets) and file-system based.
- A template file with placeholder variable names is checked in at the repo root; the real local secrets file is git-ignored. Neither was read for this analysis.
- Required build-time paths are hard-coded in `CMakePresets.json`: `CMAKE_CXX_COMPILER` = `C:/msys64/mingw64/bin/g++.exe`, `CMAKE_MAKE_PROGRAM` = `C:/msys64/mingw64/bin/ninja.exe`, `CMAKE_PREFIX_PATH` = `C:/msys64/mingw64`, and `PATH` prepended with `C:/msys64/mingw64/bin`.
- Runtime data location: `QStandardPaths::AppDataLocation` + `/resumes.sqlite` (`src/ui/MainWindow.cpp`), i.e. under `%APPDATA%` on Windows.
- Web server port: default `8765` on `127.0.0.1`, falls back to an ephemeral port if busy (`src/ui/MainWindow.cpp`, `src/web/WebServer.cpp`).

**Build:**
- `CMakeLists.txt` - project, Qt components, libraries, executable, tests, installer target
- `CMakePresets.json` - configure/build/test presets for MinGW Debug and Release
- `cmake/DeployMinGWRuntime.cmake` - post-deploy runtime DLL collection
- `installer/AIResumeAssistant.iss` - Inno Setup script (versioned via `/DAppVersion`)
- `.clang-format`, `.clang-tidy` - formatting and lint configuration
- `.claude/settings.json` - Claude Code permissions and hooks (developer tooling)
- `.mcp.json` - MCP server configuration for developer tooling (Playwright)
- `.gitignore` - excludes `/build/`, `/cmake-build-*/`, `CMakeUserPresets.json`, environment and secret files (except the checked-in template), `*.sqlite`, `node_modules/`, `.playwright-mcp/`

## Platform Requirements

**Development:**
- Windows with MSYS2 installed at `C:/msys64`, MINGW64 environment with GCC, CMake, Ninja and Qt 6 (Base + HttpServer) - `README.md`, `CMakePresets.json`
- Optional: Inno Setup 6 for the `installer` target
- Optional: Node.js and `npm install` in `tests/e2e` plus an installed Google Chrome for `web_e2e`
- CLion may use the presets directly (`README.md`)

**Production:**
- Windows 10/11 x64 desktop. Distributed as a self-contained folder (`cmake --install`) and a Setup.exe built by Inno Setup (`installer/AIResumeAssistant.iss`, CI artifact `AIResumeAssistant-setup`).
- No server, container or cloud deployment target.

---

*Stack analysis: 2026-10-24*
