# AI Resume Assistant

Windows desktop-приложение для улучшения резюме с помощью ИИ. **Текущий статус: Sprint 1 — минимальный редактор и SQLite; AI-функции ещё не реализованы.**

## Цель
В перспективе: импорт PDF/DOCX, локальная анонимизация, уточняющее AI-интервью, рекомендации профессий, сравнение с вакансией и экспорт адаптированного резюме. План: [docs/plan.md](docs/plan.md).

## Стек
C++23 (минимум), Qt 6 Widgets, Qt SQL/SQLite, Qt Test, CMake; целевая ОС — Windows 10/11 x64. Сборка через MSYS2 MinGW-w64 (`C:/msys64/mingw64`): GCC, Qt 6, CMake и Ninja из этого окружения.

## Сборка и запуск
Установить MSYS2 и пакеты окружения MINGW64:

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-qt6-base
```

Из корня проекта (PowerShell, `C:\msys64\mingw64\bin` в PATH, или оболочка MSYS2 MINGW64):

```powershell
cmake --preset mingw-debug
cmake --build --preset debug
ctest --preset debug
.\build\mingw-debug\AIResumeAssistant.exe
```

Пути к компилятору и Qt заданы в `CMakePresets.json`; при другом расположении MSYS2 их нужно поправить. CLion подхватывает пресеты `mingw-debug`/`mingw-release` автоматически. Для запуска вне PATH с `C:\msys64\mingw64\bin` Qt DLL нужно развернуть через `windeployqt6`. Для SQLite нужен доступный Qt SQL драйвер QSQLITE.

## Инсталлятор
Установщик собирается [Inno Setup 6](https://jrsoftware.org/isdl.php) из Release-сборки. Скрипт: `installer/AIResumeAssistant.iss`.

```powershell
cmake --preset mingw-release
cmake --build --preset release --target installer
```

Цель `installer` появляется, только если CMake нашёл `ISCC.exe` (стандартные пути установки Inno Setup 6 или `-DISCC_EXECUTABLE=...`). Она выполняет `cmake --install` в `build/mingw-release/deploy` (windeployqt копирует Qt DLL и плагины, `cmake/DeployMinGWRuntime.cmake` докладывает DLL MinGW/MSYS2 и оставляет только SQL-драйвер QSQLITE), затем собирает `build/mingw-release/installer/AIResumeAssistant-<версия>-setup.exe`. База резюме хранится в `%APPDATA%` и при удалении программы не удаляется.

## CI
GitHub Actions (`.github/workflows/ci.yml`) на каждый push и PR в `main` собирает проект на `windows-latest` в MSYS2 MINGW64 с теми же пресетами (`mingw-debug` и `mingw-release`) и запускает `ctest`. Джоб `Inno Setup installer` собирает установщик, тихо ставит его и проверяет, что приложение стартует без MSYS2 в PATH; готовый `setup.exe` доступен в артефакте `AIResumeAssistant-setup` запуска.

## Уже реализовано
- Окно Qt Widgets для названия резюме и списка карьерных этапов.
- Создание и изменение этапов: должность, компания, период, описание.
- Сохранение в SQLite и открытие резюме по ID.
- Сквозной Qt Test: создание → сохранение → повторное открытие → обновление.

## Архитектура
- `src/domain/` — сущности `Resume`, `CareerStage`.
- `src/storage/` — репозиторий SQLite.
- `src/ui/` — интерфейс Qt Widgets.
- `tests/` — тесты Qt Test.
- `docs/` — план, память, отчёт и сценарии проверки.

## Как агент работает в проекте
Основной агент — **Claude Code**. При старте читает `CLAUDE.md`, который импортирует корневой `AGENTS.md`. Корневой файл описывает стек, команды, конвенции и ограничения. `docs/plan.md` и `docs/memory.md` фиксируют план и состояние. Правило `.claude/rules/llm-security.md` ограничено путями `src/llm/**`.

Агент может читать и изменять исходники по задаче, запускать сборку и тесты, но не должен читать `.env`, публиковать секреты, выполнять разрушительные операции без разрешения или выдавать неподтверждённые тесты за успешные. `PreToolUse` hook блокирует некоторые попытки доступа к `.env`; `PostToolUse` hook проверяет форматирование C++ при наличии clang-format. Это дополнительные меры, а не гарантия безопасности.

Навыки: `/write-tests` — создание тестов; `/review-changes` — ручное ревью изменений. Тесты запускаются через `ctest --preset debug`, отдельный E2E — через `ctest --preset debug -R resume_e2e`. Детали демонстрации: [docs/verification.md](docs/verification.md), отчёт: [docs/about.md](docs/about.md).

## Приватность
В будущих версиях персональные данные должны анонимизироваться локально до отправки в LLM API; подтверждённые факты отделяются от предложений модели. Никогда не хранить реальные API-ключи в репозитории.

## Статус проверок
Исходники и конфигурации подготовлены, но сборка на Windows, выполнение E2E и поведение Claude Code **пока не проверены**. Результаты необходимо заполнить в `docs/about.md` после реальных запусков.
