# Phase 1: Зависимость libzip и безопасное чтение ZIP - Context

**Gathered:** 2026-10-24
**Status:** Ready for planning

<domain>
## Phase Boundary

Фаза даёт безопасный ZIP-контейнер для DOCX: `.docx` открывается как архив с лимитами, а libzip собирается на ПК разработчика, в CI и в развёрнутой папке приложения. Интерфейса и разбора содержимого документа в этой фазе нет (это фазы 2 и 3).

</domain>

<decisions>
## Implementation Decisions

### Лимиты размера
- **D-01:** Максимальный размер входного `.docx` — 10 MB. Файл больше лимита отклоняется до разбора, в ошибке указывается лимит.
- **D-02:** Лимит распаковки — 20 MB суммарно для распакованных данных `word/document.xml`. Сверх лимита чтение останавливается и файл отклоняется (защита от zip-bomb).
- **D-03:** Лимит длины текста резюме — 200 000 символов. Превышение — отказ с сообщением о лимите (проверяется на фазе 2, но значение фиксируем здесь).

### Тексты ошибок
- **D-04:** Тексты ошибок на русском языке. Формат: кратко — что не так, и подсказка что сделать (например, «Файл зашифрован. Сохраните резюме без пароля и повторите»).
- **D-05:** В тексте ошибки показывается только имя файла, без полного пути (защита приватности, см. SAFE-05).
- **D-06:** Отдельные тексты для старого `.doc` и для зашифрованного DOCX. Остальные ошибки (не ZIP, повреждённый архив, лимит) — каждая со своим текстом.

### Частичные данные
- **D-07:** Если архив целый, но `word/document.xml` повреждён — отказ целиком. Частичный результат не возвращается, чтобы не получить неполное резюме.
- **D-08:** Пропущенные конструкции (то, что не читается, но не делает файл битым) — это предупреждения, а не ошибки. Предупреждения возвращаются списком со счётчиком.
- **D-09:** На фазе 1 предупреждения возвращаются в результате чтения; показ в интерфейсе — фаза 3.
- **D-10:** Содержимое вне v1 (текстовые поля, колонтитулы и подобное, если попадается) не читается и ничего не записывается на диск.

### Claude's Discretion
- Способ подключения libzip (пакет MSYS2 или запасной вариант zlib/miniz за интерфейсом `ZipArchive`) — решается исследованием и пробной сборкой.
- Внутренняя структура `ZipArchive` и имена классов в `src/documents/`.
- Порядок внесения libzip в списки установки CI и `DeployMinGWRuntime.cmake`.
- Конкретные имена тестовых фикстур (синтетические файлы).

### Folded Todos
Не применимо: todo по фазе не найдено.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Требования и роадмап
- `.planning/REQUIREMENTS.md` — SAFE-01, SAFE-02 (владелец фазы 1)
- `.planning/ROADMAP.md` — Фаза 1, критерии успеха
- `.planning/PROJECT.md` — ключевые решения и ограничения

### Исследования
- `.planning/research/SUMMARY.md` — общий итог исследования и открытые вопросы
- `.planning/research/STACK.md` — libzip (MSYS2), `QXmlStreamReader`, решение против `QZipReader`
- `.planning/research/PITFALLS.md` — zip-bomb, XXE, кодировка, приватность, сборка и упаковка
- `.planning/research/ARCHITECTURE.md` — слой `src/documents/`, `ZipArchive`, граница с приложением
- `.planning/research/FEATURES.md` — типы документов и ошибки, которые должен распознавать читатель

### Архитектура и правила проекта
- `AGENTS.md` — слои (UI → domain/application → storage/documents), запрет логики в QWidget, приватность, тесты
- `.planning/codebase/ARCHITECTURE.md` и `.planning/codebase/STACK.md` — текущая структура и зависимости
- `src/documents/README.md` — заглушка слоя документов
- `docs/memory.md` — журнал решений проекта

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/documents/README.md` — место для нового слоя; кода ZIP в репозитории нет.
- `src/application/ResumeJson.*` — образец выноса проверок в слой приложения (для согласования стиля ошибок).
- `tests/` и `CMakeLists.txt` — существующие цели Qt Test (`resume_e2e_test`, `resume_json_test`, `web_api_test`) и шаблон добавления новой цели.

### Established Patterns
- Слои: логика разбора не внутри QWidget (`AGENTS.md`); ошибки без приватных данных.
- Сборка через пресеты `mingw-debug` / `mingw-release` и Ninja; зависимости — через `find_package` / pkg-config из MSYS2.
- Синтетические тестовые данные; реальные резюме не коммитятся.

### Integration Points
- `CMakeLists.txt` — новая статическая библиотека `resume_documents` и подключение libzip.
- `cmake/DeployMinGWRuntime.cmake` и `installer/AIResumeAssistant.iss` — деплой DLL libzip.
- `.github/workflows/ci.yml` — список пакетов MSYS2 для CI.

</code_context>

<specifics>
## Specific Ideas

- Тексты ошибок пользователь хочет короткими и с подсказкой, на русском.
- Лимиты выбраны из исследования (10 MB вход, 20 MB распаковка, 200k символов), пользователь подтвердил их.

No specific references beyond the decisions above.

</specifics>

<deferred>
## Deferred Ideas

- Текстовые поля и колонтитулы как полноценное содержимое — v2 (DOCX-V2-03). В фазе 1 такие части не читаются.
- Полный путь к файлу в ошибке — отклонено (D-05), при необходимости вернуть в настройках позже.
- Старый формат `.doc` и DOCX с паролем — v2 (DOCX-V2-01, DOCX-V2-02). Здесь только распознавание и отдельный текст ошибки.
- Исправление `selectStage` (перезапись несохранённых правок) — не относится к фазе 1; отслеживать в фазе 3.

### Reviewed Todos (not folded)
Не применимо.

</deferred>

---

*Phase: 01-zavisimost-libzip-i-bezopasnoe-chtenie-zip*
*Context gathered: 2026-10-24*
