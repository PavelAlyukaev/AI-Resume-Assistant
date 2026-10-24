---
phase: "01"
slug: "zavisimost-libzip-i-bezopasnoe-chtenie-zip"
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: false) (#2117)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-10-24"
---

# Phase 01 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Qt Test (`Qt6::Test`) |
| **Config file** | `CMakeLists.txt` (новая цель `zip_archive_test`) |
| **Quick run command** | `ctest --preset debug -R zip_archive` |
| **Full suite command** | `cmake --build --preset debug && ctest --preset debug` |
| **Estimated runtime** | ~60 seconds (оценка, не измерено) |

---

## Sampling Rate

- **After every task commit:** Run `ctest --preset debug -R zip_archive` (после сборки)
- **After every plan wave:** Run `cmake --build --preset debug && ctest --preset debug`
- **Before `/gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 120 seconds

---

## Per-Task Verification Map

Заполняется после создания PLAN.md (ID задач появятся там). Результаты записываются только после реального запуска; до тех пор — NOT RUN.

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 01-01-T1 | 01 | 1 | SAFE-01, SAFE-02 | — | Чекпоинт: проверка пакета и установка libzip (код не меняется) | checkpoint | `pacman -Q mingw-w64-x86_64-libzip` | n/a | ⬜ pending |
| 01-01-T2 | 01 | 1 | SAFE-02 | T-01-02, T-01-03, T-01-04 | Побайтовое чтение word/document.xml из памяти; бюджет на границе; открытие только из буфера; усечённый ZIP → Corrupt | unit | `ctest --preset debug -R zip_archive` | ❌ W0 (создаётся задачей) | ⬜ pending |
| 01-01-T3 | 01 | 1 | SC-1, SC-2 | T-01-01, T-01-SC | libzip в обоих блоках CI; libzip.dll и его DLL в установке; приложение стартует без MSYS2 в PATH | build / smoke | `cmake --install build/mingw-debug --prefix build/mingw-debug/install-check` + проверка зависимостей + запуск | ❌ CI / прогон установки | ⬜ pending |
| 01-02-T1 | 02 | 2 | SAFE-01 | T-01-05 | Текст без ZIP-сигнатуры → NotZip; три части читаются побайтово; отсутствующие части → предупреждения | unit | `ctest --preset debug -R zip_archive` | ❌ W0 | ⬜ pending |
| 01-02-T2 | 02 | 2 | SAFE-01 | T-01-05, T-01-06 | CFB с EncryptedPackage → Encrypted; без → LegacyDoc; PK без целого ZIP → Corrupt; без word/document.xml → MissingPart; запись libzip с паролем → Encrypted | unit | `ctest --preset debug -R zip_archive` | ❌ W0 | ⬜ pending |
| 01-02-T3 | 02 | 2 | SAFE-01 | T-01-07, T-01-08 | Пустой ввод → NotZip; ввод больше 10 MiB → TooLarge; повреждённая часть → ошибка без частичного результата | unit | `ctest --preset debug -R zip_archive` | ❌ W0 | ⬜ pending |
| 01-03-T1 | 03 | 3 | SAFE-01 | T-01-09, T-01-10 | Чтение DOCX из каталога с кириллицей; NotZip-текст содержит только имя файла | integration | `ctest --preset debug -R "zip_archive\|document_import"` | ❌ W0 | ⬜ pending |
| 01-03-T2 | 03 | 3 | SAFE-01 | T-01-10 | Точный русский текст для каждого кода и предупреждения; тексты попарно различны | unit | `ctest --preset debug -R "zip_archive\|document_import"` | ❌ W0 | ⬜ pending |
| 01-03-T3 | 03 | 3 | SAFE-01 | T-01-09, T-01-10, T-01-11 | Граница 10 MiB до открытия; снимок каталога до и после не меняется; маркер и путь не попадают в сообщения | integration | `ctest --preset debug -R "zip_archive\|document_import"` | ❌ W0 | ⬜ pending |
| 01-04-T1 | 04 | 4 | SAFE-02 | T-01-12 | Сжатая бомба → Limit (архив и приложение); несжатый большой элемент коэффициентом не ограничивается | unit + integration | `ctest --preset debug -R "zip_archive\|document_import"` | ❌ W0 | ⬜ pending |
| 01-04-T2 | 04 | 4 | SAFE-02 | T-01-13, T-01-14, T-01-15 | 1000 записей принимаются, 1001 → Limit; дубликат → Corrupt; точное совпадение имён; общий бюджет; заниженный размер не скрывает переполнение | unit | `ctest --preset debug -R zip_archive` | ❌ W0 | ⬜ pending |
| 01-04-T3 | 04 | 4 | SC-1 | — | Чистая сборка и ctest в debug и release; формат и lint записаны как есть | build / CI | `ctest --preset debug` после чистой сборки | ❌ CI | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/ZipArchiveTest.cpp` — покрывает SAFE-01/02 (см. RESEARCH.md, раздел Phase Requirements → Test Map)
- [ ] Цель `zip_archive_test` в `CMakeLists.txt` с `add_test(NAME zip_archive COMMAND zip_archive_test)` и `Qt6::Test`
- [ ] Вспомогательный генератор фикстур в тесте (ZIP через libzip, CFB байтами) — без бинарных файлов в git
- [ ] Строка `mingw-w64-x86_64-libzip` в оба блока `install:` в `.github/workflows/ci.yml`
- [ ] Установка пакета в MSYS2 и пробная сборка (см. RESEARCH.md, Environment Availability)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Установленная папка запускается без MSYS2 в PATH (`libzip.dll` резолвится) | SC-2 | Требует установленного инсталлятора на чистой машине | Установить через `installer/AIResumeAssistant.iss`, запустить `AIResumeAssistant.exe` вне MSYS2 |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 120s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
