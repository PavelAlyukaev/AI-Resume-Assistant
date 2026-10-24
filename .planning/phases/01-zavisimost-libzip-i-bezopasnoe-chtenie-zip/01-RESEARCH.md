# Phase 1: Зависимость libzip и безопасное чтение ZIP - Research

**Researched:** 2026-10-24
**Domain:** C++23 / Qt 6 (MinGW, MSYS2) — ZIP-контейнер DOCX на libzip, лимиты распаковки, понятные ошибки
**Confidence:** MEDIUM — пакет и его состав подтверждены официальной страницей MSYS2; имена функций libzip подтверждены частично (сигнатуры из документации); точные имена enum-значений и CMake-целей требуют проверки по установленным `zip.h` и `libzip-targets.cmake` (шаг пробной сборки).

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Лимиты размера**
- **D-01:** Максимальный размер входного `.docx` — 10 MB. Файл больше лимита отклоняется до разбора, в ошибке указывается лимит.
- **D-02:** Лимит распаковки — 20 MB суммарно для распакованных данных `word/document.xml`. Сверх лимита чтение останавливается и файл отклоняется (защита от zip-bomb).
- **D-03:** Лимит длины текста резюме — 200 000 символов. Превышение — отказ с сообщением о лимите (проверяется на фазе 2, но значение фиксируем здесь).

**Тексты ошибок**
- **D-04:** Тексты ошибок на русском языке. Формат: кратко — что не так, и подсказка что сделать (например, «Файл зашифрован. Сохраните резюме без пароля и повторите»).
- **D-05:** В тексте ошибки показывается только имя файла, без полного пути (защита приватности, см. SAFE-05).
- **D-06:** Отдельные тексты для старого `.doc` и для зашифрованного DOCX. Остальные ошибки (не ZIP, повреждённый архив, лимит) — каждая со своим текстом.

**Частичные данные**
- **D-07:** Если архив целый, но `word/document.xml` повреждён — отказ целиком. Частичный результат не возвращается, чтобы не получить неполное резюме.
- **D-08:** Пропущенные конструкции (то, что не читается, но не делает файл битым) — это предупреждения, а не ошибки. Предупреждения возвращаются списком со счётчиком.
- **D-09:** На фазе 1 предупреждения возвращаются в результате чтения; показ в интерфейсе — фаза 3.
- **D-10:** Содержимое вне v1 (текстовые поля, колонтитулы и подобное, если попадается) не читается и ничего не записывается на диск.

### Claude's Discretion
- Способ подключения libzip (пакет MSYS2 или запасной вариант zlib/miniz за интерфейсом `ZipArchive`) — решается исследованием и пробной сборкой.
- Внутренняя структура `ZipArchive` и имена классов в `src/documents/`.
- Порядок внесения libzip в списки установки CI и `DeployMinGWRuntime.cmake`.
- Конкретные имена тестовых фикстур (синтетические файлы).

### Deferred Ideas (OUT OF SCOPE)
- Текстовые поля и колонтитулы как полноценное содержимое — v2 (DOCX-V2-03). В фазе 1 такие части не читаются.
- Полный путь к файлу в ошибке — отклонено (D-05), при необходимости вернуть в настройках позже.
- Старый формат `.doc` и DOCX с паролем — v2 (DOCX-V2-01, DOCX-V2-02). Здесь только распознавание и отдельный текст ошибки.
- Исправление `selectStage` (перезапись несохранённых правок) — не относится к фазе 1; отслеживать в фазе 3.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| SAFE-01 | Не-ZIP, повреждённый, зашифрованный файл и старый `.doc` дают понятную ошибку, черновик не меняется | Порядок распознавания: магические байты (ZIP `PK`, OLE2/CFB) до libzip; зашифрованный DOCX — это OLE2-контейнер, а не ZIP (см. Architecture Patterns, Pitfall 4). «Черновик не меняется» — чистая функция чтения без доступа к черновику (STATE.md, решение Phase 1-2). |
| SAFE-02 | Защита от zip-bomb через лимиты распаковки | libzip даёт размер только по заголовкам; реальный объём считается по `zip_fread` с running budget (Code Examples). Лимиты: D-01 (10 MB вход), D-02 (20 MB распаковка); количество записей и коэффициент сжатия — не зафиксированы в CONTEXT, см. Open Questions. |

Phase success criteria (ROADMAP) кроме SAFE-01/02 проверяются сборкой и CI: проект собирается с нуля, `cmake --install` даёт запускаемую папку без отсутствующих DLL (CI smoke-test уже запускает установленное приложение с PATH без MSYS2).
</phase_requirements>

## Summary

Фаза добавляет в проект новый слой `src/documents/` с классом `ZipArchive` поверх libzip и подключает libzip в сборку, CI и деплой. Пакет MSYS2 `mingw-w64-x86_64-libzip` (версия **1.12.1-1**, BSD-3-Clause) доступен в официальном репозитории и ставит `libzip.dll`, `zip.h`, `libzip.pc` и CMake-конфигурацию `lib/cmake/libzip/libzip-config.cmake` [CITED: packages.msys2.org/package/mingw-w64-x86_64-libzip]. В STACK.md и ARCHITECTURE.md указана версия 1.11.4 — это устарело, принимаем 1.12.1 и сверяем по факту установки. Локально libzip **не установлен** (нет `zip.h`, `libzip.pc`, `libzip.dll.a`) [VERIFIED: probe `C:/msys64/mingw64/include`, `lib`, `lib/pkgconfig` в этой сессии].

Главный технический риск — не сам libzip, а **граница ответственности**. ZIP-слой не может распознать зашифрованный DOCX и старый `.doc`: это OLE2 Compound File, а не ZIP, и libzip на них вернёт просто «не ZIP». Поэтому распознавание по магическим байтам выполняется до libzip, а сам `ZipArchive` отвечает только за ZIP и лимиты. Второй риск — кириллица в путях: libzip-путь `zip_open(path)` на MinGW уязвим к ANSI code page, поэтому файл читается через `QFile` (wide API) в память и открывается через `zip_source_buffer_create` + `zip_open_from_source` [ASSUMED по поиску; сам libzip-код не проверен].

