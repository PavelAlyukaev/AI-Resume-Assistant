---
phase: 01-zavisimost-libzip-i-bezopasnoe-chtenie-zip
plan: 04
subsystem: documents
tags: [zip-bomb, compression-ratio, entry-limit, duplicate-names, shared-budget, libzip, qt-test, mingw]
status: complete

# Dependency graph
requires:
  - phase: 01-zavisimost-libzip-i-bezopasnoe-chtenie-zip
    provides: "plans 01-01 to 01-03: ZipArchive, DocxError, readDocxParts, DocumentImport::readDocxFile, the per-plan journal entries"
provides:
  - "ZipArchive::readEntry: compression-ratio check on produced bytes (more than 1 MiB produced at a ratio above 100 to 1 against the declared compressed size gives Limit)"
  - "ZipArchive::open: entry-count check (more than 1000 entries gives Limit, before any name is read) and duplicate-name check (Corrupt)"
  - "readDocxParts(bytes, unpackedBudget): one budget shared by the three parts, defaulted to kMaxUnpackedBytes"
  - "fixtures: deflate set explicitly in the libzip writer, declared-size option, smallEntries builder"
  - "limits table in src/documents/README.md with the enforced status of each constant"
affects: [phase-02-docx-reader, phase-03-open-in-window]

# Actuals (#2632): chars/4 over the realized diff. Measured: git diff f7c7159 HEAD is 45294 bytes (wc -c), so 45294 / 4 = 11323. This counts bytes, not characters, so it is an upper bound.
actuals:
  tokens: 11323
  tasks: 3
  commits: 4        # MEASURED: git rev-list --count f7c7159..HEAD before this summary commit

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "ratio check after each chunk once more than 1 MiB is produced; the declared compressed size is used only in the comparison (never for allocation or the budget)"
    - "structural checks at open before any entry is read: count first, then each name once"
    - "one budget shared by the parts of one open, decremented by the bytes each read returns"

key-files:
  created: []
  modified:
    - src/documents/ZipArchive.cpp
    - src/documents/DocxContainer.h
    - src/documents/DocxContainer.cpp
    - tests/DocxFixtures.h
    - tests/ZipArchiveTest.cpp
    - tests/DocumentImportTest.cpp
    - src/documents/README.md
    - docs/memory.md

key-decisions:
  - "Ratio check: once total produced is above kCompressionCheckAfterBytes (1 MiB), reject with Limit when total / kMaxCompressionRatio is greater than max(declared compressed size, 1). The declared size comes from zip_stat_index with ZIP_STAT_COMP_SIZE; when it is not known, 1 is used (stricter). A failed stat gives Corrupt."
  - "Entry count: more than kMaxEntries gives Limit at open, before any name is read. Names are then read once each; a repeated name gives Corrupt."
  - "Duplicate names: libzip itself refuses identical names at open (zip_code 10, ZIP_ER_EXISTS, observed in a diagnostic run). The own check in ZipArchive::open is kept as a second line; no test isolates it (see Deviations)."
  - "readDocxParts takes unpackedBudget with the default kMaxUnpackedBytes. The code of a failed open is passed through (Limit or Corrupt), not replaced by Corrupt."
  - "Limits kMaxEntries = 1000, kMaxCompressionRatio = 100, kCompressionCheckAfterBytes = 1 MiB are planner values and stay owner-pending. kMaxUnpackedBytes = 20 MiB as one budget for three parts is the D-02 reading, owner-pending."
  - "Forged declared size: a stored entry whose headers declare 1000 bytes fewer than the data gives Corrupt from zip_fread with no bytes returned (libzip ZIP_ER_CRC, code 7, observed); the expectation was not changed to match."

patterns-established:
  - "Each SAFE-02 control has a test at its boundary: 1000 accepted and 1001 refused; budget 4096 accepted and 4095 refused; shared budget 4096 refused and 5000 accepted"
  - "A refusal leaves no partial bytes (D-07); an entry of the same size stored without compression is not limited by the ratio (ratio 1)"

requirements-completed: [SAFE-02]

