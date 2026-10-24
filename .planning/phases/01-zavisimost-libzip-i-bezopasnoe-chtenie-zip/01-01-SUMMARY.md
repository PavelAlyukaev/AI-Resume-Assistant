---
phase: 01-zavisimost-libzip-i-bezopasnoe-chtenie-zip
plan: 01
subsystem: documents
tags: [libzip, zip, msys2, cmake, qt-test, ci, deploy, mingw]

# Dependency graph
requires: []
provides:
  - "resume_documents static library (libzip::zip, PUBLIC) and the zip_archive Qt Test target"
  - "documents::ZipArchive: read-only in-memory ZIP, exact-name lookup, budgeted readEntry"
  - "documents::DocxError with the seven codes (NotZip, Corrupt, Encrypted, LegacyDoc, TooLarge, Limit, MissingPart)"
  - "libzip.dll and its transitive DLLs in the installed folder; libzip in both CI install lists; smoke assertion for libzip.dll"
affects: [01-02, 01-03, 01-04, phase-02-docx-reader]

# Actuals (#2632): chars/4 over the realized diff (git diff aa0876a HEAD, 25191 characters)
actuals:
  tokens: 6298
  tasks: 3        # Task 1 (checkpoint) was resolved by the owner before this run; Tasks 2 and 3 were executed here
  commits: 2      # MEASURED: git rev-list --count aa0876a..HEAD (summary commit follows)

# Tech tracking
tech-stack:
  added: ["libzip 1.11.4-1 (MSYS2 mingw-w64-x86_64-libzip; planned 1.12.1-1, owner-accepted deviation)"]
  patterns:
    - "pimpl wrapper over libzip: no libzip type in the header"
    - "zip_source_buffer_create with freep 0: QByteArray owns the bytes, constData only"
    - "budgeted chunked read: zip_fread in 64 KiB chunks, Limit before the running total exceeds maxBytes"
    - "no path API in the documents layer; the caller reads with QFile"

key-files:
  created:
    - src/documents/DocxTypes.h
    - src/documents/ZipArchive.h
    - src/documents/ZipArchive.cpp
    - tests/ZipArchiveTest.cpp
  modified:
    - CMakeLists.txt
    - .github/workflows/ci.yml
    - cmake/DeployMinGWRuntime.cmake
    - docs/memory.md

key-decisions:
  - "libzip 1.11.4-1 accepted by the owner in place of the planned 1.12.1-1 (owner reply to the checkpoint; recorded in the Standard Stack section of 01-RESEARCH.md)"
  - "Failed zip_open_from_source: the source is NOT freed (owner-chosen branch c). zip.h and the zip_open_from_source man page are silent on ownership after failure; zip_source_free only says not to free a source after it was used successfully"
  - "Entry names containing NUL never match (hasEntry false, readEntry MissingPart); zip_name_locate takes a C string, so an embedded NUL would otherwise match a prefix"
  - "libzip.dll is installed with install(IMPORTED_RUNTIME_ARTIFACTS) and scanned explicitly by DeployMinGWRuntime.cmake; no version or wildcard mask"
  - "Every failed open maps to Corrupt in this plan; NotZip/Encrypted/LegacyDoc classification is plan 01-02"

patterns-established:
  - "Error codes only in src/documents (no texts); texts belong to the application layer"
  - "zip_error_t is held by an RAII wrapper and never read as text (no libzip message reaches UI or logs)"

requirements-completed: []  # The plan claims no requirement as complete; SAFE-01 and SAFE-02 close in plans 01-02 to 01-04