Третий риск — зависимости. Пакет тянет `gnutls` и `nettle` (а через них — `libgnutls-30.dll`, `libnettle-9.dll`, `libhogweed-7.dll` и др.), а не только `zlib`. Эти DLL уже есть локально (из-за Qt/curl), поэтому на этой машине сборка и деплой пройдут; в чистом CI они подтянутся автоматически. Для установщика важна лицензия LGPL-компонентов (см. Open Questions).

**Primary recommendation:** libzip из MSYS2 через `find_package(libzip CONFIG REQUIRED)` (не pkg-config); в `src/documents/` класс `ZipArchive`, который принимает `QByteArray`, держит буфер по RAII, открывает архив с `ZIP_RDONLY` из памяти, проверяет число записей, дубликаты имён, точное имя `word/document.xml` и running budget распаковки; перед libzip — проверка размера (10 MB) и магических байтов (ZIP / CFB), которая отличает «не ZIP», «старый .doc» и «зашифрован».

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Проверка размера входного файла (10 MB, D-01) | Application (file I/O) | — | Владеет файлом и путём; до чтения в память (`QFileInfo::size()`) |
| Чтение байтов файла через `QFile` (wide-path на Windows) | Application (file I/O) | — | Решает проблему кириллических путей на MinGW; путь не уходит в documents-слой |
| Распознавание ZIP / CFB (старый `.doc`, зашифрованный DOCX) по магическим байтам | Documents (`src/documents/`) | — | Формат-логика, без Qt Widgets и без пути к файлу |
| Открытие ZIP, точное чтение записей, лимиты распаковки | Documents (`ZipArchive`, libzip) | — | Единственное место, где libzip; проверяется Qt Test без UI |
| Текст ошибок на русском (без пути и содержимого) | Application | Documents (коды ошибок) | Коды — в documents; тексты и подстановка только имени файла — в application (D-04, D-05) |
| Сборка libzip, CI-пакеты, деплой `libzip.dll` | Build / CI (CMake, MSYS2) | — | `CMakeLists.txt`, `ci.yml`, `cmake/DeployMinGWRuntime.cmake` |
| Парсинг XML (`word/document.xml`) | Phase 2 (documents) | — | Вне этой фазы; `ZipArchive` только отдаёт байты |
| Показ предупреждений в окне | Phase 3 (UI) | — | D-09 |

Проверка для планировщика: задача «открыть файл с диска» не должна попадать в `ZipArchive` (он принимает `QByteArray`), иначе кириллический путь снова пойдёт через `zip_open(const char*)`.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| libzip (MSYS2 `mingw-w64-x86_64-libzip`) | **1.12.1-1** | Открытие ZIP из памяти, чтение записи по имени и индексу | Публичный C API, коды ошибок, BSD-3-Clause; единственная поддерживаемая ZIP-библиотека в репозитории MSYS2 mingw64, согласованная с решением STACK.md |
| Qt 6 Core (`QFile`, `QByteArray`, `QFileInfo`) | 6.11.1 локально (минимум проекта 6.7) | Чтение файла по `QString`-пути, буферы | Уже в `resume_core` через `Qt6::Core`; wide-path API на Windows |

**Version verification:** версия и состав пакета сняты со страницы MSYS2 (`packages.msys2.org`), не из памяти [CITED: packages.msys2.org/package/mingw-w64-x86_64-libzip]. Пакет на этой машине не установлен, поэтому фактическая версия после установки будет записана через `pacman -Q mingw-w64-x86_64-libzip` (команда для исполнителя, не запущена).

### Supporting
| Library | Version (локально) | Purpose | When to Use |
|---------|--------------------|---------|-------------|
| zlib (`mingw-w64-x86_64-zlib`) | 1.3.2-2 | Зависимость libzip (inflate) | Ставится транзитивно; напрямую не используется |
| xz / bzip2 / zstd | 5.8.3-1 / 1.0.8-3 / 1.5.7-1 | Зависимости libzip (методы сжатия) | Транзитивно |
| gnutls / nettle | 3.8.13-3 / 4.0-1 | Зависимости libzip (шифрование ZIP) | Транзитивно; тянут DLL (см. Pitfall 8) |
| pkgconf | 3.0.4-1 | Поиск `.pc`, если выбран pkg-config | Не нужен при варианте CONFIG |

Локальные версии: [VERIFIED: `C:/msys64/var/lib/pacman/local`, список каталогов пакетов, этот сеанс]. Фактические версии, которые поставит CI, могут отличаться: CI использует `release: false, update: false` и установку из текущего репозитория на момент прогона.

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| libzip (MSYS2) | minizip-ng 4.2.0 (Zlib license, в mingw64) | Лицензия мягче, но зависит от OpenSSL — больше DLL; STACK.md оставляет как вариант |
| libzip (MSYS2) | miniz (вендорный single-header, MIT) | Нет системных DLL, но парсер и проверки безопасности пишем сами — оставить только если пакет недоступен (ARCHITECTURE.md) |
| libzip (MSYS2) | Собственный reader на zlib (raw inflate) | Лимиты работают на каждом выходном байте, но zip64 и data descriptor — своя работа; только если цепочка libzip неприемлема |
| Qt `QZipReader` | — | Отклонено: приватный API (`qzipreader_p.h`), без гарантий стабильности [CITED: .planning/research/STACK.md, PITFALLS.md] |

**Решение по discretion (способ подключения):** libzip из MSYS2. Пробная сборка в этом исследовании не выполнялась: пакет не установлен, а установка меняет окружение владельца. Планировщик должен сделать установку и пробную сборку первой задачей (см. Environment Availability). Запасной вариант — miniz — включается, если `pacman -S mingw-w64-x86_64-libzip` падает или установка тянет неприемлемый набор.

**Installation (команда для владельца, в оболочке MSYS2 MINGW64):**
```bash
pacman -S --needed mingw-w64-x86_64-libzip
pacman -Q mingw-w64-x86_64-libzip
```

## Package Legitimacy Audit

> Обязательно: фаза устанавливает внешний пакет.

Легитимность пакета проверена по официальному репозиторию MSYS2 и по совпадению с upstream-проектом libzip (libzip.org). Seam `gsd-tools query package-legitimacy check` в этой сессии **не запускался**: он покрывает только `npm`, `pypi` и `crates`, а MSYS2 в нём не предусмотрен. Поэтому тег здесь `[CITED]`, а не `[VERIFIED: npm registry]`.