# Coverage metadata (#1602)
coverage:
  - id: D1
    description: "A deflated 2 MiB bomb is refused with Limit at the archive level; the same size stored is read byte for byte"
    requirement: "SAFE-02"
    verification:
      - kind: unit
        ref: "tests/ZipArchiveTest.cpp#deflatedBombIsLimitedByRatio, storedLargeEntryIsNotRatioLimited"
        status: pass
    human_judgment: false
  - id: D2
    description: "A bomb file in a Cyrillic directory is refused through DocumentImport::readDocxFile with the Limit text naming only the file"
    requirement: "SAFE-02"
    verification:
      - kind: integration
        ref: "tests/DocumentImportTest.cpp#bombFromCyrillicDirectoryIsLimitWithFileNameOnly"
        status: pass
    human_judgment: false
  - id: D3
    description: "1000 entries accepted and 1001 refused at open; no part is read for the refused archive"
    requirement: "SAFE-02"
    verification:
      - kind: unit
        ref: "tests/ZipArchiveTest.cpp#tooManyEntriesIsLimit, exactlyMaxEntriesIsAccepted"
        status: pass
    human_judgment: false
  - id: D4
    description: "Duplicate entry name gives Corrupt; lookup is exact and case-sensitive (WORD/document.xml and word/document.xml.bak do not match)"
    requirement: "SAFE-02"
    verification:
      - kind: unit
        ref: "tests/ZipArchiveTest.cpp#duplicateEntryNameIsCorrupt, caseAndSuffixVariantsAreNotMatched"
        status: pass
    human_judgment: false
  - id: D5
    description: "Budget counts bytes returned, exact at 4096/4095; shared across word/document.xml and word/styles.xml (4096 refused, 5000 accepted)"
    requirement: "SAFE-02"
    verification:
      - kind: unit
        ref: "tests/ZipArchiveTest.cpp#budgetBoundaryIsExact, sharedBudgetAcrossParts"
        status: pass
    human_judgment: false
  - id: D6
    description: "A forged declared size (1000 bytes smaller in both headers) opens, yields no bytes and gives Corrupt"
    requirement: "SAFE-02"
    verification:
      - kind: unit
        ref: "tests/ZipArchiveTest.cpp#declaredSizeCannotHideOverflow"
        status: pass
    human_judgment: false
  - id: D7
    description: "Owner confirmation of the numeric limits, the MB reading and the D-02 scope"
    requirement: "SAFE-02"
    verification: []
    human_judgment: true
    rationale: "The values are planner choices and the D-02 reinterpretation is owner-pending; automation cannot decide them"

# Metrics
duration: "not measured (start time was not recorded at session start)"
completed: 2026-10-24
---

# Phase 1 Plan 04: Limits SAFE-02 на чтение ZIP и ворота фазы 1

**Коэффициент сжатия по выданным байтам, предел 1000 записей и дубликаты при открытии, общий бюджет 20 MiB на три части: бомба отклоняется с Limit, в том числе из каталога с кириллицей, с текстом, где только имя файла.**

## Performance

- **Duration:** не измерено (время старта не записывалось)
- **Started:** не записано
- **Completed:** 2026-10-24
- **Tasks:** 3 (задача 3 — ворота фазы: сборка, тесты, формат, lint, README, журнал)
- **Files modified:** 8 (см. frontmatter `key-files`)

## Accomplishments

- `readEntry` берёт объявленный размер сжатых данных через `zip_stat_index` и отказывает `Limit`, когда после 1 MiB выдано больше 100 байт на каждый байт сжатых данных. Несжатый элемент того же размера читается полностью.
- `open` проверяет число записей до чтения любого имени (больше 1000 — `Limit`), затем читает каждое имя один раз; повтор — `Corrupt`.
- `readDocxParts` принимает бюджет (по умолчанию `kMaxUnpackedBytes`), который делят три части одного открытия; код отказа открытия передаётся как есть.
- Тестовые фикстуры: явный deflate, опция заниженного объявленного размера, построитель из 1000 записей.
- Таблица лимитов в `src/documents/README.md` с фактическим статусом каждой константы и тестами.
- Журнал фазы 1 в `docs/memory.md` с реальными результатами, открытыми вопросами и отклонениями.

## Task Commits

1. **Task 1: tracer — ratio check and bomb tests (archive and application level)** — `32246c5` (feat)
2. **Task 2: entry count, duplicate names, shared budget, forged size, boundary tests** — `6a2b7f0` (feat)
3. **Task 3: phase gate** — `80309aa` (style: clang-tidy widening in new test lines), `b183aee` (docs: README limits, journal entry)