coverage:
  - id: D1
    description: "Byte-exact read of word/document.xml (with Cyrillic text) from an in-memory ZIP"
    requirement: "SAFE-02"
    verification:
      - kind: unit
        ref: "tests/ZipArchiveTest.cpp#openAndReadExactEntry"
        status: pass
    human_judgment: false
  - id: D2
    description: "Byte budget: budget one byte below the entry size gives Limit across several 64 KiB chunks; the exact size succeeds"
    requirement: "SAFE-02"
    verification:
      - kind: unit
        ref: "tests/ZipArchiveTest.cpp#readEntryEnforcesByteBudget"
        status: pass
    human_judgment: false
  - id: D3
    description: "Exact, case-sensitive entry lookup; WORD/document.xml and a NUL-suffixed name give MissingPart"
    requirement: "SAFE-02"
    verification:
      - kind: unit
        ref: "tests/ZipArchiveTest.cpp#exactNameMissingIsMissingPart"
        status: pass
    human_judgment: false
  - id: D4
    description: "Truncated ZIP and plain text fail to open with Corrupt"
    requirement: "SAFE-01"
    verification:
      - kind: unit
        ref: "tests/ZipArchiveTest.cpp#truncatedAndGarbageInputsAreCorrupt"
        status: pass
    human_judgment: false
  - id: D5
    description: "libzip in both MSYS2 install blocks of ci.yml and a smoke assertion for bin/libzip.dll"
    requirement: "SAFE-02"
    verification: []
    human_judgment: true
    rationale: "GitHub Actions was not run in this environment; the CI result must be confirmed on the PR"
  - id: D6
    description: "Installed folder carries bin/libzip.dll and its non-system DLLs and the application starts without MSYS2 on PATH"
    requirement: "SAFE-02"
    verification:
      - kind: other
        ref: "cmake --install build/mingw-debug --prefix <absolute path>; ldd on libzip.dll; timeout 10 on the installed AIResumeAssistant.exe with PATH limited to System32 and Windows"
        status: pass
    human_judgment: true
    rationale: "Clean-machine check is manual-only (01-VALIDATION.md); this run used the development machine"

# Metrics
duration: "not measured (the start time was not recorded at session start)"
completed: 2026-10-24
status: complete
---

# Phase 1 Plan 01: libzip в сборке, CI и установке; байтово точное чтение word/document.xml из памяти

**libzip 1.11.4 подключён через `resume_documents`; `ZipArchive` читает `word/document.xml` из буфера байт в байт с бюджетом, а `libzip.dll` попадает в установленную папку.**

## Performance

- **Duration:** не измерено (время старта не записано в начале сессии)
- **Started:** не записано
- **Completed:** 2026-10-24
- **Tasks:** 3 (Task 1 — checkpoint, решён владельцем до запуска; Tasks 2 и 3 выполнены здесь)
- **Files modified:** 8 (4 создано, 4 изменено)

## Accomplishments

- `ZipArchive::open` принимает только `QByteArray`: буфер через `zip_source_buffer_create` с `freep = 0`, открытие `zip_open_from_source` с `ZIP_RDONLY | ZIP_CHECKCONS`; функции с путём в слое documents нет.
- `readEntry` ищет имя точно (без `ZIP_FL_NOCASE`), читает `zip_fread` кусками по 64 KiB, возвращает `Limit` до превышения бюджета и никогда не отдаёт частичный результат.
- Тесты `zip_archive` (Qt Test) покрывают байтово точное чтение, границу бюджета, точное совпадение имён и отказ `Corrupt` для обрезанного и мусорного входа.
- `libzip` в обоих блоках `install:` CI и проверка `bin/libzip.dll` в smoke-шаге; `cmake --install` кладёт `libzip.dll`, а `DeployMinGWRuntime.cmake` сканирует его явно, чтобы разрешились транзитивные DLL.

## Task Commits

1. **Task 1: Verify and install the single new package** — checkpoint, решён владельцем (`pacman -Q mingw-w64-x86_64-libzip` → `1.11.4-1`); код не менялся.
2. **Task 2: Tracer — in-memory DOCX-shaped ZIP through libzip** — `f9eb343` (feat)
3. **Task 3: Ship libzip in CI lists and in the installed folder** — `941857f` (build)

**Plan metadata:** этот файл коммитится отдельным коммитом `docs(01-01): ...` после `941857f`.

## Files Created/Modified

- `src/documents/DocxTypes.h` — enum class `DocxError` с семью кодами, без текстов.
- `src/documents/ZipArchive.h` — класс `documents::ZipArchive` (pimpl, без заголовков libzip), `open`, `hasEntry`, `readEntry`, удалённое копирование.
- `src/documents/ZipArchive.cpp` — обёртка libzip: буфер, открытие из памяти, точный поиск, чтение с бюджетом, деструктор закрывает архив до освобождения буфера.
- `tests/ZipArchiveTest.cpp` — цель `zip_archive_test`; CRC-32, построитель stored-ZIP и фикстура DOCX в памяти, без бинарных файлов.
- `CMakeLists.txt` — `find_package(libzip CONFIG REQUIRED)`, `resume_documents`, `zip_archive_test` с `add_test(NAME zip_archive …)`, install-правило `IMPORTED_RUNTIME_ARTIFACTS libzip::zip`.
- `.github/workflows/ci.yml` — `mingw-w64-x86_64-libzip` в оба блока установки; проверка `libzip.dll is missing` в smoke-шаге.
- `cmake/DeployMinGWRuntime.cmake` — явное добавление `bin/libzip.dll` в список сканируемых файлов (без маски).
- `docs/memory.md` — запись журнала по плану 01-01.