| Package | Registry | Age | Downloads | Source Repo | Verdict | Disposition |
|---------|----------|-----|-----------|-------------|---------|-------------|
| mingw-w64-x86_64-libzip | MSYS2 (pacman) | зрелый проект libzip (давно в MSYS2) | не снято | libzip.org upstream; MSYS2 MINGW-packages | не проверено seam (MSYS2 не покрыт) | Approved with checkpoint: planner добавляет `checkpoint:human-verify` перед `pacman -S` |
| mingw-w64-x86_64-gnutls, -nettle | MSYS2 (pacman) | — | — | — | транзитивные | Уже установлены локально; без отдельного решения |

**Packages removed due to [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** none по доступным данным; seam не запускался, поэтому ни один пакет не получил вердикт OK от seam.

*Пакет `libzip` не был найден через WebSearch или обучающие данные — его имя и состав подтверждены официальной страницей MSYS2. Любые дополнительные пакеты (например, miniz) — `[ASSUMED]` и требуют отдельного подтверждения.*

## Architecture Patterns

### System Architecture Diagram

```
[Application layer]  (wire-up для фазы 2/3; в фазе 1 — функция чтения файла)
   |
   | 1. QFileInfo(path).size() > 10 MiB ?  --yes--> ошибка TooLarge (только имя файла)
   |        | no
   | 2. QFile(path) ReadOnly -> read(10 MiB + 1)   (wide-path на Windows, без записи на диск)
   |        |
   v
[documents layer: ZipArchive / DocxContainer]  (принимает QByteArray, без пути)
   |
   | 3. магические байты:
   |      "PK\x03\x04"  -> ZIP-путь
   |      D0 CF 11 E0 A1 B1 1A E1 (CFB):
   |          содержит UTF-16LE "EncryptedPackage" -> ошибка Encrypted
   |          иначе                                -> ошибка LegacyDoc
   |      иначе                                    -> ошибка NotZip
   |        |
   | 4. буфер RAII: QByteArray владеет байтами, zip_source_buffer_create(freep=0)
   |    zip_open_from_source(ZIP_RDONLY)  -- сбой: NotZip (нет PK) / Corrupt (PK есть)
   |        |
   | 5. zip_get_num_entries <= лимита записей ; нет дублей имён (один проход)
   |        |
   | 6. zip_name_locate(точное имя "word/document.xml") -- нет: ошибка MissingPart
   |        |
   | 7. zip_fopen_index + zip_fread с running budget (20 MiB, одна квота на все читаемые части)
   |      + проверка коэффициента сжатия (только после порога 1 MiB)
   |      сбой чтения / превышение -> ошибка целиком (D-07, лимит)
   v
[результат: байты word/document.xml (+ styles.xml, numbering.xml при наличии), список warnings]
   |
   v
[Phase 2: DocxReader (QXmlStreamReader) — вне фазы 1]
```

Pipeline выполняется до любой записи на диск: `QFile` открыт только на чтение, libzip открыт с `ZIP_RDONLY`, временных файлов нет.

### Recommended Project Structure
```
src/documents/
├── ZipArchive.h / .cpp     # libzip-обёртка: буфер, открытие, точное чтение, лимиты
├── DocxError.h / .cpp      # enum-коды ошибок (без текстов с содержимым)
└── README.md               # заменить заглушку (уже существует)
src/application/
└── DocumentImport.h / .cpp # размер, QFile-чтение, сопоставление кодов с русскими текстами
tests/
├── ZipArchiveTest.cpp      # Qt Test: синтетические ZIP/CFB в памяти (и в QTemporaryDir для лимита размера)
```

### Pattern 1: Буфер-владение через RAII
**What:** `ZipArchive` хранит `QByteArray` как член, создаёт `zip_source_buffer_create(data, len, 0, &err)` и `zip_open_from_source`, в деструкторе сначала `zip_discard`/`zip_close`, затем буфер уничтожается.
**When to use:** всегда, когда архив читается из памяти.
**Why:** документация libzip требует, чтобы данные жили дольше источника [CITED: libzip.org/documentation/zip_source_buffer.html]; `freep=0` оставляет владение за `QByteArray`.
**Порядок деструкции:** члены в порядке объявления: сначала `zip_t*` (освобождается в деструкторе класса), затем `QByteArray`. Буфер не должен быть перемещён/реаллоцирован после создания источника: передавать в конструктор по значению (move) до создания источника.

### Pattern 2: Точное чтение по имени
**What:** `zip_name_locate(archive, "word/document.xml", 0)` без `ZIP_FL_NOCASE`, затем `zip_fopen_index` по найденному индексу.
**Why:** `ZIP_FL_NOCASE` — единственный флаг, который игнорирует регистр; без него сравнение регистрозависимое [CITED: libzip.org/documentation/zip_name_locate.html]. Не использовать `zip_fopen(archive, name, ...)` с флагами, которые меняют поиск.
**Duplicate check:** один проход по `zip_get_num_entries` и `zip_get_name(archive, i, ZIP_FL_ENC_RAW?)` — имена собираются в `std::set`; повтор — ошибка.

### Anti-Patterns to Avoid
- **`zip_open(path)` на Windows с кириллическим путём:** narrow-path API использует ANSI code page; путь ломается [ASSUMED, поиск без libzip-источника]. Использовать буфер.
- **Доверять `zip_stat().size`:** заголовки подделываются; учитывать только фактически прочитанные байты (`zip_fread`).
- **Извлекать на диск «для удобства»:** запрещено (D-10, SAFE-02). Вся работа в памяти.
- **Частичный результат при повреждении:** D-07 запрещает. Любая ошибка `zip_fread` — отказ целиком.
- **Логировать или показывать `zip_error_strerror()`:** текст libzip может содержать внутренние детали; тексты ошибок — собственные (D-04).
- **Распознавать зашифрованный DOCX как «не ZIP»:** даёт неверный текст (D-06).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Разбор ZIP-контейнера (central directory, zip64, data descriptor) | Свой парсер | libzip | Zip64 и повреждённые заголовки — источник уязвимостей; libzip их уже обрабатывает |
| Декомпрессия deflate | Raw inflate вручную | libzip + zlib | Лимиты в этой фазе ставятся на выходе чтения, libzip отдаёт байты через `zip_fread` |
| Открытие файла с кириллическим путём | `std::ifstream` / `zip_open(const char*)` | `QFile` (QString) → `QByteArray` → буфер | Narrow-path на MinGW ломает Unicode-пути (DOCX-06) |
| Определение типа файла | Проверка расширения `.docx` | Магические байты (`PK`, CFB) | Расширение подменяется; старый `.doc` с расширением `.docx` встречается |
| Декодирование имён записей | Свой UTF-8 конвертер | `zip_get_name` с флагами libzip | Имена в не-UTF-8 кодировке в старых архивах |

**Key insight:** в этой фазе самопис разрешён только в двух местах: проверка магических байтов (несколько байт) и подсчёт лимитов поверх `zip_fread`. Всё остальное — libzip и Qt.

## Runtime State Inventory

Не применимо: фаза не переименовывает и не мигрирует существующие данные (greenfield).

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| MSYS2 MINGW64 `C:/msys64/mingw64` | Сборка всей программы | ✓ | — | — |
| GCC (`mingw-w64-x86_64-gcc`) | C++23 | ✓ | 16.1.0-5 | — |
| CMake / Ninja | Сборка | ✓ | 4.4.0-1 / 1.13.2-1 | — |
| Qt 6 base / httpserver | UI и веб | ✓ | 6.11.1-1 | — |
| pkgconf | pkg-config (если выбран) | ✓ | 3.0.4-1 | не нужен при варианте CONFIG |
| zlib / bzip2 / xz / zstd | Зависимости libzip | ✓ | 1.3.2-2 / 1.0.8-3 / 5.8.3-1 / 1.5.7-1 | — |
| gnutls / nettle | Зависимости libzip | ✓ | 3.8.13-3 / 4.0-1 | — |
| **libzip** (`mingw-w64-x86_64-libzip`) | Фаза 1 целиком | **✗** (нет `zip.h`, `libzip.pc`, `libzip.dll.a`) | — | miniz за `ZipArchive` (дорогой откат) |
| Inno Setup (`ISCC`) | Установщик | не проверено в этой сессии | — | — |

Проверка: [VERIFIED: `ls` на `C:/msys64/mingw64/include/zip.h`, `lib/libzip.dll.a`, `lib/pkgconfig/libzip.pc` — все отсутствуют; `C:/msys64/var/lib/pacman/local` — перечень установленных пакетов, этот сеанс].

**Missing dependencies with no fallback:**
- `mingw-w64-x86_64-libzip` — блокирует сборку фазы. Действие планировщика: задача «установить и проверить» (`pacman -S --needed mingw-w64-x86_64-libzip`, затем `pkg-config`/CMake-поиск), с checkpoint, потому что установка меняет машину владельца.

**Missing dependencies with fallback:**
- Нет. Miniz — дорогой откат, не равноценный.

## Common Pitfalls

### Pitfall 1: libzip есть на ПК, но не в CI
**What goes wrong:** сборка проходит локально, в CI падает на `find_package`.
**Why it happens:** CI ставит пакеты явным списком в двух местах: джоб `build-test` (матрица debug/release) и джоб `installer`.
**How to avoid:** добавить `mingw-w64-x86_64-libzip` в оба блока `install:` в `.github/workflows/ci.yml`. Строка-образец сейчас: `mingw-w64-x86_64-qt6-httpserver` (сохранить как есть и добавить после неё) [VERIFIED: `.github/workflows/ci.yml`, этот сеанс].
**Warning signs:** CI-лог `Could NOT find libzip`; локальная сборка без проблем.

### Pitfall 2: Кириллический путь через `zip_open(path)`
**What goes wrong:** на Windows с путём `C:\Users\Пётр\...` файл не открывается, ошибка выглядит как «не ZIP».
**Why it happens:** libzip открывает путь через narrow API; MinGW использует ANSI code page [ASSUMED, поиск без libzip-источника].
**How to avoid:** `QFile` с `QString` → `readAll` → буфер. Тест: путь с кириллицей во временной папке (`QTemporaryDir` с именем, содержащим кириллицу).
**Warning signs:** тест с кириллическим путём проходит только для ASCII.

### Pitfall 3: Доверие заголовкам размера
**What goes wrong:** bomb объявляет маленький `size`, а реально распаковывается в гигабайты.
**Why it happens:** `zip_stat` читает центральный каталог, который подделывается.
**How to avoid:** считать байты, возвращённые `zip_fread`, и остановиться при превышении бюджета; `zip_stat` использовать только как предварительную проверку.
**Warning signs:** тест-бомба с «лживым» `size` проходит проверку.

### Pitfall 4: Зашифрованный DOCX и старый `.doc` — не ZIP
**What goes wrong:** `zip_open_from_source` возвращает «не ZIP», и пользователь получает общий текст вместо «зашифрован» (D-06).
**Why it happens:** Word сохраняет зашифрованный OOXML как OLE2 Compound File с потоками `EncryptionInfo` и `EncryptedPackage` [CITED: pronom.nationalarchives.gov.uk/fmt/494, Container signature 16000, Type: OLE2]. Старый `.doc` — тот же контейнер без `EncryptedPackage`.
**How to avoid:** проверка магических байтов CFB до libzip; внутри CFB — байтовый поиск UTF-16LE строки `EncryptedPackage` (имена записей каталога хранятся в UTF-16LE [CITED: MS-CFB, общая структура; подпункт с сигнатурой не открыт]). Найдено → Encrypted, иначе → LegacyDoc.
**Warning signs:** тест на «зашифрованный» файл возвращает NotZip.

### Pitfall 5: Повреждённый и обрезанный ZIP неразличимы для libzip
**What goes wrong:** обрезанный файл с сохранённым `PK\x03\x04`, но без центрального каталога, даёт тот же код, что случайный мусор.
**Why it happens:** libzip ищет конец центрального каталога; без него открытие не удаётся.
**How to avoid:** если магия `PK` есть, а libzip не открыл архив, — ошибка Corrupt («повреждён или обрезан»), а не NotZip. Один текст на оба случая (D-06 перечисляет «повреждённый архив» одной категорией).
**Warning signs:** обрезанный DOCX получает текст «не DOCX».

### Pitfall 6: Дубликаты имён записей
**What goes wrong:** в архиве две записи `word/document.xml`; `zip_name_locate` вернёт первую, а Word откроет вторую.
**Why it happens:** ZIP допускает повторные имена в центральном каталоге.
**How to avoid:** один проход по всем записям, повтор нужного имени → Corrupt.
**Warning signs:** тест с двумя записями проходит без ошибки.

### Pitfall 7: Указатель на буфер, переживший `QByteArray`
**What goes wrong:** use-after-free при чтении после выхода `QByteArray` из области видимости.
**Why it happens:** `zip_source_buffer` требует, чтобы данные жили до закрытия архива [CITED: libzip.org/documentation/zip_source_buffer.html].
**How to avoid:** `QByteArray` — член класса, который закрывает архив в деструкторе раньше, чем освобождается буфер (Pattern 1). Запрет копирования класса (`= delete`).
**Warning signs:** падение под AddressSanitizer или странные данные в тесте после перемещения объекта.

### Pitfall 8: Транзитивные DLL и лицензии
**What goes wrong:** в установщике не хватает DLL (если деплой не подхватил) или LGPL-компоненты поставляются без текста лицензии.
**Why it happens:** libzip тянет `gnutls` и `nettle` [CITED: packages.msys2.org]; через них — `libgnutls-30.dll`, `libnettle-9.dll`, `libhogweed-7.dll`, `libgmp-10.dll`, `libp11-kit-0.dll`, `libtasn1-6.dll`, `libidn2-0.dll`, `libunistring-5.dll` (эти файлы **локально** есть в `C:/msys64/mingw64/bin`) [VERIFIED: этот сеанс]. Лицензии gnutls/nettle — LGPL [ASSUMED].
**How to avoid:** `DeployMinGWRuntime.cmake` уже резолвит транзитивные DLL через `file(GET_RUNTIME_DEPENDENCIES)` от исполняемого файла, поэтому `libzip.dll` будет найден, если exe от неё зависит (см. Architecture). Добавить в установщик текст лицензий (открытый вопрос).
**Warning signs:** CI-smoke-тест падает с «не найдена DLL» при запуске установленного exe с PATH без MSYS2.

### Pitfall 9: Имя DLL — не `libzip-5.dll`
**What goes wrong:** правило деплоя с маской `libzip-*.dll` не найдёт файл.
**Why it happens:** в пакете MSYS2 имя `libzip.dll` (без версии в имени) [CITED: packages.msys2.org, список файлов `bin`].
**How to avoid:** не использовать маски по версии; полагаться на `GET_RUNTIME_DEPENDENCIES`.
**Warning signs:** `ls bin/libzip-*.dll` пуст после установки.

### Pitfall 10: Тестовые фикстуры-бомбы в репозитории
**What goes wrong:** в репозиторий попадают бинарные бомбы и (хуже) реальный DOCX.
**Why it happens:** проще положить файл, чем генерировать.
**How to avoid:** генерировать фикстуры в тесте (libzip пишет в буфер или во временную папку; CFB — байтовый массив с UTF-16LE именем). В `Downloads` пользователя лежит реальный личный `.docx` — **не использовать** как фикстуру и не открывать его содержимое в тестах (AGENTS.md, SAFE-05).
**Warning signs:** `git status` показывает `.docx`, `.zip` или `.bin` в `tests/`.

## Code Examples

> Сигнатуры `zip_source_buffer`, `zip_source_buffer_create`, `zip_fopen`, `zip_fopen_index`, `zip_name_locate` взяты из документации libzip [CITED]. Остальные имена (`zip_open_from_source`, `ZIP_RDONLY`, `ZIP_CHECKCONS`, `zip_get_num_entries`, `zip_stat_index`, `zip_fread`, `zip_error_t`, `zip_source_free`) — `[ASSUMED]`: сверить с установленным `zip.h` и страницами man перед использованием. Значения ошибок `ZIP_ER_NOPASSWD` и `ZIP_ER_WRONGPASSWD` подтверждены описанием в документации [CITED: libzip.org/documentation/zip_fopen.html], остальные коды — `[ASSUMED]`.

### Открытие из буфера и точное чтение (скелет)
```cpp
// Source: сигнатуры — libzip.org/documentation/zip_source_buffer.html [CITED];
// прочие имена — ASSUMED, сверить с установленным zip.h.
// Сейчас не скомпилировано: пробная сборка — отдельная задача.
class ZipArchive {
public:
    explicit ZipArchive(QByteArray bytes);          // владеет данными (RAII)
    ~ZipArchive();                                   // сначала архив, потом буфер
    ZipArchive(const ZipArchive&) = delete;
    ZipArchive& operator=(const ZipArchive&) = delete;

    // Возвращает байты записи с точным именем или код ошибки; без записи на диск.
    std::expected<QByteArray, DocxError> readEntry(std::string_view exactName, qint64& budget);

private:
    QByteArray m_bytes;      // должен жить дольше zip_t
    zip_t* m_archive = nullptr;
};

// В конструкторе (ASSUMED имена):
zip_error_t err;
zip_error_init(&err);
zip_source_t* src = zip_source_buffer_create(m_bytes.constData(), m_bytes.size(), 0, &err);
// freep = 0: владение буфером у QByteArray; при неуспехе источник освобождается вызывающим
m_archive = zip_open_from_source(src, ZIP_RDONLY | ZIP_CHECKCONS, &err);
```

### Чтение с бюджетом (идея, не код)
- `zip_fread` возвращает фактические байты; вычитать из `budget` (20 MiB, D-02); при `budget < 0` — лимит, отказ целиком.
- Коэффициент сжатия: `total_out / max(comp_size, 1)` только после `total_out > 1 MiB`; порог 100:1 — кандидат (Open Questions).

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| libzip 1.11.4 (записано в STACK.md, ARCHITECTURE.md) | libzip **1.12.1-1** в MSYS2 | актуально на 2026-10-24 | Обновить версию в документации; сверить с установленной |
| `pkg_check_modules(LIBZIP ...)` (STACK.md) | `find_package(libzip CONFIG REQUIRED)` (рекомендация этой работы) | — | Пакет ставит `lib/cmake/libzip/libzip-config.cmake` и `libzip-targets.cmake` [CITED: список файлов пакета]; проект уже ищет Qt через `find_package` и `CMAKE_PREFIX_PATH` в пресетах; pkg-config в shell PowerShell не видит `C:/msys64/mingw64/lib/pkgconfig` без MSYS2-оболочки |
| `QZipReader` (Qt) | libzip | — | `QZipReader` приватный — не использовать |

**Deprecated / сомнительно:**
- Версия 1.11.4 из старых исследований: устарела, не использовать как факт.
- `zip_open(path)` для путей с Unicode на Windows: заменить на буфер.

**CMake-рекомендация (обоснование):** `CMakePresets.json` задаёт `"CMAKE_PREFIX_PATH": "C:/msys64/mingw64"` [VERIFIED: CMakePresets.json:12, этот сеанс]. Поиск через `find_package(libzip CONFIG)` использует этот путь так же, как Qt, и не зависит от `PKG_CONFIG_PATH` и от оболочки. Имя экспортируемой цели (`libzip::zip` ожидается) — `[ASSUMED]`: сверить с `lib/cmake/libzip/libzip-targets.cmake` после установки. Внимание: в пакете есть `FindNettle.cmake` и `Findzstd.cmake` — конфиг может вызывать `find_dependency`; при ошибке поиска первым делом проверить эти модули.

## Assumptions Log

> Все утверждения `[ASSUMED]` требуют подтверждения (пробная сборка или владелец) до фиксации как решения.

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Сигнатура CFB — `D0 CF 11 E0 A1 B1 1A E1` | Architecture Patterns, Pitfall 4 | Неверное распознавание старого `.doc` и зашифрованного DOCX. **Частично проверено**: локальный файл `doc.msi` (OLE2) начинается с `d0 cf 11 e0 a1 b1 1a e1` [VERIFIED: `od` на локальном файле, этот сеанс]; DOCX-шифрование этой проверкой не покрыто |
| A2 | Имена функций `zip_open_from_source`, `zip_get_num_entries`, `zip_stat_index`, `zip_fread`, `zip_source_free`, `zip_error_init` и флаги `ZIP_RDONLY`, `ZIP_CHECKCONS` | Code Examples | Ошибка компиляции; исправляется по `zip.h` |
| A3 | Семантика `zip_open_from_source`: при успехе источник принадлежит архиву, при неуспехе вызывающий освобождает его | Code Examples | Двойное освобождение или утечка источника |
| A4 | `zip_open(path)` на MinGW не работает с Unicode-путём | Pitfall 2 | Лишний код (буфер всё равно безопаснее) |
| A5 | Лицензии gnutls и nettle — LGPL (нужен текст лицензии в установщике) | Pitfall 8 | Нарушение лицензии при распространении |
| A6 | Экспортируемая CMake-цель `libzip::zip` | State of the Art | Ошибка линковки; исправляется по `libzip-targets.cmake` |
| A7 | Лимит количества записей 1000 | Open Questions, Security | Слишком строгий или слишком мягкий лимит |
| A8 | Порог коэффициента сжатия 100:1 после 1 MiB | Code Examples, Security | Ложные срабатывания на больших однообразных документах |
| A9 | Черновые русские тексты ошибок (раздел Open Questions) | Open Questions | Нужна правка формулировок владельцем (D-04) |
| A10 | ASVS-номера (V5, V7, V12, V14) | Security Domain | Неверная ссылка на раздел стандарта |

**If this table is empty:** не пустая — нужна проверка A1–A4 и A6 пробной сборкой и подтверждение A5, A7–A9 владельцем.

## Open Questions

1. **Область D-02: «суммарно для распакованных данных word/document.xml» — только документ или все читаемые части?**
   - What we know: CONTEXT фиксирует 20 MB «суммарно для распакованных данных `word/document.xml`». SAFE-02 и criterion 4 требуют также `word/styles.xml` и `word/numbering.xml`.
   - What's unclear: если бюджет считать только для `document.xml`, `styles.xml` и `numbering.xml` читаются без лимита.
   - Recommendation: одна квота 20 MB на все читаемые части за одно открытие (строже и безопаснее). Подтвердить владельцем: это уточнение, а не изменение решения.

2. **Лимит количества записей.**
   - What we know: CONTEXT не фиксирует; SAFE-02 требует отклонять «слишком много записей»; PITFALLS.md предлагает ~1000.
   - Recommendation: 1000 записей (A7), подтвердить.

3. **Коэффициент сжатия.**
   - What we know: SAFE-02 требует отклонять «чрезмерное сжатие»; коэффициент — отдельный лимит, PITFALLS.md рекомендует 100:1 после 1 MB, предупреждает о ложных срабатываниях.
   - Recommendation: 100:1 с порогом 1 MiB (A8); если ложных срабатываний на фикстурах нет — оставить.

4. **Тексты ошибок (D-04, D-06): черновики для утверждения.**
   - NotZip: «Файл не является DOCX (это не ZIP-архив). Сохраните резюме в Word как .docx и повторите.»
   - Corrupt: «Файл повреждён или обрезан. Сохраните резюме заново в Word и повторите.»
   - Encrypted: «Файл зашифрован. Сохраните резюме без пароля и повторите.»
   - LegacyDoc: «Это старый формат .doc, а не .docx. Откройте файл в Word, сохраните как .docx и повторите.»
   - TooLarge: «Файл больше 10 МБ. Уменьшите файл (например, удалите изображения) и повторите.»
   - Limit: «Файл отклонён: содержимое превышает допустимый размер после распаковки. Сохраните резюме заново в Word и повторите.»
   - MissingPart: «В файле нет основного текста резюме (word/document.xml). Сохраните файл как .docx в Word и повторите.»
   - Каждая строка включает имя файла (D-05) и не содержит фрагментов содержимого.

5. **Проверка распознавания зашифрованного DOCX без реального файла.**
   - No sample: в окружении нет зашифрованного DOCX. Синтетическая фикстура проверяет логику, но не реальный файл Word.
   - Recommendation: TEST-02 (фаза 3) включает ручную проверку на реальном зашифрованном файле, который владелец создаёт сам и не коммитит.

6. **Лицензии LGPL-компонентов в установщике.**
   - What we know: через libzip приходят gnutls/nettle (A5); проект — GPL-3.0-or-later (STACK.md).
   - Recommendation: решить, добавлять ли тексты лицензий в установщик; это вне кода фазы, но влияет на `installer/AIResumeAssistant.iss`.

7. **Версия libzip в CI и локально.**
   - Recommendation: записать фактическую версию (`pacman -Q`) в `docs/memory.md` после пробной сборки и в CI-логе; пин версии в MSYS2 не предусмотрен.

## Environment Availability (контрольный список для исполнителя)

- [ ] `pacman -S --needed mingw-w64-x86_64-libzip` (checkpoint: владелец запускает или подтверждает)
- [ ] Проверить `C:/msys64/mingw64/include/zip.h` на имена из A2 и значения флагов
- [ ] Проверить `C:/msys64/mingw64/lib/cmake/libzip/libzip-targets.cmake` на имя цели (A6)
- [ ] `cmake --preset mingw-debug` и `cmake --build --preset debug` (NOT RUN до выполнения)
- [ ] `ctest --preset debug` (NOT RUN до выполнения)

## Validation Architecture

> Включено: ключ `workflow.nyquist_validation` в `.planning/config.json` не задан в `false` (ключ отсутствует либо `true` по умолчанию).

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Qt Test (`Qt6::Test`), подключение через `qt_add_executable` и `add_test`, как в `CMakeLists.txt` |
| Config file | `CMakeLists.txt` (новая цель `zip_archive_test`) |
| Quick run command | `ctest --preset debug -R zip_archive` |
| Full suite command | `cmake --build --preset debug && ctest --preset debug` |

Существующие цели: `resume_e2e`, `resume_json`, `web_api`, `web_e2e` (условный). Образец: `add_test(NAME resume_json COMMAND resume_json_test)` [VERIFIED: `CMakeLists.txt`, этот сеанс].

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| SAFE-01 | Не-ZIP текст → NotZip | unit | `ctest --preset debug -R zip_archive` | ❌ Wave 0 |
| SAFE-01 | Обрезанный ZIP с `PK` → Corrupt | unit | как выше | ❌ Wave 0 |
| SAFE-01 | Синтетический CFB с `EncryptedPackage` (UTF-16LE) → Encrypted | unit | как выше | ❌ Wave 0 |
| SAFE-01 | Синтетический CFB без `EncryptedPackage` → LegacyDoc | unit | как выше | ❌ Wave 0 |
| SAFE-01 | ZIP без `word/document.xml` → MissingPart | unit | как выше | ❌ Wave 0 |
| SAFE-01 | Файл > 10 MiB → TooLarge (проверка до чтения) | unit | как выше | ❌ Wave 0 |
| SAFE-01 | Ошибки содержат только имя файла, без полного пути и без фрагментов содержимого | unit | как выше | ❌ Wave 0 |
| SAFE-01 | Ни один файл не создан и не изменён (снимок `QDir` до/после) | unit | как выше | ❌ Wave 0 |
| SAFE-01 | Путь с кириллицей открывается (`QTemporaryDir` с кириллицей) | integration | как выше | ❌ Wave 0 |
| SAFE-02 | Бомба 20 MiB+ (один zero-entry, сжатый) → лимит, память не растёт сверх бюджета | unit | как выше | ❌ Wave 0 |
| SAFE-02 | Бомба с ложным `size` в заголовке → лимит по факту | unit | как выше | ❌ Wave 0 |
| SAFE-02 | Записей > лимита (например, 1001) → отказ | unit | как выше | ❌ Wave 0 |
| SAFE-02 | Две записи с одним именем → Corrupt | unit | как выше | ❌ Wave 0 |
| SAFE-02 | Имена `WORD/document.xml`, `word/document.xml.bak` не совпадают с точным | unit | как выше | ❌ Wave 0 |
| SAFE-02 | Коэффициент сжатия > 100:1 после 1 MiB → отказ | unit | как выше | ❌ Wave 0 |
| SAFE-01/02 | Корректный синтетический DOCX: байты `word/document.xml`, `styles.xml`, `numbering.xml` совпадают | unit | как выше | ❌ Wave 0 |
| SC-1 | Сборка и `ctest` с нуля, пакет в CI | build/CI | `cmake --build --preset debug`, CI-джоб | ❌ (CI-список) |
| SC-2 | Установленная папка запускается без MSYS2 в PATH | smoke (CI) | существующий smoke в `ci.yml` | ✅ (нужна проверка `libzip.dll`) |

### Sampling Rate
- **Per task commit:** `ctest --preset debug -R zip_archive` (после сборки)
- **Per wave merge:** `cmake --build --preset debug && ctest --preset debug`
- **Phase gate:** полный набор зелёный до `/gsd-verify-work`. Результаты записываются только после реального запуска; до тех пор — NOT RUN.

### Wave 0 Gaps
- [ ] `tests/ZipArchiveTest.cpp` — покрывает SAFE-01/02 (вся таблица выше)
- [ ] Цель `zip_archive_test` в `CMakeLists.txt` с `add_test(NAME zip_archive COMMAND zip_archive_test)` и `Qt6::Test`
- [ ] Вспомогательный генератор фикстур в тесте (ZIP через libzip, CFB байтами) — без бинарных файлов в git
- [ ] Строка `mingw-w64-x86_64-libzip` в оба блока `install:` в `.github/workflows/ci.yml`
- [ ] Установка пакета и пробная сборка (см. Environment Availability)

## Security Domain

> Включено: `security_enforcement: true` (ключ по умолчанию), `security_asvs_level: 1`, `security_block_on: high` в `.planning/config.json` [VERIFIED: этот сеанс].

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | нет | — (локальное чтение файла) |
| V3 Session Management | нет | — |
| V4 Access Control | нет | — (файл выбирает пользователь) |
| V5 Input Validation | да | Проверка размера и магических байтов; точные имена записей; libzip; лимиты (A7–A8) |
| V6 Cryptography | нет | Не реализуется; расшифровка не делается, только распознавание (D-06, v2) |
| V7 Error Handling and Logging | да | Тексты без содержимого и пути (D-04, D-05, SAFE-05); в логи ничего не пишется |
| V12 Files and Resources | да | Лимиты 10 MB / 20 MB; никакой записи на диск; без извлечения (zip-slip не применим) |
| V14 Configuration | да | Пакет MSYS2 из официального репозитория; версия фиксируется в `docs/memory.md` |

### Known Threat Patterns for C++/Qt/libzip reader

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Zip bomb (распаковка в гигабайты) | Denial of Service | Running budget по `zip_fread`, лимит записей, коэффициент сжатия (SAFE-02) |
| Ложный `size` в центральном каталоге | Tampering | Учёт фактических байтов, не заголовков |
| Дубликаты имён записей (подмена документа) | Tampering | Проверка уникальности имён, отказ при дубликате |
| Совпадение по регистру/подстроке (`word/document.xml.bak`) | Spoofing | Точное сравнение без `ZIP_FL_NOCASE` |
| Повреждённые заголовки CFB (выход за границы) | Denial of Service / Tampering | Байтовый поиск с проверкой границ; никаких указателей по смещениям из файла |
| Утечка текста резюме в ошибку или лог | Information Disclosure | Собственные тексты ошибок; без `zip_error_strerror` в UI; без логов (SAFE-05) |
| Подмена DLL при деплое | Tampering | Установка только из официального MSYS2; `libzip.dll` резолвится из каталога `mingw64/bin` |

Для ASVS V5/V7/V12 номера приведены по ASVS 4.0 [ASSUMED]; проверить уровень 1 по актуальной версии.

## Project Constraints (from CLAUDE.md / AGENTS.md)

- Перед задачей читать `docs/plan.md` и `docs/memory.md` (прочитаны в этом сеансе).
- Стандарт C++23 не понижать; Qt 6 Widgets, CMake, SQLite; минимум Qt 6.7 (`CMakeLists.txt`).
- Сборка и тесты: `cmake --preset mingw-debug`, `cmake --build --preset debug`, `ctest --preset debug`; одиночный тест: `ctest --preset debug -R <имя>`.
- Никогда не утверждать, что тесты прошли, если они не запускались (в этом исследовании ничего не запускалось).
- Format: `clang-format --dry-run --Werror <изменённые .cpp/.h>`. Существующие файлы уже не проходят; новые файлы в этой работе отформатированы с `PointerAlignment: Left` (из `docs/memory.md`).
- Linter: `clang-tidy <файл> -p build/mingw-debug` при наличии compilation database.
- Слои: UI → domain/application → storage/documents; не переносить логику в QWidget.
- Ошибки без секретов и приватных данных; резюме не попадает в логи и сообщения (SAFE-05).
- Для новых возможностей писать тесты и обновлять документацию (`docs/memory.md` после значимой задачи).
- Не читать файлы с секретами и переменными окружения; не коммитить ключи, исходные резюме и персональные данные; каталоги сборки не коммитить.
- Коммит в отдельную ветку, в `main` только через PR. Текущая ветка — `chore/map-codebase`.
- Тесты: Qt Test, синтетические детерминированные данные, без внешних API (`.claude/skills/write-tests/SKILL.md`).
- `.claude/rules/llm-security.md` — только для `src/llm/**`; к этой фазе не применяется.

## Sources

### Primary (HIGH confidence)
- [MSYS2 package mingw-w64-x86_64-libzip](https://packages.msys2.org/package/mingw-w64-x86_64-libzip) — версия 1.12.1-1, BSD-3-Clause, зависимости (bzip2, gnutls, nettle, xz, zlib, zstd), список файлов (`libzip.dll`, `zip.h`, `libzip.pc`, `lib/cmake/libzip/libzip-config.cmake`, `libzip-targets.cmake`)
- [libzip: zip_source_buffer](https://libzip.org/documentation/zip_source_buffer.html) — сигнатуры `zip_source_buffer` и `zip_source_buffer_create`; правило владения буфером
- [libzip: zip_fopen](https://libzip.org/documentation/zip_fopen.html) — сигнатуры `zip_fopen`, `zip_fopen_index`; описание `ZIP_ER_NOPASSWD` и `ZIP_ER_WRONGPASSWD`
- [libzip: zip_name_locate](https://libzip.org/documentation/zip_name_locate.html) — сигнатура; описание `ZIP_FL_NOCASE`, `ZIP_FL_ENC_RAW`, `ZIP_FL_ENC_UTF_8`
- Локальная проверка (этот сеанс): `C:/msys64/var/lib/pacman/local` (версии gnutls, nettle, zlib, bzip2, xz, zstd, Qt, CMake, Ninja, GCC), отсутствие `zip.h`/`libzip.pc`/`libzip.dll.a`, сигнатура OLE2 на локальном `doc.msi`
- Файлы проекта (этот сеанс): `CLAUDE.md`, `AGENTS.md`, `docs/plan.md`, `docs/memory.md`, `CMakeLists.txt`, `CMakePresets.json`, `cmake/DeployMinGWRuntime.cmake`, `installer/AIResumeAssistant.iss`, `.github/workflows/ci.yml`, `.clang-format`, `.clang-tidy`, `.claude/settings.json`, `.claude/skills/write-tests/SKILL.md`, `.planning/config.json`, `.planning/phases/01-.../01-CONTEXT.md`, `.planning/REQUIREMENTS.md`, `.planning/STATE.md`

### Secondary (MEDIUM confidence)
- [PRONOM fmt/494 (Office encrypted document)](https://pronom.nationalarchives.gov.uk/fmt/494) — контейнер OLE2, потоки `EncryptedPackage` и `EncryptionInfo`
- [MS-CFB: Compound File Binary File Format](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-cfb/53989ce4-7b05-4f8d-829b-d08d6148375b) — формат существует; таблица сигнатуры на этой странице не показана
- [MS-OFFCRYPTO](https://msopenspecs.microsoft.com/files/MS-OFFCRYPTO/%5bMS-OFFCRYPTO%5d-240416.docx) — найден в поиске, не открыт (только метаданные)

### Tertiary (LOW confidence)
- Поиск по поведению `zip_open` на Windows с Unicode-путями (без источника из libzip) — основа для A4, требует проверки
- Страница `libzip.org/documentation/zip_open_from_source.html` — 404, сигнатура не проверена (A2, A3)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — версия и состав пакета с официальной страницы MSYS2; локальное состояние проверено
- Architecture: HIGH для слоёв и порядка проверок (по CONTEXT и требованиям); MEDIUM для распознавания CFB (сигнатура подтверждена локально для `.msi`, не для DOCX)
- Pitfalls: MEDIUM — часть рисков (кириллица, семантика источника) по поиску, не по исходникам libzip
- CMake and API names: LOW до сверки с установленными `zip.h` и `libzip-targets.cmake`

**Research date:** 2026-10-24
**Valid until:** 2026-11-23 (30 дней; версия пакета MSYS2 меняется чаще — проверить перед установкой)