**Plan metadata:** этот SUMMARY коммитится отдельно после `b183aee`; его хеш указан в ответе исполнителя.

## Files Created/Modified

- `src/documents/ZipArchive.cpp` — проверка коэффициента в `readEntry`; `checkEntryStructure` (число записей и уникальность имён) в `open`.
- `src/documents/DocxContainer.h/.cpp` — параметр `unpackedBudget`, общий остаток бюджета между частями.
- `tests/DocxFixtures.h` — deflate явно; `declaredSizeDelta`; `smallEntries`.
- `tests/ZipArchiveTest.cpp` — 7 новых тестов задачи 2 и 2 теста задачи 1 (всего 24 тестовые функции).
- `tests/DocumentImportTest.cpp` — `bombFromCyrillicDirectoryIsLimitWithFileNameOnly`.
- `src/documents/README.md` — статус лимитов, правила структуры и коэффициента, коды `Limit` и `Corrupt`.
- `docs/memory.md` — итоговая запись фазы 1 (одна, по grep).

## Decisions Made

- Коэффициент: проверка после более 1 MiB выданных байт; отказ, если `total / 100 > max(объявленный размер сжатых данных, 1)`. Без известного размера берётся 1 (строже). Ошибка `zip_stat_index` — `Corrupt`.
- Структура при открытии: сначала число записей, затем имена. Имена читаются через `zip_get_name` с флагами 0 и не печатаются.
- Лимиты `kMaxEntries`, `kMaxCompressionRatio` и `kCompressionCheckAfterBytes` — значения планировщика, ждут подтверждения владельца. «MB» как MiB и охват D-02 — тоже ожидают владельца.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Чистая сборка в существующем каталоге не выполнена**
- **Found during:** Task 3, шаг 1 (`rm -rf build/mingw-debug`)
- **Issue:** Система доступа отклонила `rm -rf` в каталоге сборки; удалять его без разрешения нельзя.
- **Fix:** Чистая сборка выполнена в новых пустых каталогах `build/gate-debug` и `build/gate-release`: `cmake -S . -B ... -G Ninja` с теми же переменными кэша, что в `CMakePresets.json`. Существующий `build/mingw-debug` не менялся; пресеты запущены на нём отдельно (инкрементально).
- **Files modified:** нет.
- **Verification:** см. раздел «Verification»; сборка 47/47 в каждом каталоге, `ctest` 5 из 5.
- **Committed in:** не применимо (команда).

**2. [Rule 2 - Lint] Формат и предупреждения clang-tidy в новых строках тестов**
- **Found during:** Task 1 (`clang-format --dry-run --Werror`) и Task 3 (`clang-tidy`)
- **Issue:** новые строки `QFAIL` не были отформатированы; пять строк давали `bugprone-implicit-widening` в новых тестах.
- **Fix:** `clang-format -i` на двух тестовых файлах (изменились только новые строки); явные `qsizetype{...}` в фикстурах бомбы.
- **Files modified:** `tests/ZipArchiveTest.cpp`, `tests/DocumentImportTest.cpp`
- **Committed in:** `32246c5` (формат), `80309aa` (widening).

**Итого отклонений:** 1 Rule 3 (чистая сборка в другом каталоге), 1 Rule 2 (формат и widening). Поведение и лимиты не менялись.

### Диагностика и мутации (не попали в коммиты)

- Временный `qDebug` в `ZipArchive.cpp` (коды libzip для `declaredSizeCannotHideOverflow` и дубликата) — удалён; проверка `grep` по `DIAG-TEMP` вернула 0 совпадений.
- Временные мутации (отключены проверка записей, дубликатов и коэффициента): после прогона файл восстановлен из резервной копии, `grep MUTANT` вернул 0.
- Мутация проверки дубликатов не провалила `duplicateEntryNameIsCorrupt`: libzip отказывает при повторе имени раньше цикла проверки. Это записано в открытых вопросах, а не «исправлено» тестом.

## Issues Encountered