## Decisions Made

- Версия libzip 1.11.4-1 вместо 1.12.1-1 — решение владельца на checkpoint.
- Ветка (c) для источника после неуспешного открытия — решение владельца: источник не освобождается.
- Имена с NUL не совпадают ни с одной записью (`hasEntry` = false, `readEntry` = `MissingPart`): `zip_name_locate` принимает C-строку, и без проверки `word/document.xml\0.bak` совпал бы с `word/document.xml`.
- Установка `libzip.dll` через install-правило, а не через запасной вариант с `find_file`: правило сработало.

## Deviations from Plan

### Принято владельцем (не отклонения исполнителя)

**1. Версия libzip 1.11.4-1 вместо 1.12.1-1.** Владелец принял отклонение в ответе на checkpoint. Запись в `docs/memory.md` и в этом файле.

**2. Ветка (c) для владения источником при ошибке открытия.** Проверено: `zip.h` не описывает владение; в man-странице `zip_open_from_source` сказано только, что при ошибке заполняется `ze`; в man-странице `zip_source_free` сказано только, что источник нельзя освобождать после успешного `zip_open_from_source`. Код не освобождает источник при ошибке. Исходники libzip не читались, поэтому возможна мелкая утечка этой структуры, двойного освобождения нет.

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Отказ для имён с NUL**
- **Found during:** Task 2 (проектирование `hasEntry`/`readEntry`)
- **Issue:** `zip_name_locate` принимает C-строку; `std::string_view` может содержать NUL, и `word/document.xml\0.bak` совпал бы с `word/document.xml`.
- **Fix:** имена с `\0` отклоняются до libzip; в тесте `exactNameMissingIsMissingPart` есть такой случай.
- **Files modified:** `src/documents/ZipArchive.cpp`, `tests/ZipArchiveTest.cpp`
- **Verification:** `ctest --preset debug -R zip_archive`: `exactNameMissingIsMissingPart` PASS.
- **Committed in:** `f9eb343`

**2. [Rule 3 - Blocking] Абсолютный `--prefix` для установки**
- **Found during:** Task 3, шаг 4 (`cmake --install build/mingw-debug --prefix build/mingw-debug/install-check`)
- **Issue:** с относительным префиксом установка падает в `Qt6CoreDeploySupport.cmake:38`: «Given qt.conf path is not an absolute path». `libzip.dll` успел установиться до падения.
- **Fix:** та же директория, записанная абсолютным путём; изменений в файлах не потребовалось.
- **Files modified:** нет.
- **Verification:** установка с абсолютным путём завершилась с кодом 0.
- **Committed in:** не применимо (команда, а не код). Записано в `docs/memory.md`.

**3. [Verify-команда] `QT_QPA_PLATFORM=offscreen` не работает для установленной копии**
- **Found during:** Task 3, запуск установленного exe
- **Issue:** в установке есть только `share/qt6/plugins/platforms/qwindows.dll`; `qoffscreen.dll` есть в MSYS2 Qt, но деплой его не копирует. С offscreen exe не стартует (exit 127).
- **Fix:** запуск без `QT_QPA_PLATFORM` (платформа по умолчанию, как в CI smoke-шаге).
- **Files modified:** нет. Деплой не менялся: это область деплоя, не libzip.

**4. [Форматирование] Новые файлы отформатированы по `.clang-format` проекта**
- Не было в плане. Применено `clang-format -i -style=file` к четырём новым файлам; `clang-format --dry-run --Werror` для них проходит (exit 0).

**Total deviations:** 2 принятых владельцем, 2 auto-fixed (1 Rule 2, 1 Rule 3), 1 исправление verify-команды, 1 форматирование.
**Impact on plan:** код соответствует плану; отличия только в команде установки (абсолютный путь) и в способе запуска установленного exe.

## Issues Encountered

