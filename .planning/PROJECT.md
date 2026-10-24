# AI Resume Assistant

## What This Is

Windows-приложение на C++23 / Qt 6 для работы с резюме. Сейчас работают редактирование карьерных этапов и сохранение в SQLite, а также локальная веб-версия UI на `127.0.0.1`. Ближайшая цель — чтение существующих резюме из DOCX, чтобы пользователь мог открыть свой файл в приложении вместо ручного ввода.

## Core Value

Открыть свой DOCX-файл резюме в приложении и увидеть его текст (абзацы, заголовки, списки, таблицы) без потери уже введённых данных — это условие для всех следующих этапов.

## Requirements

### Validated

- ✓ Создание и редактирование карьерного этапа в Qt Widgets UI — Sprint 1
- ✓ Сохранение и открытие резюме в SQLite по ID (`ResumeRepository`, QSQLITE) — Sprint 1
- ✓ Локальный веб-UI на `127.0.0.1` с JSON API (`src/web/`) — Sprint 1
- ✓ Qt Test сквозной тест слоя данных (`resume_e2e`) и тесты веб-API (`web_api`, `resume_json`) — Sprint 1

### Active

- [ ] Открытие `.docx` в Qt-окне: чтение абзацев, заголовков, списков и таблиц в текст окна
- [ ] При открытии DOCX с несохранёнными правками приложение спрашивает подтверждение перед заменой
- [ ] Изображения из DOCX не читаются и не сохраняются на этом этапе
- [ ] Автоматический Qt Test на тестовом DOCX с ожидаемым текстом
- [ ] Ручной сценарий в окне: открыть DOCX и проверить отображаемый текст

### Out of Scope

- Импорт текстовых PDF — отдельный этап Sprint 2, чтобы не смешивать парсеры
- Импорт изображений и сохранение вложений — не нужно для текста резюме на первом этапе
- Анонимизация и отправка данных в API — Sprint 2–3, после чтения документов
- LLM-провайдеры и AI-интервью — Sprint 3
- Анализ профессий, вакансий, экспорт DOCX/PDF — Sprint 4
- Веб-интерфейс для импорта DOCX — пока только Qt-окно

## Context

- Текущая архитектура: `src/ui/MainWindow.*` (Qt Widgets), `src/web/` (HTTP), `src/application/ResumeJson.*`, `src/domain/Resume.h`, `src/storage/ResumeRepository.*`. Документного слоя `src/documents/` ещё нет.
- Карта кодовой базы: `.planning/codebase/` (от 2026-10-24, коммит `21afce5`).
- Sprint-план: `docs/plan.md`. Память проекта: `docs/memory.md`.
- Не выдумывать достижения: факты кандидата хранятся отдельно от AI-предложений (`AGENTS.md`).
- Пользователь пока не уточнил формат тестового DOCX и поведение при ошибках чтения повреждённого файла.

## Constraints

- **Tech stack**: C++23 (нельзя понижать), Qt 6 Widgets, Qt SQL/SQLite, CMake, Qt Test — из `AGENTS.md`.
- **Platform**: Windows x64, MSYS2 MinGW-w64 (`C:/msys64/mingw64`) — текущая среда разработки.
- **Layers**: логика импорта не должна жить в QWidget; разбор DOCX — в отдельном слое документов.
- **Privacy**: тестовые резюме и персональные данные не коммитятся; локальный веб-сервер только на loopback.
- **Verification**: перед коммитом `cmake --build --preset debug` и `ctest --preset debug`; не утверждать, что тесты прошли, без запуска.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Первый этап — только чтение DOCX | Самая маленькая полезная часть Sprint 2 | — Pending |
| Чтение: абзацы, заголовки, списки, таблицы | Нужная структура для резюме | — Pending |
| Изображения игнорируются | Не нужны для текста, усложняют парсер | — Pending |
| Открытие DOCX при несохранённых правках спрашивает подтверждение перед заменой | Защита от потери правок (см. PITFALLS: баг `selectStage`) | — Pending |
| Готовность: открыть файл в окне + автотест на тестовом DOCX | Проверяемый критерий и ручная проверка | — Pending |

---
*Last updated: 2026-10-24 after initialization*

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state