- Защита worktree отклоняла составные команды с переменными оболочки и с подстановкой `$(git ...)`. Команды разбиты на простые; результаты не меняются.
- Тест `importWritesNothing` сохраняет обходное решение для NTFS (ожидание, пока два снимка каталога совпадут). Это известное обходное решение, не исправление.
- `zip_archive_test.exe` и `document_import_test.exe` запускаются из Git Bash только с `/c/msys64/mingw64/bin` в PATH (libzip.dll); ctest получает PATH из пресета.

## Verification (реальные запуски)

| Команда | Результат |
|---|---|
| `cmake --preset mingw-debug` и `cmake --build --preset debug` до правок (HEAD `f7c7159`) | Generating done; сборка exit 0 |
| `ctest --preset debug` до правок | 100% tests passed, 0 tests failed out of 5 |
| `ctest --preset debug -R zip_archive` (задача 1) | 100% tests passed, 0 failed out of 1 |
| `ctest --preset debug -R document_import` (задача 1) | 100% tests passed, 0 failed out of 1 |
| `zip_archive_test.exe -o <file>,txt` (задача 1) | Totals: 19 passed, 0 failed (включая `deflatedBombIsLimitedByRatio`, `storedLargeEntryIsNotRatioLimited`) |
| `document_import_test.exe -o <file>,txt` (задача 1) | Totals: 17 passed, 0 failed (включая `bombFromCyrillicDirectoryIsLimitWithFileNameOnly`) |
| `zip_archive_test.exe -o <file>,txt` (задача 2) | Totals: 26 passed, 0 failed |
| Мутация: без проверки записей | `tooManyEntriesIsLimit` FAIL |
| Мутация: без проверки коэффициента | `deflatedBombIsLimitedByRatio` FAIL, `bombFromCyrillicDirectoryIsLimitWithFileNameOnly` FAIL |
| Мутация: без проверки дубликатов | `duplicateEntryNameIsCorrupt` PASS (не изолирует проверку, см. ниже) |
| Диагностика libzip (временная) | открытие дубликата: `zip_code=10` (ZIP_ER_EXISTS); заниженный размер: `zip_fread` `zip_code=7` (ZIP_ER_CRC), `sys_code=0` |
| Чистая сборка debug: `cmake -S . -B build/gate-debug` (Debug, Ninja) и `cmake --build build/gate-debug` | exit 0, `[47/47] Linking CXX executable web_api_test.exe` |
| `ctest --test-dir build/gate-debug --output-on-failure` | 100% tests passed, 0 tests failed out of 5 (resume_e2e, resume_json, zip_archive, document_import, web_api) |
| Чистая сборка release: `build/gate-release` (Release, Ninja) и сборка | exit 0, `[47/47]` |
| `ctest --test-dir build/gate-release --output-on-failure` | 100% tests passed, 0 tests failed out of 5 |
| `cmake --build --preset debug` и `ctest --preset debug` (существующее дерево) | exit 0; 100% tests passed, 0 tests failed out of 5 |
| Итог QTest на финальном коде | zip_archive 26 passed, 0 failed; document_import 17 passed, 0 failed |
| `clang-format --dry-run --Werror` (10 файлов фазы) | exit 0 |
| `clang-tidy -p build/mingw-debug` (5 .cpp, `compile_commands.json` есть) | exit 0; до правок 74 предупреждения (см. ниже) |
| `/c/msys64/usr/bin/pacman.exe -Q mingw-w64-x86_64-libzip` | `mingw-w64-x86_64-libzip 1.11.4-1` |
| `grep -c "Фаза 1 (libzip" docs/memory.md` | 1 |

**clang-tidy:** 74 предупреждения до правок: `modernize-use-trailing-return-type` 24, `modernize-use-designated-initializers` 39 (в тестах), `bugprone-implicit-widening-of-multiplication-result` 8, `modernize-use-nodiscard` 1 (`hasEntry`), `performance-enum-size` 2 (`DocxError`, `DocxWarning`). Пять widening были в новых строках и исправлены в `80309aa`. Остальные проверки того же класса, что в 01-01 — 01-03; стиль проекта не решён, NOLINT не добавлялся, код не менялся. Осталось три widening в строках 01-01: `ZipArchive.cpp:14` (`64 * 1024`), `ZipArchiveTest.cpp:22` и `:33`.

