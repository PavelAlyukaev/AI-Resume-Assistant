# AGENTS.md — AI Resume Assistant

## 1. Проект и стек
- Windows desktop-приложение для работы с резюме: импорт PDF/DOCX, локальная анонимизация, AI-интервью, рекомендации профессий, адаптация под вакансии и экспорт.
- Минимум C++23, Qt 6 Widgets, CMake, SQLite/Qt SQL, Qt Test. Нельзя понижать стандарт C++.
- Сейчас реализован Sprint 1: редактирование карьерных этапов и сохранение в SQLite. Остальное — план, не выдавать за готовое.
- Слои: UI → domain/application → storage/LLM/documents; не переносить бизнес-логику в QWidget.

## 2. Команды сборки, запуска и тестирования
- Тулчейн: MSYS2 MinGW-w64 в `C:/msys64/mingw64` (GCC, Qt 6 с Core/Widgets/Sql/Test, CMake, Ninja). Пути заданы в `CMakePresets.json`.
- Configure: `cmake --preset mingw-debug` (release: `mingw-release`). `C:/msys64/mingw64/bin` должен быть в PATH или запускать из оболочки MSYS2 MINGW64.
- Build: `cmake --build --preset debug`.
- Test all: `ctest --preset debug`.
- One test: `ctest --preset debug -R resume_e2e`.
- Run: `build/mingw-debug/AIResumeAssistant.exe` (Qt DLL берутся из `C:/msys64/mingw64/bin` в PATH).
- Format check: `clang-format --dry-run --Werror <изменённые .cpp/.h>`.
- Linter: `clang-tidy <файл> -p build/mingw-debug` при наличии корректной compilation database; иначе указать причину пропуска.
- Никогда не утверждать, что тесты прошли, если они не были запущены.

## 3. Конвенции
- Классы PascalCase, методы camelCase, RAII, владение QObject через parent или контролируемое владение.
- Все SQL-запросы с пользовательскими данными параметризовать.
- UI не блокировать сетевыми запросами. Ошибки отображать без секретов и приватных данных.
- Для новых возможностей писать тесты и обновлять документацию.
- Хранить факты кандидата отдельно от AI-предложений; не придумывать достижения.

## 4. Правила работы агента
- Перед задачей читать `docs/plan.md` и `docs/memory.md`, затем затрагиваемые файлы.
- Не читать, не менять и не выводить содержимое `.env` и секретов; не обходить защитные hooks.
- Не коммитить ключи API, исходные резюме и персональные данные.
- В `src/llm/**` дополнительно соблюдать `.claude/rules/llm-security.md`.
- Изменять только относящиеся к задаче файлы; избегать разрушительных git-команд.
- Порядок работы: код и тесты на ПК разработчика в локальном MSYS2 (`C:/msys64/mingw64`); перед коммитом `cmake --build --preset debug` и `ctest --preset debug`; коммит в отдельную ветку, в `main` только через PR. Каталоги сборки (`build/`, `cmake-build-*/`) не коммитить.
- После значимых задач обновлять `docs/memory.md` с результатами и нерешёнными вопросами.
- В отчёте указывать изменённые файлы, реальные команды проверок, их результаты и ограничения.
- Навыки: `/write-tests` — написание тестов; `/review-changes` — ревью изменений.