- Каталога фазы нет в рабочем дереве worktree: `01-*-PLAN.md`, `01-CONTEXT.md` и остальные документы фазы лежат в основном checkout как неотслеживаемые файлы. Читались оттуда; SUMMARY записан в worktree.
- `pacman` не в PATH оболочки Bash; использован `/c/msys64/usr/bin/pacman.exe`.
- Защита worktree отклоняла некоторые составные команды; они разбивались на простые. Одна команда с `rm -rf` была отклонена правами; удалять ничего не потребовалось (каталог установки новый).
- `QTest` при запуске не выводит в stdout в этой среде; результаты снимались `-o <file>,txt`.
- `ctest` с `web_e2e`: пропущен, так как нет `tests/e2e/node_modules` (как и до изменений).
- `clang-format --dry-run --Werror` на старых файлах проекта не проходит (расхождение с `.clang-format`, уже записано в `docs/memory.md` от 2026-10-13); на новых файлах проходит.
- `clang-tidy` на `src/documents/ZipArchive.cpp`: exit 0, 12 предупреждений, все стилевые или некритичные: `modernize-use-trailing-return-type` (9), `modernize-use-nodiscard` для `hasEntry`, `performance-enum-size` для `DocxError`, `bugprone-implicit-widening-of-multiplication-result` для константы `64 * 1024`. Не исправлено в этом прогоне.
- `.planning/STATE.md` и `ROADMAP.md` не обновлялись: владелец разрешил менять только файлы плана и `docs/memory.md`. Обновление состояния делает оркестратор.

## Verification (реальные запуски)

| Команда | Результат |
|---|---|
| `cmake --preset mingw-debug` (до изменений) | Configuring done, Generating done |
| `cmake --build --preset debug` (до изменений) | exit 0 |
| `ctest --preset debug` (до изменений) | 3/3 passed |
| `cmake --build --preset debug` (после Task 2) | exit 0, `[15/15] Linking CXX executable zip_archive_test.exe` |
| `ctest --preset debug -R zip_archive` | `100% tests passed, 0 failed out of 1` |
| `zip_archive_test.exe -o <file>,txt` | `PASS` по четырём тестовым функциям плюс initTestCase и cleanupTestCase; `Totals: 6 passed, 0 failed, 0 skipped` |
| `cmake --build --preset debug` (после Task 3) | exit 0 |
| `cmake --install build/mingw-debug --prefix <absolute>` | exit 0 |
| `test -f .../install-check/bin/libzip.dll` | present |
| Проверка прямых импортов libzip.dll (план) | `dependency check passed` |
| `ldd` для `libzip.dll` с PATH только из `install-check/bin` и System32 | `not found` 0 |
| `ldd` для установленного `AIResumeAssistant.exe` с тем же PATH | `not found` 0 |
| Запуск установленного exe, PATH = System32;Windows, 10 с, без QT_QPA_PLATFORM | exit 124 (жив до таймаута) |
| `objdump -p AIResumeAssistant.exe \| grep -ci libzip` (до Task 3, build) | 0 |
| То же после Task 3 (build и install-check) | 0 и 0 |
| `ctest --preset debug` (после Task 3) | 4/4 passed (`resume_e2e`, `resume_json`, `zip_archive`, `web_api`), `web_e2e` пропущен |
| `clang-format --dry-run --Werror` (4 новых файла) | exit 0 |
| `clang-tidy src/documents/ZipArchive.cpp -p build/mingw-debug` | exit 0, 12 предупреждений (см. выше) |

Вывод `objdump`: в `AIResumeAssistant.exe` нет импорта libzip, поэтому явное install-правило и сканирование в `DeployMinGWRuntime.cmake` нужны. Контрфакт (установка без них) не проверялся, поэтому нельзя утверждать, что без сканирования отсутствовали бы именно `libbz2-1.dll`, `liblzma-5.dll`, `zlib1.dll` или `libzstd.dll`.

## Known Stubs

None.

## Threat Flags

None: новых сетевых точек, путей доступа или границ доверия относительно плана нет.

## Next Phase Readiness

- Plan 01-02 (классификация `NotZip`, `Encrypted`, `LegacyDoc`, `MissingPart`) может опираться на `ZipArchive::open` и `hasEntry`; сейчас все сбои открытия отображаются в `Corrupt`.
- Открыто: CI на GitHub не запускался (нужна проверка в PR); чистая машина не проверялась (ручная проверка по VALIDATION.md).
- Открыто: владение источником при ошибке открытия (ветка c) не проверено по исходникам libzip.
- Открыто: `QT_QPA_PLATFORM=offscreen` в verify-команде Task 3 этого плана не работает для установленной копии, пока деплой не копирует `qoffscreen.dll`; команду нужно поправить до повторного использования.
- Ветка `worktree-agent-ae75ac1f74e7edc04`; не отправлена (push не выполнялся).

---
*Phase: 01-zavisimost-libzip-i-bezopasnoe-chtenie-zip*
*Completed: 2026-10-24*