**NOT RUN (с причиной):**
- GitHub Actions CI — нет доступа к CI из этой среды; проверяется на PR.
- `cmake --install` и проверка установленной папки — этот план их не менял, проверено в 01-01.
- Inno Setup и чистая машина — ручные проверки (VALIDATION, Manual-Only).
- Release-установка и `web_e2e` — `web_e2e` не зарегистрирован: нет `tests/e2e/node_modules`.
- Мутации `sharedBudgetAcrossParts` и `declaredSizeCannotHideOverflow` — не выполнялись.

## Known Stubs

None.

## Threat Flags

None. Новых сетевых точек, путей доступа и границ доверия относительно плана нет; проверки коэффициента, структуры и бюджета закрывают T-01-12 — T-01-15 из плана.

## VALIDATION status (01-VALIDATION.md)

Файл `01-VALIDATION.md` вне списка `files_modified` плана 01-04, поэтому не редактировался. По строкам карты проверки:

- **01-04-T1** (сжатая бомба → Limit; несжатый — нет): выполнено, тесты зелёные (zip_archive 19/19 на задаче 1; document_import 17/17).
- **01-04-T2** (1000/1001, дубликат, точное совпадение, общий бюджет, заниженный размер): выполнено, `zip_archive` 26/26. Ограничение: дубликат проверяется libzip раньше нашей проверки.
- **01-04-T3** (чистая сборка и ctest в debug и release; формат и lint записаны): выполнено в новых каталогах (см. Deviations 1), 5/5 в обоих; формат проходит; lint записан.

Чек-лист sign-off в `01-VALIDATION.md` (`nyquist_compliant`, CI, ручная установка на чистой машине) не закрыт: CI не запускался, ручные проверки не выполнялись. Статус `draft` и `nyquist_compliant: false` оставлены оркестратору.

## Открытые вопросы и ожидает владельца

- Лимиты `kMaxEntries` = 1000, `kMaxCompressionRatio` = 100 после 1 MiB, `kMaxUnpackedBytes` = 20 MiB на три части — подтверждение владельца.
- Чтение «MB» в D-01 и D-02 как MiB — владелец.
- Охват D-02: один бюджет на `word/document.xml`, `word/styles.xml` и `word/numbering.xml` — владелец.
- Владение источником при неуспешном `zip_open_from_source`: ветка (c), источник не освобождается; не проверено по исходникам libzip.
- Проверка дубликатов в `ZipArchive::open` не изолирована тестом: libzip (`ZIP_ER_EXISTS`) отказывает раньше. Либо оставить как второй рубеж, либо удалить как мёртвый код — решение за владельцем. Вариантов теста, который дошёл бы до нашего цикла, нет.
- Мутационные проверки общего бюджета и заниженного размера не выполнены.
- Русские тексты ошибок (исследование, вопрос 4) — владелец, показ в фазе 3.
- Тексты лицензий LGPL для gnutls и nettle в установщике (вопрос 6) — фаза 3.
- Синтетический зашифрованный DOCX для ручной проверки (вопрос 5) — фаза 3 (TEST-02).
- Стиль выравнивания указателя (`PointerAlignment`) — владелец.
- Версия libzip (вопрос 7) закрыта: `1.11.4-1`.

## Next Phase Readiness

- Фаза 2 может вызывать `DocumentImport::readDocxFile` и читать `ImportedDocx.documentXml`. Лимиты подключены; `kMaxTextChars` (D-03) остаётся фазе 2.
- Фаза 3 получает тексты ошибок и предупреждения; владелец подтверждает лимиты и тексты до показа в окне.
- `.planning/STATE.md` и `ROADMAP.md` не обновлялись: это делает оркестратор, как и в 01-01 — 01-03.
- Ветка `worktree-agent-af05debd4c5e11918` локальная, не отправлена.

## Self-Check

Проверено после записи этого файла, до его коммита:
- FOUND: все файлы из `key-files` и `src/documents/README.md`, `docs/memory.md`, `01-04-SUMMARY.md`.
- FOUND commits (ancestors of HEAD): `32246c5`, `6a2b7f0`, `80309aa`, `b183aee`.
- Рабочее дерево до коммита SUMMARY: только этот файл не отслеживается; `build/` игнорируется git.

Self-Check: PASSED

---
*Phase: 01-zavisimost-libzip-i-bezopasnoe-chtenie-zip*
*Completed: 2026-10-24*
