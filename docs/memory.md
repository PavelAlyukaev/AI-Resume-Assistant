# Память проекта для Claude Code

## Текущее состояние
- Sprint 1: C++23, Qt6 Widgets, SQLite, Qt Test. Сборка и E2E проходят локально в MSYS2 MinGW (debug и release), установщик Inno Setup собирается и проходит smoke-тест.
- Разработка ведётся на ПК владельца, в `main` изменения попадают через PR; GitHub Actions CI дублирует сборку на `windows-latest`.
- Поведение Claude Code (правило, навыки, hooks, память) ещё не проверено, скриншотов нет.
- Веб-версия UI Sprint 1 реализована в коде и покрыта тестами (`resume_json`, `web_api`); HTML-страница в браузере глазами не проверялась.
- Импорт PDF/DOCX, облачные LLM, анонимизация и экспорт пока не реализованы.

## Архитектурные решения
- Один модульный десктопный процесс. Внутри него — локальный HTTP-сервер (Qt HttpServer) только на `127.0.0.1` для веб-версии UI; внешнего/сетевого доступа нет (решение владельца 2026-10-13). Qt и веб используют один `ResumeRepository`; JSON-валидация в `src/application/ResumeJson`, HTTP в `src/web/WebServer`, страница `src/web/index.html` встроена как Qt-ресурс.
- Минимальная версия Qt поднята до 6.7 (QHttpHeaders в QHttpServerResponse); в CI добавлен пакет `mingw-w64-x86_64-qt6-httpserver`.
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
- 2026-10-13: Ветка `feature/web-ui`: добавлены `src/application/ResumeJson.*`, `src/web/WebServer.*`, `src/web/index.html`, интеграция в `MainWindow` (меню «Открыть веб-версию»), тесты `tests/ResumeJsonTest.cpp`, `tests/WebApiTest.cpp`, пакет httpserver в CI. Локально: `cmake --build --preset debug` и `ctest --preset debug` — 3/3; то же в release (3/3). Реальное приложение отвечало на GET `/` (200), `/api/resumes/99999` (404), чужой Host (403), порт слушал только 127.0.0.1. `cmake --install` разворачивает Qt6HttpServer/Network/WebSockets DLL. НЕ ПРОВЕРЕНО: GitHub Actions CI, установщик Inno Setup (smoke), визуальная работа страницы в браузере. Нерешённое: `clang-format --Werror` падает и на старых файлах (в коде `QString* error`, в `.clang-format` по умолчанию LLVM-выравнивание справа); новые файлы отформатированы с `PointerAlignment: Left`.
- 2026-10-24: GSD-инициализация (`.planning/`): PROJECT.md, config.json (yolo, standard, Budget, research и проверки включены), 4 исследования, SUMMARY.md, REQUIREMENTS.md (18 v1-требований), ROADMAP.md с 3 фазами (libzip + безопасное ZIP; чтение содержимого DOCX; открытие в окне Qt). Структура Horizontal Layers. Решение владельца: при открытии DOCX с несохранёнными правками спрашивать перед заменой. Найден баг `selectStage` (перезапись несохранённых правок при переключении этапа) — не исправлен. Не проверено: установка libzip, сборка и тесты (ничего не запускалось). Ветка с изменениями — `chore/map-codebase`, не смёржена.
- 2026-10-13: Добавлен браузерный e2e `web_e2e` (`tests/e2e/web_e2e.mjs` + `tests/e2e/WebE2EServer.cpp`, playwright-core и установленный Chrome; нужен `npm install` в `tests/e2e`, иначе тест пропускается с сообщением CMake). Сценарий: создать этап, сохранить, перезагрузить, открыть по ID, сверить поля и API, скриншот `screenshots/web-e2e-sprint1.png`. Локально `cmake --build --preset debug` и `ctest --preset debug` — 4/4. Первый прогон падал: CSP страницы запрещает eval, `waitForFunction` заменён на локатор. НЕ ПРОВЕРЕНО: release-сборка с этим тестом, CI (в нём нет Node/Chrome-шага, тест там будет пропущен).
- 2026-10-24: Планирование фазы 1 (`.planning/phases/01-zavisimost-libzip-i-bezopasnoe-chtenie-zip/`): 4 плана (01-01…01-04), последовательные волны 1–4, потому что все четыре меняют общие CMake-цели и `src/documents/`. Решения планировщика: блокирующая контрольная точка установки libzip стоит перед трассером (это шлюз, не рабочая задача); `cmake/DeployMinGWRuntime.cmake` всё же придётся менять (прогноз исследования «без изменений» не подтверждён: исполняемый файл не импортирует `libzip.dll`, поэтому его надо ставить и сканировать явно, проверка через `objdump`); `std::expected` в новом слое; лимиты 1000 записей, коэффициент 100:1 после 1 MiB и общий бюджет 20 MiB выбраны планировщиком и ждут подтверждения владельца. Тесты не запускались; реальные результаты появятся в плане 01-04.
