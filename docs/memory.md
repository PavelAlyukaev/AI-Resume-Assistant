# Память проекта для Claude Code

## Текущее состояние
- Sprint 1: C++23, Qt6 Widgets, SQLite, Qt Test. Сборка и E2E проходят локально в MSYS2 MinGW (debug и release), установщик Inno Setup собирается и проходит smoke-тест.
- Разработка ведётся на ПК владельца, в `main` изменения попадают через PR; GitHub Actions CI дублирует сборку на `windows-latest`.
- Поведение Claude Code (правило, навыки, hooks, память) ещё не проверено, скриншотов нет.
- Импорт PDF/DOCX, облачные LLM, анонимизация и экспорт пока не реализованы.

## Архитектурные решения
- Один модульный десктопный процесс, без сервера.
- Тулчейн: MSYS2 MinGW-w64 (GCC + Qt 6 из `C:/msys64/mingw64`), генератор Ninja; MSVC не используется.
- `ResumeRepository` владеет именованным Qt SQL соединением; интерфейс использует репозиторий.
- Персональные данные не отправлять в API до локальной анонимизации и подтверждения пользователя.
- C++23 обязателен; Qt Widgets для UI; SQLite для хранения.

## Следующие шаги
1. Провести демонстрации из `docs/verification.md`, приложить реальные скриншоты.
2. Улучшить выбор резюме (вместо открытия по ID).
3. Начать Sprint 2: импорт DOCX и текстовых PDF.

## Журнал
- 2026-10-11: Создан каркас репозитория и учебная конфигурация Claude Code. Проверки в Claude Code не проводились.
- 2026-10-11: Сборка переведена с MSVC на MSYS2 MinGW (пресеты `mingw-debug`/`mingw-release`, Ninja). Сборка ещё не запускалась.
- 2026-10-11: Добавлен GitHub Actions CI (`.github/workflows/ci.yml`): windows-latest + MSYS2 MINGW64, пресеты mingw-debug и mingw-release, configure/build/ctest на push и PR в main.
- 2026-10-11: Добавлен установщик Inno Setup (`installer/AIResumeAssistant.iss`, CMake-цель `installer`, деплой через windeployqt + `cmake/DeployMinGWRuntime.cmake`) и CI-джоб, который собирает, тихо устанавливает и запускает приложение, выкладывая setup.exe артефактом.
- 2026-10-12: Порядок работы: код пишется и тесты прогоняются на ПК владельца в локальном MSYS2 MinGW (`C:/msys64/mingw64`), затем commit и PR на GitHub. Каталоги сборки (`/build/`, `/cmake-build-*/` из CLion) и `CMakeUserPresets.json` в `.gitignore`, не коммитятся.
- 2026-10-12: Описания синхронизированы с фактическим состоянием (README, AGENTS.md, plan, memory, about). Проверено на `b77c582`: `cmake --preset mingw-debug`, `cmake --build --preset debug`, `ctest --preset debug` — 1/1 пройден; ранее в тот же день на `468230a` прошли release-сборка, `ctest --preset release` 1/1 и smoke-тест установщика. Права агента в `.claude/settings.json` (allow для git/cmake/ctest, ask для `git push`) и игнор `.claude/settings.local.json` добавлены владельцем.
