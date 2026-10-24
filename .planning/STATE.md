---
gsd_state_version: '1.0'
status: planning
progress:
  total_phases: 3
  completed_phases: 0
  total_plans: 0
  completed_plans: 0
  percent: 0
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-10-24)

**Core value:** Открыть свой DOCX-файл резюме в приложении и увидеть его текст (абзацы, заголовки, списки, таблицы) без потери уже введённых данных
**Current focus:** Phase 1 - Зависимость libzip и безопасное чтение ZIP

## Current Position

Phase: 1 of 3 (Зависимость libzip и безопасное чтение ZIP)
Plan: 0 of 0 in current phase
Status: Ready to plan
Last activity: 2026-10-24 - Roadmap created (3 phases, 18/18 v1 requirements mapped)

Progress: [░░░░░░░░░░] 0%

## Performance Metrics

**Velocity:**
- Total plans completed: 0
- Average duration: -
- Total execution time: 0.0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| - | - | - | - |

**Recent Trend:**
- Last 5 plans: -
- Trend: -

*Updated after each plan completion*

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- [Roadmap]: 3 горизонтальные фазы (libzip + ZIP, содержимое DOCX, Qt-окно). Порядок из исследования изменён: зависимость libzip идёт первой, потому что читатель нельзя собрать без неё
- [Roadmap]: Сервис импорта (чтение файла по `QString`-пути, выбор обработчика) входит в Phase 2, чтобы кириллический путь проверялся без UI
- [Roadmap]: Открытие DOCX с несохранёнными правками спрашивает подтверждение (решено пользователем); в Phase 1-2 читатель остаётся чистой функцией без доступа к черновику

### Pending Todos

None yet.

### Blockers/Concerns

- [Phase 1]: libzip не установлен и не проверен (имя пакета `mingw-w64-x86_64-libzip`, pkg-config, транзитивные DLL в `DeployMinGWRuntime.cmake`, списки установки CI). Запасной вариант: zlib/miniz за тем же интерфейсом `ZipArchive`
- [Phase 1]: числовые лимиты (файл 10 или 20 MB, вывод 20 или 64 MB, 200k символов) в исследовании расходятся; выбрать один набор при планировании
- [Phase 2]: поведение DTD/сущностей в `QXmlStreamReader` на Qt 6.11.1 не проверено; названия стилей заголовков в русском Word, `sdt`, объединённые ячейки проверять на фикстурах
- [Phase 2]: реальных образцов резюме нет, нужны синтетические фикстуры (Word RU, LibreOffice, текстовое поле, `sdt`, исправления, повреждённый, обрезанный, зашифрованный, переименованный `.doc`, bomb, XXE); персональные данные не коммитить
- [Phase 3]: нет флага «изменено» и обработчика закрытия; в `selectStage` есть известный баг перезаписи. Уточнить, живёт ли черновик только в памяти UI или также в SQLite

## Deferred Items

Items acknowledged and deferred at milestone close, most recent first:

| Category | Item | Status | Deferred At | Milestone |
|----------|------|--------|-------------|-----------|
| *(none)* | | | | |

## Session Continuity

Last session: 2026-10-24
Stopped at: Roadmap and state created; ready for `/gsd-plan-phase 1`
Resume file: None
