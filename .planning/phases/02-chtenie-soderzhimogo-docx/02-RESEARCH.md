# Phase 2: Чтение содержимого DOCX - Research

**Researched:** 2026-10-24
**Domain:** C++23 / Qt 6.11.1 — разбор OOXML-частей (`word/document.xml`, `word/styles.xml`, `word/numbering.xml`) в слое `src/documents/`, без UI и без SQLite
**Confidence:** MEDIUM. Поведение `QXmlStreamReader` проверено пробами на установленном Qt 6.11.1 (HIGH для этих пунктов). Правила Word (русские имена стилей, форма текстовых полей в `mc:AlternateContent`, состав `w:numPr` в стилях) не подтверждены файлом, сохранённым Word, и помечены `[ASSUMED]`.

**Легенда тегов:** `[VERIFIED: local probe]` — запуск на установленном Qt 6.11.1, вывод приведён; `[VERIFIED: путь:строки]` — файл репозитория прочитан в этой сессии, значение приведено дословно; `[CITED: url]` — внешний источник, см. раздел Sources (tier из `classify-confidence` указан там же); `[ASSUMED]` — знание без проверки в этой сессии, требует подтверждения владельца или реального файла.

<user_constraints>
## User Constraints (from CONTEXT.md)

CONTEXT.md для фазы 2 не создан (`/gsd-discuss-phase` не запускался). Ограничения ниже взяты из ROADMAP, STATE, REQUIREMENTS, PROJECT и AGENTS.md.

### Locked Decisions (из STATE.md, ROADMAP.md, PROJECT.md, AGENTS.md)
- Слой документов принимает байты, а не пути; в нём нет UI и SQLite; он возвращает коды, тексты ошибок живут в слое приложения (`src/documents/DocxTypes.h`, комментарий шапки; `src/documents/README.md`, раздел «Правила слоя»).
- Сервис импорта (`DocumentImport`) входит в фазу 2, чтобы кириллический путь проверялся без UI (STATE.md, «Roadmap»-решение). Он уже создан в фазе 1 (`src/application/DocumentImport.cpp`) и в фазе 2 расширяется.
- Читатель в фазах 1–2 остаётся чистой функцией без доступа к черновику; подтверждение замены черновика — фаза 3 (STATE.md).
- Тестовые резюме и персональные данные не коммитятся; фикстуры синтетические, генерируются в коде (STATE.md, PROJECT.md; `tests/DocxFixtures.h`, шапка).
- Текст резюме не попадает в логи и в сообщения об ошибках (SAFE-05, REQUIREMENTS.md).
- Текстовые поля и колонтитулы в v1 — только предупреждение, не содержимое (ROADMAP фаза 2, SC4; REQUIREMENTS DOCX-V2-03).
- Лимит текста `kMaxTextChars` = 200000 (D-03) должен быть принудительно проверен в фазе 2 (`src/documents/DocxTypes.h`, строки 47–48).
- Значения лимитов (1000 записей, 100:1 после 1 MiB, 20 MiB на три части) ждут подтверждения владельца (`01-VERIFICATION.md`, раздел Owner Decisions).
- Перед коммитом: `cmake --build --preset debug` и `ctest --preset debug`; отчёт не должен утверждать, что тесты прошли, без запуска (AGENTS.md, раздел 2 и 4).

### Claude's Discretion (исследование даёт рекомендацию, решение за планировщиком)
- Форма модели (типы, имена), разбиение на планы, политика неизвестных элементов.
- Способ сборки нумерации (только маркер и номер по `numFmt` или счётчики с перезапуском).
- Способ обнаружения колонтитулов (новый перечень имён в `ZipArchive` или чтение `_rels`).
- Значение предела глубины вложенности (не задано в проекте).

### Deferred Ideas (OUT OF SCOPE для фазы 2)
- Колонтитулы и текстовые поля как полноценное содержимое (DOCX-V2-03); старый `.doc` (DOCX-V2-01); DOCX с паролем (DOCX-V2-02).
- Отображаемый адрес гиперссылки и точная форма нумерации (roman, letters, пользовательский `lvlText`) — дифференциаторы из `.planning/research/FEATURES.md`, не в v1.
- Извлечение карьерных этапов из DOCX (MAP-V2-01, Sprint 2).
- Окно Qt, предпросмотр, подтверждение замены, строка состояния — фаза 3.
- Импорт PDF, изображения, запись DOCX — вне этапа (REQUIREMENTS.md, Out of Scope).
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description (дословно из REQUIREMENTS.md) | Research Support |
|----|-------------------------------------------|------------------|
| DOCX-02 | Абзацы показываются в порядке документа | Обход `w:body` в порядке потока; `w:p`, `w:tbl` в одной последовательности блоков (раздел Architecture Patterns, Pattern 2) |
| DOCX-03 | Заголовки показываются как заголовки (по стилю) | `w:pStyle` → `styles.xml`; сопоставление по `styleId`, `w:name`, `w:outlineLvl` (Pattern 3). Русское имя `Заголовок N` — `[ASSUMED]` (Assumptions A1) |
| DOCX-04 | Маркированные и нумерованные списки показываются с вложенностью | `w:numPr` (`w:numId`, `w:ilvl`) → `numbering.xml` → `w:numFmt` (Pattern 4) |
| DOCX-05 | Таблицы показываются с строками, пустыми и объединёнными ячейками | `w:tblGrid`, `w:gridSpan`, `w:vMerge` (Pattern 5; ISO-цитата в Common Pitfalls) |
| DOCX-06 | Кириллица в тексте и путях к файлу читается корректно | Декодирование делает `QXmlStreamReader` (проверено пробой, включая windows-1251 и UTF-16 с BOM). Путь уже покрыт `DocumentImportTest::readsDocxFromCyrillicDirectory` (`tests/DocumentImportTest.cpp:53`) |
| DOCX-07 | Изображения пропускаются | `w:drawing`, `w:pict`, `w:object` пропускаются, каждый экземпляр считается (SAFE-04) |
| DOCX-08 | Гиперссылки и вставки показываются как текст; удалённые исправления не показываются | `w:hyperlink`, `w:ins`, `w:sdtContent`, `w:fldSimple` — разворачивать; `w:del` и `w:delText` — пропускать целиком |
| SAFE-03 | DTD и внешние сущности XML отклоняются | Любой токен `DTD` отклоняет часть до чтения содержимого. Проверено: внутренние сущности раскрываются, внешние молча пропадают (Pitfall 1) |
| SAFE-04 | Пропущенное содержимое считается и показывается предупреждением, а не теряется молча | Один код предупреждения на каждый пропущенный экземпляр; размер списка = число пропусков (как в `DocxContainer.h:15`). Колонтитулы требуют перечня имён ZIP (Open Question 2) |
| SAFE-05 | Текст резюме не попадает в логи и сообщения об ошибках | Ошибки XML — только код; `QXmlStreamReader::errorString()` не передаётся наружу, он содержит имена из документа (проверено). Тест через `qInstallMessageHandler` (Validation Architecture) |
| TEST-01 | Qt Test на синтетических DOCX-фикстурах, включая вредоносные случаи | Новая цель `docx_content_test` (`tests/DocxContentTest.cpp`), фикстуры в коде (Validation Architecture) |
</phase_requirements>

## Summary

Фаза 2 превращает три уже прочитанные части (`DocxParts`, фаза 1) в модель документа: блоки (абзацы, таблицы), заголовки, списки с уровнем вложенности и список предупреждений о пропущенном содержимом. Вся работа идёт в слое `src/documents/` (без UI); сервис `DocumentImport` получает модель и переводит коды в русские тексты.

Главные факты, проверенные на установленном Qt 6.11.1 и GCC 16.1.0:

1. **DTD-токен приходит до содержимого.** `<!DOCTYPE ...>` даёт токен `DTD` (`isDTD()` равен 1) до первого элемента, поэтому отклонение по этому токену успевает раньше любого раскрытия сущностей. Внутренние сущности и вложенные сущности (billion-laughs форма) раскрываются автоматически; внешние `SYSTEM`-сущности (и параметрические) **молча пропадают**: нет текста, нет токена-ссылки, нет ошибки. Молчаливая потеря при простом «пропустить DTD» нарушает SAFE-04.
2. **Пробелы.** Пробельный `w:t` (`xml:space="preserve"` с одним пробелом) отдаётся с `isWhitespace() == 1`. Фильтр по `isWhitespace()` внутри `w:t` удалит значимые пробелы. CDATA делит один `w:t` на несколько токенов `Characters`, поэтому текст нужно склеивать из всех токенов.
3. **Кодировки.** Qt уважает объявление кодировки: `encoding="windows-1251"` с байтами cp1251 даёт «Тест»; `encoding="UTF-8"` с байтами cp1251 даёт ошибку `Encountered incorrectly encoded content.`. Правило OPC (только UTF-8 и UTF-16) известно из вторичных источников — решение за владельцем (Open Question 3).
4. **Ошибки.** `errorString()` при неизвестной сущности содержит её имя из документа (`Entity 'nope' not declared.`) — значит, текст ошибки XML нельзя показывать и логировать. Коды ошибок (`NotWellFormedError = 3`, `PrematureEndOfDocumentError = 4`) стабильны и маппятся в `DocxError::Corrupt`.
5. **Колонтитулы и текстовые поля не лежат в трёх частях.** `header*.xml`/`footer*.xml` — отдельные части; `ZipArchive` сейчас не даёт перечень имён (`ZipArchive.h`, только `hasEntry` и `readEntry`), поэтому нужен небольшой дополнительный метод (только имена, без чтения содержимого).

**Primary recommendation:** разбирать части через `QXmlStreamReader` с `setNamespaceProcessing(true)`; сравнивать пространство по `namespaceUri()`, а не по префиксу; при первом токене `DTD` возвращать новый код (предложение `UnsafeXml`, `[ASSUMED]`); при `hasError()` возвращать `Corrupt` без текста ошибки; текст `w:t` собирать из всех `Characters` без фильтрации пробелов; модель хранить в `src/documents/`; предупреждения — по одному коду на каждый пропущенный экземпляр; лимит `kMaxTextChars` проверять при накоплении текста.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Отклонение DTD и сущностей (SAFE-03) | documents (парсер) | — | Решение о безопасности принимается на токенах XML до модели |
| Декодирование и пространства имён | documents (QXmlStreamReader) | — | Декодирование делает Qt; парсер сравнивает `namespaceUri()` |
| Заголовки и списки (DOCX-03, DOCX-04) | documents (модель + разбор стилей и нумерации) | — | Чистая логика на `DocxParts`, без UI |
| Таблицы и объединение ячеек (DOCX-05) | documents (модель) | UI (фаза 3, только отрисовка) | Геометрия столбцов задаётся моделью, UI её не пересчитывает |
| Подсчёт пропусков (SAFE-04) | documents (коды `DocxWarning`) | — | Коды без текста, как в `DocxTypes.h` |
| Русские тексты ошибок и предупреждений | application (`DocumentImport.cpp`) | — | Правило слоя из `src/documents/README.md` |
| Перечень имён ZIP для колонтитулов | documents (`ZipArchive`) | — | Только имена; содержимое не читается |
| Показ модели и строка состояния | UI (фаза 3) | — | Вне этой фазы |

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| QXmlStreamReader (Qt6::Core) | Qt 6.11.1 локально (`qVersion()` в пробе) | Потоковый разбор OOXML-частей | Уже в `Qt6::Core`, новых зависимостей нет; пространства имён включаются явно через `setNamespaceProcessing(true)` (в пробе так и сделано); токен `DTD` — точка отказа; ссылка на необъявленную сущность даёт ошибку `NotWellFormedError` [VERIFIED: local probe; `C:/msys64/mingw64/include/qt6/QtCore/qxmlstream.h`] |
| libzip | 1.11.4-1 (MSYS2, `pacman` local DB) | Чтение ZIP из памяти (фаза 1) | Используется как есть, `ZipArchive::readEntry` |
| std::expected, std::variant, std::vector | C++23, GCC 16.1.0 | Код ошибки без частичного результата (D-07), модель блоков | Уже используется в фазе 1 (`src/documents/DocxContainer.h`) |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Qt Test (`Qt6::Test`) | Qt 6.11.1 | Тесты, `QTEST_GUILESS_MAIN` | Все тесты фазы (как `tests/ZipArchiveTest.cpp`) |
| `qInstallMessageHandler` | Qt 6.11.1 | Перехват сообщений Qt в тесте SAFE-05 | Тест «маркер не попадает в логи» |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| QXmlStreamReader (потоковый разбор) | pugixml 1.16 (упомянут в `.planning/research/STACK.md`; в этой сессии не проверялся) | DOM проще для вложенных таблиц и `sdt`, но новая зависимость и DOM-парсер с полным деревом в памяти. Брать только если потоковый обход станет нечитаемым (STACK.md) |
| Собственный обход по строкам/регулярным выражениям | — | Не использовать: ломается на пространствах имён и сущностях (`.planning/research/STACK.md`, раздел «Don't use») |

**Installation:** новых пакетов нет. Фаза 2 использует уже подключённые `Qt6::Core` и `resume_documents`.

**Version verification:** Qt 6.11.1 подтверждён пробой (`qVersion()=6.11.1`). Минимум в `CMakeLists.txt` — `find_package(Qt6 6.7 REQUIRED ...)`: поведение DTD-токена на 6.7 не проверялось `[ASSUMED]`, см. Assumptions A8.

## Package Legitimacy Audit

Фаза 2 не устанавливает внешних пакетов (QXmlStreamReader входит в установленный Qt; libzip поставлен в фазе 1). Проверка легитимности (`gsd-tools query package-legitimacy`) не требуется; таблица аудита пуста.

## Architecture Patterns

### System Architecture Diagram

```
  DocxParts (фаза 1): documentXml, stylesXml, numberingXml, warnings(Styles/Numbering)
            |
            v
  [1] Guard: QXmlStreamReader по каждой части
        |-- hasError()  ------------------------> Corrupt (без текста ошибки)
        |-- токен DTD   ------------------------> UnsafeXml [предложение]
        |-- объявленная кодировка не UTF-8/16 --> Corrupt [решение владельца, Open Q3]
            |
            v
  [2] Разбор стилей и нумерации (styles.xml, numbering.xml)
        styleId -> уровень заголовка | numId -> abstractNum -> numFmt (маркер/номер)
            |
            v
  [3] Обход word/document.xml (стек элементов, без рекурсии без предела)
        body -> p ----------> абзац (текст, уровень заголовка, список)
        body -> tbl --------> таблица (tblGrid, tr, tc, gridSpan, vMerge)
        body -> sdt --------> разворачивание sdtContent
        r/hyperlink/ins/fldSimple/smartTag --> текст (w:t, Characters склеиваются)
        del, delText, instrText --------------> пропуск
        drawing, pict, object, txbxContent ---> пропуск + предупреждение
            |                         |
            |                  счётчик пропусков -> DocxWarning (по одному на экземпляр)
            v
  [4] Проверка лимитов: kMaxTextChars (накопленный текст) -> Limit; глубина -> Limit [предложение]
            |
            v
  DocxDocument { blocks, warnings }  -->  DocumentImport (application): коды -> русские тексты
            |
            v
  фаза 3: UI (только показ модели)
```

Точки решения: [1] — отказ на любом DTD или ошибке XML, без частичного результата (D-07); [3] — ветка по имени элемента, неизвестный содержательный элемент считается пропуском (SAFE-04).

### Recommended Project Structure
```
src/documents/
├── DocxTypes.h          # + новые коды DocxError / DocxWarning (коды, без текста)
├── DocxContainer.*      # без изменений (три части)
├── ZipArchive.*         # + перечень имён записей (только имена), см. Open Q2
├── DocxModel.h          # NEW: Paragraph, Table, TableRow, TableCell, Block, DocxDocument
├── DocxContent.h/.cpp   # NEW: разбор стилей, нумерации и document.xml в DocxDocument
└── README.md            # обновить таблицу кодов и правила слоя
src/application/
└── DocumentImport.*     # + модель в ImportedDocx, тексты новых кодов (владелец проверит в фазе 3)
tests/
├── DocxContentTest.cpp  # NEW: цель docx_content_test, ctest-имя docx_content
└── DocxFixtures.h       # + билдеры word/styles.xml, numbering.xml, document.xml
```

Имя теста следует конвенции `tests/<Subject>Test.cpp` и правилу «snake_case + `_test`» (`.planning/codebase/TESTING.md`, раздел Naming).

### Pattern 1: Guard на токенах (SAFE-03, SAFE-05)

Скелет (набросок, не проверен компиляцией в проекте; имя кода `UnsafeXml` — предложение `[ASSUMED]`):

```cpp
QXmlStreamReader xml(partBytes);
xml.setNamespaceProcessing(true);
while (!xml.atEnd()) {
    const QXmlStreamReader::TokenType token = xml.readNext();
    if (xml.hasError())                                // только код, без xml.errorString() (SAFE-05)
        return std::unexpected(DocxError::Corrupt);
    if (token == QXmlStreamReader::DTD)               // раньше любой ссылки на сущность
        return std::unexpected(DocxError::UnsafeXml); // предложение, [ASSUMED]
    // ... ветки StartElement / Characters / EndElement
}
```

Значения токенов и кодов ошибок подтверждены: `DTD` и `isDTD()` из `qxmlstream.h` (строки 245, 298), значения `Error` из пробы (`NoError=0 CustomError=2 NotWellFormedError=3 PrematureEndOfDocumentError=4 UnexpectedElementError=1`, пробный вывод `probe2-out.txt`).

### Pattern 2: Обход тела и сохранение порядка (DOCX-02)

- Один стек открытых элементов; ветка по `namespaceUri()` и `name()`, префикс не используется.
- `w:body` содержит блоки в порядке потока; `w:sectPr` (свойства страницы) пропускать без предупреждения.
- Неизвестный элемент с содержимым в `w:body` — пропуск с предупреждением (`OtherSkipped`, предложение); служебные (`w:bookmarkStart`, `w:bookmarkEnd`, `w:proofErr`) — без предупреждения. Это закрывает SAFE-04 «не теряется молча».
- Глубина вложенности (`tbl` → `tc` → `tbl` … и `sdt`) ограничивается предельным значением (предложение: 64, `[ASSUMED]`), превышение — `Limit`. Рекурсия без предела на враждебном вложении — риск переполнения стека (Assumptions A9).

### Pattern 3: Текст и заголовки (DOCX-02, DOCX-03, DOCX-08)

Текст абзаца — конкатенация всех `Characters` внутри `w:t` в порядке потока, **без** `isWhitespace()` и **без** `trimmed()`:

```cpp
// внутри w:t (inText == true)
if (token == QXmlStreamReader::Characters && inText)
    paragraphText += xml.text();   // все токены, включая пробельные; CDATA даёт отдельные токены
```

Проверено: `<w:t xml:space="preserve"> </w:t>` даёт `Characters=" " isWhitespace=1`; `<w:t>a<![CDATA[b]]>c</w:t>` даёт три токена (`a`, `b` с `isCDATA=1`, `c`), склейка даёт `abc`; `A&amp;B` даёт один токен `A&B`; пустой `<w:t/>` — ноль токенов (`probe2-out.txt`).

Заголовок (порядок проверки, см. Assumptions A1 и A3):
1. `w:pPr/w:pStyle@w:val` → `styleId` в `styles.xml`; следовать `w:basedOn`.
2. Уровень: `styleId` вида `Heading1`…`Heading9`, либо `w:name` в нижнем регистре `heading N` (английское внутреннее имя; соответствие UI-имени `Heading 1` ↔ `heading 1` подтверждено в `python-docx` BabelFish [CITED: raw.githubusercontent.com/python-openxml/python-docx/master/src/docx/styles/__init__.py]), либо `w:outlineLvl` в `w:pPr` стиля (уровень = значение + 1).
3. Русское имя `Заголовок N` — `[ASSUMED]`: источник LibreOffice bug 115856 описывает локализованные имена встроенных стилей, но конкретный файл русского Word не проверен. Надёжный путь — `styleId` и `outlineLvl`, русское имя как дополнительное совпадение.

### Pattern 4: Списки (DOCX-04)

Цепочка: `w:p/w:pPr/w:numPr` (`w:numId`, `w:ilvl`) → `w:num` с этим `w:numId` → `w:abstractNumId` → `w:abstractNum/w:lvl[@w:ilvl]/w:numFmt@w:val`. Маркер (`bullet`) — маркированный список; любое другое значение (`decimal`, `lowerLetter`, `upperRoman` и т. д.) — нумерованный.

Правила из ISO/IEC 29500-1 (цитата через Microsoft Learn, tier по seam LOW — сверить со спецификацией):
> "When this element appears as part of the paragraph formatting for a paragraph style, then any numbering level defined using the ilvl element shall be ignored, and the pStyle element (§17.9.24) on the associated abstract numbering definition shall be used instead."

Перевод: если `numPr` задан в стиле абзаца, `ilvl` из стиля игнорируется, уровень берётся из связанного абстрактного определения.

Word не хранит метки списков в тексте: метку (`•` или номер) формирует разбор. Для v1 рекомендуется: маркер `•` для `bullet`, номер — счётчик на паре `(numId, ilvl)` с перезапуском при появлении более высокого уровня (`[ASSUMED]`, точное правило Word не проверено). Не копировать `w:lvlText` как есть: для маркеров он часто указывает на символ шрифта Symbol `[ASSUMED]`.

`numId` со значением `0` означает «без нумерации» `[ASSUMED]` (в цитате Microsoft Learn `numId 0` приведён только как пример, без определения семантики).

### Pattern 5: Таблицы (DOCX-05)

- Ширина сетки — число `w:gridCol` в `w:tblGrid`; строка может содержать меньше `w:tc`, это не ошибка, пустые ячейки добавляются до ширины сетки.
- `w:tc/w:tcPr/w:gridSpan@w:val` — число столбцов, которые занимает ячейка `[ASSUMED]` (знание спецификации, не извлечено в этой сессии).
- `w:tc/w:tcPr/w:vMerge`: `w:val="restart"` начинает группу, `w:val="continue"` продолжает, элемент без `w:val` в цитате ISO означает «не часть группы». Цитата ISO (`CT_VMerge`, через Microsoft Learn, LOW по seam): «If a vertically merged group of cells do not span the same set of grid columns, then the document is non-conformant.» Значит, продолжающие ячейки должны совпадать по столбцам с начальной.
- Рекомендуемая модель: каждая ячейка знает `startColumn`, `columnSpan`, `rowSpan`; продолжающие ячейки не выводятся отдельно. Так строки остаются выровненными по сетке (ROADMAP SC2: «пустые и объединённые ячейки не ломают строки»).
- Вложенные таблицы: `w:tc` содержит блоки (`w:p`, `w:tbl`, `w:sdt`) в порядке документа.

### Anti-Patterns to Avoid
- **Фильтровать пробельные токены** (`if (!xml.isWhitespace())`): удаляет значимые пробелы внутри `w:t` (проверено).
- **Показывать или логировать `xml.errorString()`**: при неизвестной сущности содержит имя из документа (проверено).
- **Пропускать DTD без ошибки**: внешняя сущность исчезает молча, это нарушает SAFE-04 (проверено).
- **Читать оба ветки `mc:AlternateContent`** (`mc:Choice` и `mc:Fallback`): текст текстового поля, вероятно, дублируется (`[ASSUMED]`, A2). Брать одну ветку, по правилам совместимости разметки (MCE) — первую поддерживаемую.
- **Читать `w:t` внутри `w:del`**: удалённый текст записан в `w:delText`, но пропуск всей ветки `w:del` надёжнее.
- **Регулярные выражения по XML**: ломаются на пространствах имён и сущностях (`.planning/research/STACK.md`).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Обработка DTD и сущностей | Собственный разбор `<!ENTITY>` или подстановка сущностей | Отклонить токен `DTD` | Реализация подстановки повторяет поверхность атаки; проверено, что Qt сам раскрывает внутренние сущности |
| Резолвер внешних сущностей | `QXmlStreamReader::setEntityResolver` | Не устанавливать | Резолвер по умолчанию не загружает внешние сущности (проверено: файл-маркер не читается), а свой резолвер открывает поверхность атаки (`.planning/research/PITFALLS.md`) |
| Декодирование кодировок | `QString::fromLatin1`, `toLocal8Bit()` | Декодирование `QXmlStreamReader` | Qt уважает объявление и BOM (проверено) |
| Распаковка ZIP | Свой парсер | `ZipArchive` (фаза 1) | Уже с лимитами и без путей |
| Метки нумерованных списков (roman, letters, пользовательские `lvlText`) | Полный движок нумерации Word | Маркер `•` и простые счётчики (v1) | Расширенная нумерация — дифференциатор, не v1 (`.planning/research/FEATURES.md`) |
| Перечень записей ZIP | Чтение имён через libzip напрямую вне класса | Метод `ZipArchive` (только имена) | Сохраняет инкапсуляцию pimpl в `ZipArchive.h` |

**Key insight:** проблемы разбора OOXML лежат в правилах Word, а не в XML. Самый дорогой риск — ложная уверенность в поведении Word; его снимают синтетические фикстуры, которые явно помечены как «воспроизведение нашего понимания», и ручная проверка на файле, сохранённом Word (Open Question 4).

## Common Pitfalls

### Pitfall 1: DTD пропущен — внешняя сущность исчезает молча, внутренняя раскрывается
**What goes wrong:** если отклонять DTD не делается, внешняя сущность даёт пустой текст без ошибки (потеря содержимого), а внутренние сущности раскрываются, в том числе вложенными.
**Why it happens:** `QXmlStreamReader` не сообщает об отсутствующем внешнем содержимом, а сущности раскрываются при чтении содержимого.
**How to avoid:** при первом токене `DTD` возвращать ошибку. Проверено на Qt 6.11.1.
**Warning signs:** в тестах текст `&xxe;` или `&secret;` пустой или раскрыт; `document.xml` содержит `<!DOCTYPE`.

Доказательство (проба, Qt 6.11.1, `probe-out.txt`):
```
== external SYSTEM entity (XXE) (bytes=431)
  DTD isDTD=1 text="<!DOCTYPE w:document [<!ENTITY xxe SYSTEM "file:///C:/.../external-secret.txt">]>"
  Start name="document" ns="http://schemas.openxmlformats.org/wordprocessingml/2006/main"
  ...
  END hasError=0 steps=13 textLength=0
```
Маркер из файла (`MARKER-EXTERNAL-7731`) в выводе отсутствует, но ошибки тоже нет: потеря молчаливая.

Вложенные сущности (проба): `== nested entities (billion-laughs shape, 3 levels)` даёт `textLength=300` (три уровня по десять). Проба показывает, что раскрытие выполняет сам Qt; при большем числе уровней размер растёт экспоненциально.

### Pitfall 2: Текст ошибки XML содержит содержимое документа
**What goes wrong:** `errorString()` для неизвестной сущности: `Entity 'nope' not declared.` (код 3). Имя сущности задаёт автор файла.
**How to avoid:** в `DocxError` передавать только код; не передавать `errorString()` в `DocumentImport` и в логи.
**Проба:** `ERROR code=3 line=1 col=167 errorString="Entity 'nope' not declared." errorContainsMarker=no` — маркер в тексте элемента не попал в `errorString`, но имя сущности попадает. Тест должен поместить маркер в имя сущности.

### Pitfall 3: Пробелы и разбиение текста
**What goes wrong:** пробел между словами в `w:t` удаляется фильтром пробельных токенов; текст из CDATA или нескольких токенов теряет часть.
**How to avoid:** склейка всех `Characters` внутри `w:t` (см. Pattern 3). Тест: `<w:t xml:space="preserve"> </w:t>` даёт пробел (проверено).

### Pitfall 4: Объявленная кодировка не UTF
**What goes wrong:** `encoding="windows-1251"` в части DOCX Qt декодирует корректно (проверено: «Тест»), то есть парсер «работает», хотя правило OPC требует UTF-8 или UTF-16 `[ASSUMED]`. Объявленная `UTF-8` с байтами cp1251 даёт `Encountered incorrectly encoded content.` (код 3, проверено).
**How to avoid:** решение владельца (Open Question 3). Рекомендация: отклонять любую объявленную кодировку, кроме UTF-8 и UTF-16, как `Corrupt`.

### Pitfall 5: Текстовые поля и колонтитулы
**What goes wrong:** колонтитулы лежат в `word/header*.xml` и `word/footer*.xml`, а не в трёх читаемых частях (D-10). Без перечня имён их нельзя ни прочитать, ни посчитать.
**How to avoid:** добавить в `ZipArchive` перечень имён записей (без чтения содержимого) и считать пропуски. Альтернатива — читать `word/_rels/document.xml.rels`, но это четвёртая часть и нарушение D-10 (Open Question 2).

### Pitfall 6: Текстовое поле дублируется в `mc:AlternateContent`
**What goes wrong:** текстовое поле задаётся двумя ветками: `mc:Choice` (DrawingML, `wps:txbxContent`) и `mc:Fallback` (VML). Чтение обеих даёт повторный текст `[ASSUMED]`.
**How to avoid:** в `mc:AlternateContent` брать только `mc:Choice`, при его отсутствии — `mc:Fallback` с предупреждением. Структура ветвления подтверждена описанием MCE; сам факт дублирования в файлах Word не проверен (A2).
**Source:** структура `Choice`/`Fallback` — [CITED: openxmldeveloper.org, статья про MCE, найдена поиском, не открыта; tier websearch MEDIUM по seam]. Факт дублирования — вывод `[ASSUMED]`.

### Pitfall 7: Отслеживаемые изменения
**What goes wrong:** `w:del` содержит `w:delText` (удалённый текст); `w:t` внутри `w:del` встречается редко, но его нельзя читать.
**How to avoid:** пропускать всю ветку `w:del` и `w:moveFrom`; `w:ins` и `w:moveTo` разворачивать. Пометка абзаца как удалённого (`w:pPr/w:rPr/w:del`) — отдельный случай, решение за планом (Open Q5).

### Pitfall 8: Объединённые ячейки
**What goes wrong:** строка с `vMerge continue` без начальной ячейки или группа с другой геометрией столбцов — некорректный документ (цитата ISO выше). Продолжающую ячейку нельзя выводить как обычную пустую, иначе строка «уезжает».
**How to avoid:** геометрия ячейки из `startColumn`/`columnSpan`; тест на `gridSpan` и на `vMerge` с пустыми продолжениями (DOCX-05, ROADMAP SC2).

### Pitfall 9: Лимит текста и предел глубины
**What goes wrong:** `kMaxTextChars` = 200000 объявлен в фазе 1, но не проверяется (`DocxTypes.h:47`: «D-03, enforced in Phase 2.»).
**How to avoid:** считать накопленный текст всех абзацев и ячеек; превышение — `DocxError::Limit` без частичного результата. Граница 200000 включается (тест в обе стороны, как `budgetBoundaryIsExact`).

### Pitfall 10: Тест на тексте сообщений при смене кодов
**What goes wrong:** `failureTextsArePairwiseDistinct` жёстко проверяет число текстов.
**Evidence:** `QCOMPARE(texts.size(), qsizetype{5});` (`tests/DocumentImportTest.cpp:179`); `DocumentImport.cpp:33–60` строит тексты по `switch` без `default`.
**How to avoid:** при добавлении кода `UnsafeXml` обновить и `switch`, и число в тесте одновременно; иначе тест падает, как и должен.

### Pitfall 11: Пути в bash на этой машине
**What goes wrong:** в Git Bash `g++` и Qt-DLL не находятся без `/c/msys64/mingw64/bin` в `PATH`; путь вида `C:/msys64/...` в `PATH` не работает.
**Evidence:** `which g++` не нашёл компилятор; `g++` с полным путём `/c/msys64/mingw64/bin/g++.exe` работает. AGENTS.md требует `C:/msys64/mingw64/bin` в `PATH` для `ctest`.
**How to avoid:** запускать сборку и тесты с `export PATH="/c/msys64/mingw64/bin:$PATH"` или из оболочки MSYS2 MINGW64.

### Pitfall 12: Расположение исполняемых тестов
**What goes wrong:** `.planning/codebase/TESTING.md` пишет путь `build/mingw-debug/tests/web_api_test`, но `CMakeLists.txt` создаёт цели в корне сборки (`qt_add_executable(zip_archive_test ...)` без каталога).
**How to avoid:** проверить путь после configure; в отчёте указывать фактический путь.

## Code Examples

### Namespace WordprocessingML (проверено пробой)
```
Start name="document" ns="http://schemas.openxmlformats.org/wordprocessingml/2006/main"
```
[VERIFIED: local probe, `probe-out.txt`] — значение строки `ns` в примерах должно совпадать с этим выводом.

### Токены и коды (проверено)
```cpp
// QXmlStreamReader::DTD, isDTD(): qxmlstream.h строки 245 и 298
// enum Error (qxmlstream.h 342-348): NoError, UnexpectedElementError, CustomError,
//   NotWellFormedError, PrematureEndOfDocumentError
// значения: NotWellFormedError=3, PrematureEndOfDocumentError=4 (probe2-out.txt)
```

### Текущие коды фазы 1 (верхняя граница того, что можно переиспользовать)
```cpp
// [VERIFIED: src/documents/DocxTypes.h:10-18]
enum class DocxError {
    NotZip,
    Corrupt,
    Encrypted,
    LegacyDoc,
    TooLarge,
    Limit,
    MissingPart,
};
// [VERIFIED: src/documents/DocxTypes.h:22-25]
enum class DocxWarning {
    StylesPartMissing,
    NumberingPartMissing,
};
// [VERIFIED: src/documents/DocxTypes.h:48]
inline constexpr qint64 kMaxTextChars = 200000;
```
Новые значения (`UnsafeXml`, пропуски `TextBoxSkipped`, `ImageSkipped`, `HeaderFooterSkipped`, `OtherSkipped`) — предложения `[ASSUMED]`, имена не проверены.

### Модель: рекурсивные контейнеры компилируются (проверено)
```cpp
struct Block;
struct Cell { QList<Block> blocks; };      // compile OK, g++ 16.1.0 + Qt 6.11.1
struct Row { QList<Cell> cells; };
struct Table { QList<Row> rows; };
struct Block { std::variant<QString, Table> content; };
```
Тот же вариант на `std::vector` тоже компилируется. [VERIFIED: local compile, `-fsyntax-only`, exit 0 для обоих вариантов]. Выбор контейнера — вкус проекта; `QList` подходит на этом тулчейне.

### Тесты: перехват сообщений Qt (API подтверждён)
```cpp
// QtCore/qlogging.h:198 [VERIFIED: C:/msys64/mingw64/include/qt6/QtCore/qlogging.h]
// Q_CORE_EXPORT QtMessageHandler qInstallMessageHandler(QtMessageHandler);
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Разбор `document.xml` регулярными выражениями | Потоковый XML-парсер с пространствами имён | — | Обязательно для вложенных таблиц и `sdt` (`.planning/research/STACK.md`) |
| Доверие к `DOCTYPE` и разрешению сущностей | Отклонение любого DTD | — | Закрывает XXE и billion-laughs (`.planning/research/PITFALLS.md`) |

**Deprecated/outdated:** ничего не требует замены в этой фазе. `QXmlStreamEntityResolver` в Qt 6 остаётся, но не используется.

## Assumptions Log

> Все `[ASSUMED]` утверждения. Перед блокирующим решением по каждому — подтверждение владельца или реальный файл, сохранённый Word/LibreOffice (не коммитится).

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Русский Word сохраняет встроенные заголовки с `styleId` `Heading1…9` и `w:name` `heading N`, а русское `Заголовок N` встречается лишь у части файлов | Pattern 3, DOCX-03 | Заголовки не распознаются в русских файлах; опора на `outlineLvl` смягчает риск |
| A2 | Текстовое поле в `mc:AlternateContent` дублируется в `mc:Choice` и `mc:Fallback`; чтение одной ветки убирает дубль | Pitfall 6, Anti-Patterns | Дубли текста или, наоборот, потеря текста при чтении одной ветки |
| A3 | Заголовочные стили несут `w:outlineLvl` в `w:pPr` | Pattern 3 | Меньше точных сопоставлений заголовков |
| A4 | `w:numId` со значением 0 означает «без нумерации» | Pattern 4 | Лишний маркер у абзацев без списка |
| A5 | `w:lvlText` маркера часто символ шрифта Symbol (PUA), его нельзя выводить как текст | Pattern 4 | Непечатаемые знаки в тексте резюме |
| A6 | `w:gridSpan` задаёт число занимаемых столбцов сетки | Pattern 5 | Неверная ширина строк |
| A7 | `w:del` содержит `w:delText`, пропуск всей ветки `w:del` безопасен | Pitfall 7 | Удалённый текст в выводе, если пропуск не полный |
| A8 | Поведение DTD-токена и раскрытие сущностей на Qt 6.7 (минимум в CMake) совпадает с 6.11.1 | Standard Stack | Расхождение поведения на минимальной версии; нужна проверка на 6.7 или поднятие минимума |
| A9 | Рекурсия по вложенным таблицам без предела опасна; предел 64 безопасен | Pattern 2 | Переполнение стека или ложные отказы |
| A10 | Правило OPC: XML-части только в UTF-8 или UTF-16; объявление другой кодировки нарушает его | Pitfall 4, Open Q3 | Неверный отказ на допустимом файле, либо принятие нарушения |
| A11 | Placeholder-текст `w:sdt` с `w:showingPlcHdr` следует пропускать | Pattern 2 | Подсказка «Введите текст» попадает в резюме |
| A12 | `w:tab` → `\t`, `w:br` → `\n` | Pattern 2 | Неверные разрывы строк |
| A13 | `w:numPr` в `w:pStyle` при отсутствии `numPr` в абзаце даёт нумерацию абзаца | Pattern 4 | Список без маркера или лишний маркер |
| A14 | `w:ptab` (позиционная табуляция) выводится как табуляция | OQ11 (план 02-03) | Выравнивание в тексте может отличаться от Word; текст не теряется |
| A15 | `w:sym` (символ шрифта) не выводится, пропуск считается как `OtherSkipped` | OQ11 (план 02-03) | Один символ не виден; пропуск не молчаливый |

**If this table is empty:** не пустая. Подтверждение владельца нужно по A1, A2, A10 до блокирующих решений.

## Open Questions

> Статусы: `RESOLVED` означает, что планы фазы приняли значение по умолчанию, указанное в статусе. Подтверждение владельца ожидается по каждому вопросу и не блокирует реализацию; вопросы для владельца фиксируются в журнале фазы (`docs/memory.md`, план 02-04).

1. **Смысл «вставок» в DOCX-08 и «текстовых полей» в SAFE-04.**
   - Что известно: DOCX-08 требует «гиперссылки и вставки показываются как текст»; ROADMAP SC4 относит «текстовые поля» к предупреждениям; DOCX-V2-03 — текстовые поля и колонтитулы как полноценное содержимое в v2.
   - Что неясно: «вставки» — это `w:sdt` (content controls), `w:smartTag`, `w:fldSimple` или текстовые поля `wps:txbxContent`?
   - Рекомендация: `w:sdt`, `w:smartTag`, `w:fldSimple`, сложные поля (только результат, не `w:instrText`) — **текст**; `w:txbxContent` (текстовые поля) — **пропуск с предупреждением**. Это соответствует ROADMAP SC4 и не противоречит DOCX-08. Решение владельца.
   - **Статус: RESOLVED** (по умолчанию, план 02-03, Task 3). Вставки `w:sdt`, `w:smartTag`, `w:fldSimple` и результат сложного поля дают текст; `w:txbxContent` даёт предупреждение `TextBoxSkipped`. Подтверждение владельца ожидается.

2. **Колонтитулы: как обнаружить и посчитать.**
   - Что известно: `header*.xml`/`footer*.xml` — отдельные части (`.planning/research/FEATURES.md`); `ZipArchive` не даёт перечень записей (`ZipArchive.h`, только `hasEntry`/`readEntry`); внутри `checkEntryStructure` имена уже читаются, но не сохраняются (`ZipArchive.cpp:40-48`).
   - Рекомендация: добавить `ZipArchive::entryNames()` (только имена, сохранённые при открытии); счётчик — число записей, совпадающих с `word/header*.xml` и `word/footer*.xml`. Альтернатива (чтение `document.xml.rels`) нарушает D-10.
   - **Статус: RESOLVED** (по умолчанию, план 02-04, Task 2). Счёт колонтитулов по точным именам `word/header<цифры>.xml` и `word/footer<цифры>.xml` через `ZipArchive::entryNames()`; `_rels` не читаются; содержимое колонтитулов не читается. Подтверждение владельца ожидается.

3. **Объявленные кодировки.**
   - Рекомендация: отклонять объявление, отличное от UTF-8 и UTF-16, как `Corrupt`. Поведение Qt (принимает windows-1251) проверено; правило OPC — вторичный источник (A10). Альтернатива — новый код `UnsupportedEncoding` (ещё одна строка текста для владельца).
   - **Статус: RESOLVED** (по умолчанию, план 02-01, Task 2). Допускаются объявления UTF-8 и UTF-16 (без учёта регистра) и отсутствие объявления; иное объявление даёт `DocxError::UnsupportedEncoding`, а не `Corrupt`. Подтверждение владельца по A10 ожидается.

4. **Русские имена стилей.**
   - Что известно: нет файла, сохранённого русским Word, в этой сессии; LibreOffice bug 115856 описывает локализованные имена встроенных стилей (французский пример `Titre 1`/`Titre1`).
   - Рекомендация: сопоставление по `styleId`, `outlineLvl`, английскому `heading N` и русскому `Заголовок N` (как дополнительное); ручная проверка на файле, созданном владельцем вне репозитория, в фазе 3 (TEST-02).
   - **Статус: RESOLVED** (по умолчанию, план 02-02, Tasks 1–2). Заголовок определяется по `styleId` `HeadingN`, по имени `heading N`, по русскому имени `Заголовок N` (дополнительное совпадение) и по `outlineLvl` 0–8 через цепочку `basedOn`. Прямой `outlineLvl` 9 в `w:pPr` абзаца и `outlineLvl` 9 в стиле цепочки останавливают обход и дают 0: явное «не заголовок» переопределяет базовый стиль, как в Word. Проверено только на синтетике (A1); подтверждение владельца ожидается, проверка на файле Word — TEST-02 в фазе 3.

5. **Нумерация: показывать ли номера.**
   - Что известно: DOCX-04 требует «маркированные и нумерованные списки с вложенностью»; Word не хранит метки.
   - Рекомендация: маркер `•` и номер-счётчик по паре `(numId, ilvl)` с перезапуском при более высоком уровне; точные правила Word — `[ASSUMED]` (A13). Расширенные форматы — deferred.
   - **Статус: RESOLVED** (по умолчанию, план 02-02, Tasks 1 и 3). Маркер «•» для `bullet`; для остальных форматов номер «N.». Формат, который не воспроизводится (не `bullet` и не `decimal`, либо `decimal` с текстом метки уровня, отличным от «%1.»), даёт одно предупреждение `ListFormatApproximated` на список (numId). Подтверждение владельца ожидается.

6. **Форма API импорта.**
   - Что известно: `ImportedDocx` сейчас хранит байты (`DocumentImport.h:12-17`); тесты фазы 1 сравнивают `documentXml`, `stylesXml`, `numberingXml` (`tests/DocumentImportTest.cpp:72-74`).
   - Рекомендация: добавить поле модели в `ImportedDocx` и оставить три поля байтов до фазы 3 (меньше правок тестов; память до 20 MiB, уже учтена лимитом). Либо новая функция `readDocxDocument` и отдельный путь. Решение планировщика.
   - **Статус: RESOLVED** (по умолчанию, план 02-04, Task 1). Поле `document` добавлено в `ImportedDocx`; поля `documentXml`, `stylesXml`, `numberingXml` сохранены до фазы 3. Подтверждение владельца ожидается.

7. **Предел глубины вложенности и код его превышения.**
   - Рекомендация: 64, превышение — `Limit` (`[ASSUMED]`, A9).
   - **Статус: RESOLVED** (по умолчанию, план 02-03, Tasks 2–3). Предел 64 по общему счётчику счётных элементов (`w:tbl`, `w:sdt`, `w:customXml`, `w:hyperlink`, `w:ins`, `w:moveTo`, `w:smartTag`, `w:fldSimple`, выбранная ветка `mc:AlternateContent`); превышение — `DocxError::Limit`. Безопасность предела (A9) не проверялась отдельно; подтверждение владельца ожидается.

8. **Тексты для новых кодов.**
   - Рекомендация: тексты пишутся в фазе 2 (для тестов точного текста), владелец проверяет в фазе 3 вместе с остальными русскими текстами (`01-VERIFICATION.md`, владелец принял решение о проверке в фазе 3).
   - **Статус: RESOLVED** (по умолчанию, планы 02-01 — 02-04). `UnsafeXml` и `UnsupportedEncoding` — план 02-01; `ListFormatApproximated` — 02-02; `ImageSkipped`, `TextBoxSkipped`, `OtherSkipped` — 02-03; `HeaderFooterSkipped` и текст `Limit` для длины текста — 02-04. Владелец проверит их в фазе 3; подтверждение ожидается.
9. **Предел числа узлов модели.** Сколько абзацев, таблиц, строк и ячеек допустимо? Байтовый бюджет не ограничивает число пустых `<w:p/>` (около 3,5 млн при 20 МиБ).
   - Рекомендация: `kMaxModelNodes` = 50000, превышение — `DocxError::Limit`; счётчик общий на вызов `readDocxContent` и учитывает дополнительные ячейки до ширины сетки.
   - **Статус: RESOLVED** (по умолчанию, план 02-01, Task 3; план 02-03, Task 2). Значение — решение планировщика; подтверждение владельца ожидается.
10. **Части `word/comments.xml`.** Считать ли их и как?
   - Рекомендация: не читать; одно предупреждение `OtherSkipped` на часть, по аналогии с колонтитулами.
   - **Статус: RESOLVED** (по умолчанию, план 02-04, Task 2). Реализация не ждёт подтверждения владельца; текст `OtherSkipped` проверит владелец в фазе 3.
11. **Символьные элементы в тексте.** `w:noBreakHyphen`, `w:softHyphen`, `w:ptab`, `w:sym` дают видимый текст или пропуск.
   - Рекомендация: `w:noBreakHyphen` → U+2011, `w:softHyphen` → U+00AD, `w:ptab` → табуляция (A14); `w:sym` не выводит символ и даёт один `OtherSkipped` (A15): глиф символьного шрифта без таблицы шрифтов не восстанавливается.
   - **Статус: RESOLVED** (по умолчанию, план 02-03, Task 3). Подтверждение владельца ожидается; A14 и A15 проверены только синтетикой.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Qt 6.11.1 (QtCore: QXmlStreamReader, qlogging) | разбор XML, тесты | ✓ | 6.11.1 (`qVersion()` в пробе) | — |
| g++ (MSYS2 MinGW-w64) | сборка | ✓ | 16.1.0 (`g++.exe (Rev5, Built by MSYS2 project) 16.1.0`) | — |
| CMake | configure | ✓ | `C:/Program Files/CMake/bin/cmake` (версия не записана) | `C:/msys64/mingw64/bin/cmake.exe` присутствует |
| Ninja | генератор пресета | ✓ (присутствует в `C:/msys64/mingw64/bin`) | версия не проверена | — |
| libzip | фаза 1, `ZipArchive` | ✓ | 1.11.4-1 (`mingw-w64-x86_64-libzip-1.11.4-1` в `pacman` local DB) | — |
| clang-format | `--dry-run --Werror` | ✓ (присутствует) | версия не проверена | — |
| clang-tidy | `-p build/mingw-debug` | ✓ (присутствует) | версия не проверена | — |
| Node.js + Chrome (`web_e2e`) | не нужны для фазы 2 | не проверялось | — | `web_e2e` в ctest не регистрируется без `tests/e2e/node_modules` (`CMakeLists.txt`) |

**Missing dependencies with no fallback:** нет.
**Missing dependencies with fallback:** нет.

Пробы в этой сессии скомпилированы и запущены вне дерева проекта (scratch-каталог, см. Sources). Сборка `cmake --build --preset debug` и `ctest --preset debug` в этой сессии **не запускались**; состояние дерева на момент фазы 1 — по `01-VERIFICATION.md` (зафиксировано там, а не здесь).

## Validation Architecture

> `workflow.nyquist_validation` в `.planning/config.json` не задан (ключ отсутствует) — раздел включён.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Qt Test (`Qt6::Test`) 6.11.1 + CTest |
| Config file | `CMakeLists.txt` (блок `enable_testing()`, `qt_add_executable`, `add_test`) |
| Quick run command | `cmake --build --preset debug && ctest --preset debug -R docx_content` |
| Full suite command | `cmake --build --preset debug && ctest --preset debug` |
| Test executable | `docx_content_test` (расположение проверить после configure, Pitfall 12) |

### Phase Requirements → Test Map

> Авторитетные имена тестов — в PLAN-файлах 02-01…02-04 после ревизии. Имена в таблице ниже — предложения исследования и могут не совпадать (например, `skippedContentCountedAsWarnings` и `errorsAndLogsDoNotContainMarker` в планах разбиты по элементам и по тестам `messagesDoNotCarryMarker`, `messagesAndLogsCarryNoMarker`).
| Req ID | Behavior | Test Type | Automated Command (предложение) | File Exists? |
|--------|----------|-----------|----------------------------------|--------------|
| DOCX-02 | абзацы и таблицы в порядке потока | unit | `ctest --preset debug -R docx_content` (`paragraphsKeepDocumentOrder`) | ❌ Wave 0 |
| DOCX-03 | заголовок по `styleId`, `name` `heading N`, `outlineLvl`; русское `Заголовок N` — тест собственного сопоставления | unit, data-driven | `... -R docx_content` (`headingByStyleIdNameOrOutline`) | ❌ Wave 0 |
| DOCX-04 | маркер и номер, вложенность `ilvl` 0–2, `numId` 0, `numPr` в стиле | unit, data-driven | `... -R docx_content` (`listsKeepKindAndLevel`) | ❌ Wave 0 |
| DOCX-05 | строки сохраняют ширину сетки; `gridSpan`; `vMerge` без дублей; пустая ячейка; вложенная таблица | unit | `... -R docx_content` (`tableRowsKeepGridWidth`, `vMergeNotDuplicated`) | ❌ Wave 0 |
| DOCX-06 | кириллица в тексте точно равна ожидаемой строке; cp1251 и UTF-16 BOM по решению | unit | `... -R docx_content` (`cyrillicTextExact`) | ❌ Wave 0 |
| DOCX-07 | изображение пропущено, соседний текст цел, счётчик | unit | `... -R docx_content` (`drawingSkippedWithWarning`) | ❌ Wave 0 |
| DOCX-08 | `hyperlink`, `ins`, `sdtContent`, `fldSimple` дают текст; `del` и `delText` нет; `instrText` нет | unit, data-driven | `... -R docx_content` (`inlineWrappersAndTracking`) | ❌ Wave 0 |
| SAFE-03 | DOCTYPE с внутренним и внешним подмножеством — отказ; внешняя сущность не читается (файл-маркер); billion-laughs отклонён до раскрытия | unit, data-driven | `... -R docx_content` (`doctypeRejected`, `externalEntityNeverRead`) | ❌ Wave 0 |
| SAFE-04 | число пропусков: текстовое поле, колонтитул, изображение; размер списка = число экземпляров | unit | `... -R docx_content` (`skippedContentCountedAsWarnings`) | ❌ Wave 0 |
| SAFE-05 | маркер (в тексте, в имени сущности, в атрибуте) не попадает в сообщения и логи | unit + message handler | `... -R docx_content` (`errorsAndLogsDoNotContainMarker`) | ❌ Wave 0 |
| TEST-01 | все перечисленные вредоносные случаи (отказ, лимит, битый XML, кодировка) | unit | `ctest --preset debug` | ❌ Wave 0 |
| — (ROADMAP SC3) | лимит текста: 200000 включительно — ок, 200001 — `Limit` | unit | `... -R docx_content` (`textLimitBoundary`) | ❌ Wave 0 |

### Sampling Rate
- **Per task commit:** `ctest --preset debug -R docx_content` (после `cmake --build --preset debug`).
- **Per wave merge:** `ctest --preset debug` (все пять текущих тестов плюс `docx_content`).
- **Phase gate:** полный `ctest --preset debug` зелёный; `clang-format --dry-run --Werror` по новым файлам; `clang-tidy -p build/mingw-debug` по новым `.cpp` (при наличии compile database). Отчёт — только реальные результаты.

### Wave 0 Gaps
- [ ] `tests/DocxContentTest.cpp` — основной набор, класс `DocxContentTest : public QObject` с `Q_OBJECT` и `private slots:` (конвенция `tests/ZipArchiveTest.cpp`).
- [ ] `tests/DocxFixtures.h` — новые билдеры: `buildDocxParts(document, styles, numbering)`, сборка `word/styles.xml` и `word/numbering.xml` с нужными стилями и уровнями. Строки XML — через `QStringLiteral(...).toUtf8()` (исходники UTF-8, как в фазе 1).
- [ ] `CMakeLists.txt` — `qt_add_executable(docx_content_test tests/DocxContentTest.cpp)`, `target_link_libraries(... resume_documents Qt6::Test)` или `resume_core`, `add_test(NAME docx_content COMMAND docx_content_test)`; новые `.cpp` в `add_library(resume_documents ...)`.
- [ ] `tests/DocumentImportTest.cpp:179` — обновить число текстов при добавлении кода ошибки.
- [ ] `src/documents/README.md` — таблицы кодов и правил слоя.
- [ ] Фикстуры не коммитятся бинарно (Pitfall 10 фазы 1); синтетический XML — в коде. Проверка реальных файлов Word — вне репозитория (TEST-02 в фазе 3).

*(Пробелы фреймворка нет: Qt Test и CTest уже работают, фаза 1 подтвердила 5/5 тестов.)*

**Ограничение синтетических фикстур:** фикстура проверяет наше понимание разметки Word, а не то, что записывает Word. Поэтому тесты на русские имена стилей и на `mc:AlternateContent` помечаются в отчёте как «проверка парсера на нашей фикстуре», не как «проверка на файле Word» (A1, A2).

**Мутационная проверка (рекомендация):** как в фазе 1 — временно отключить проверку DTD, фильтр пробелов, склейку `Characters`, пропуск `w:del`; тест соответствующего правила обязан упасть. Файл восстанавливается из резервной копии, в коммит не попадает (`docs/memory.md`, журнал 01-04).

## Security Domain

> `security_enforcement` не задан в `config.json` — раздел обязателен (по умолчанию включён). Уровень ASVS 1, блокировка от `high`.

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|------------------|
| V2 Authentication | нет | — (фаза не работает с аутентификацией) |
| V3 Session Management | нет | — |
| V4 Access Control | нет | — |
| V5 Input Validation | да | отказ по DTD, по ошибке XML, по лимиту текста и глубины; кодировка по решению |
| V6 Cryptography | нет | шифрование обрабатывается фазой 1 (`Encrypted`), криптографии здесь нет |
| V7 Error Handling and Logging | да | коды без текста; `errorString()` не передаётся; тест SAFE-05 |

### Known Threat Patterns for C++/Qt XML-разбора

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| XXE: внешняя сущность читает локальный файл (CWE-611) | Information disclosure | Отказ по токену `DTD` до содержимого; резолвер не устанавливается; тест с файлом-маркером |
| Billion laughs: раскрытие вложенных сущностей (CWE-776) | Denial of service | Отказ по `DTD` до раскрытия (проверено: раскрытие делает сам Qt при ссылке) |
| Глубокое вложение `tbl`/`sdt` (CWE-674) | Denial of service | Предел глубины (предложение 64), `Limit` |
| Огромный текст при допустимом размере файла | Denial of service | `kMaxTextChars` при накоплении, `Limit` |
| Текст резюме в сообщении или логе (CWE-209) | Information disclosure | Коды; тексты в `DocumentImport`; `qInstallMessageHandler` в тесте |
| Смешение кодировок (неверное декодирование) | Tampering (искажение данных) | Декодирование Qt; отказ по объявлению вне UTF-8/16 (решение владельца) |
| Ссылки на внешние ресурсы в `_rels` (гиперссылки, изображения) | SSRF/утечка | `_rels` не читаются; гиперссылки показываются только текстом |

## Project Constraints (from CLAUDE.md / AGENTS.md)

- Читать `docs/plan.md` и `docs/memory.md` перед задачей; после значимой задачи обновить `docs/memory.md` (CLAUDE.md, AGENTS.md раздел 4).
- Использовать `.claude/skills/write-tests/SKILL.md` для тестов и `.claude/skills/review-changes/SKILL.md` для ревью (CLAUDE.md).
- Правила `.claude/rules/*.md` применять только к совпадающим путям; `llm-security.md` относится к `src/llm/**` и к этой фазе не относится (`src/llm` не трогается).
- C++23, нельзя понижать; Qt 6 Widgets, CMake, Qt Test; слои UI → domain/application → storage/LLM/documents, бизнес-логика не в QWidget (AGENTS.md раздел 1, 3).
- Классы PascalCase, методы camelCase, RAII; `k`-префикс для констант (CONVENTIONS, `.planning/codebase/CONVENTIONS.md`).
- Все SQL с пользовательскими данными параметризованы (в этой фазе SQL не используется).
- Ошибки без секретов и приватных данных (AGENTS.md раздел 3; SAFE-05).
- Синтетические тестовые данные; реальные резюме и персональные данные не коммитить (AGENTS.md раздел 4).
- Команды проверки: `cmake --build --preset debug`, `ctest --preset debug`, `clang-format --dry-run --Werror <файлы>`, `clang-tidy <файл> -p build/mingw-debug` (AGENTS.md раздел 2). Не утверждать, что тесты прошли, без запуска.
- `C:/msys64/mingw64/bin` в `PATH` для запуска тестов (AGENTS.md раздел 2).
- Коммит — в отдельную ветку, в `main` только через PR; каталоги сборки не коммитить (AGENTS.md раздел 4).

## Recommended Plan Split (для планировщика)

Предлагается 4 последовательных плана (как в фазе 1, потому что все меняют общие `CMakeLists.txt` и `src/documents/`):

1. **02-01 Guard и ядро разбора:** коды (`UnsafeXml` и пр.), отказ по DTD, `hasError` → `Corrupt`, пространства имён, склейка текста, кодировка по решению; тесты SAFE-03, SAFE-05 (частично), DOCX-06.
2. **02-02 Стили и нумерация:** заголовки (`styleId`, `name`, `outlineLvl`), списки (`numPr`, `numFmt`, вложенность, стиль с `numPr`); тесты DOCX-03, DOCX-04.
3. **02-03 Таблицы и встроенное содержимое:** `tblGrid`, `gridSpan`, `vMerge`, вложенные таблицы; `hyperlink`, `ins`, `del`, `sdt`, `fldSimple`, пропуски `drawing`/`txbxContent`; счётчики пропусков; `kMaxTextChars`; тесты DOCX-05, DOCX-07, DOCX-08, SAFE-04.
4. **02-04 Сервис и интеграция:** `ZipArchive::entryNames()` для колонтитулов (если решено), `ImportedDocx` с моделью, русские тексты новых кодов, обновление `DocumentImportTest` и `README`; сквозной тест маркера (SAFE-05, TEST-01); гейт (`ctest`, `clang-format`, `clang-tidy`).

## Sources

### Primary (HIGH — проверено в этой сессии)
- **Пробы на Qt 6.11.1** (scratch, вне репозитория; компиляция `g++ -std=c++23` с `-I/c/msys64/mingw64/include/qt6`, `-lQt6Core`):
  - `C:\Users\Apostol\AppData\Local\Temp\claude\C--Users-Apostol-CLionProjects-AI-Resume-Assistant\a5cfe561-8642-4bbd-ae69-92593d7951f3\scratchpad\xmlprobe\probe.cpp` и вывод `probe-out.txt` в той же папке: DTD, сущности, кодировки, пространства имён, сообщения об ошибках.
  - `...\scratchpad\xmlprobe\probe2.cpp` и `probe2-out.txt`: пробельные токены, разбиение CDATA, значения `QXmlStreamReader::Error`.
  - `...\scratchpad\xmlprobe\recursive_qlist.cpp` и `recursive_vector.cpp`: компиляция `-fsyntax-only`, exit 0 для обоих.
- **Локальные заголовки Qt 6.11.1:** `C:/msys64/mingw64/include/qt6/QtCore/qxmlstream.h` — строки 245–246 (`DTD`, `EntityReference`), 298–299 (`isDTD`, `isEntityReference`), 342–348 (`enum Error`); `C:/msys64/mingw64/include/qt6/QtCore/qlogging.h:198` — `qInstallMessageHandler`.
- **Репозиторий (прочитано в этой сессии):**
  - `src/documents/DocxTypes.h` (строки 10–18, 22–25, 31–48) — коды и лимиты, значения выше в Code Examples.
  - `src/documents/DocxContainer.h` (строки 11–17, 25–26) — `DocxParts`, сигнатура `readDocxParts`; комментарий «the size is the count».
  - `src/documents/DocxContainer.cpp` (строки 46–112) — классификация и три части.
  - `src/documents/ZipArchive.h` (строки 14–32) — API без перечня имён; `src/documents/ZipArchive.cpp` (строки 33–50, 54–59) — `checkEntryStructure`, имена не сохраняются.
  - `src/application/DocumentImport.h` (строки 8–22) и `DocumentImport.cpp` (строки 33–60, 79–118) — `ImportedDocx`, тексты, `switch` без `default`.
  - `tests/DocumentImportTest.cpp` (строки 53–76, 179) — Cyrillic-тест и `QCOMPARE(texts.size(), qsizetype{5})`.
  - `tests/ZipArchiveTest.cpp`, `tests/DocxFixtures.h` — структура тестов и билдеры.
  - `CMakeLists.txt` — `add_library(resume_documents STATIC ...)`, блок тестов (`zip_archive`, `document_import`).
  - `.planning/REQUIREMENTS.md`, `.planning/ROADMAP.md`, `.planning/STATE.md`, `.planning/PROJECT.md`, `.planning/config.json`, `.planning/phases/01-.../01-VERIFICATION.md`, `docs/memory.md`, `AGENTS.md`, `CLAUDE.md`.
- **Инструменты:** `g++ --version` (16.1.0), `pacman` local DB (libzip 1.11.4-1), наличие `cmake`, `ninja`, `clang-format`, `clang-tidy` в `C:/msys64/mingw64/bin`.

### Secondary (внешние; tier по `gsd-tools query classify-confidence`)
- [python-docx: BabelFish, `style_aliases`](https://raw.githubusercontent.com/python-openxml/python-docx/master/src/docx/styles/__init__.py) — открыт через WebFetch; пары `Heading 1` ↔ `heading 1` и т. д. Tier провайдера `webfetch`: **LOW** (seam). Использовано для факта «внутреннее имя `heading N`».
- [Microsoft Learn: VerticalMerge (ISO/IEC 29500-1 remarks)](https://learn.microsoft.com/en-us/dotnet/api/documentformat.openxml.wordprocessing.verticalmerge) — открыт через WebFetch; семантика `restart`/`continue`, требование одинаковых столбцов. Tier `webfetch`: **LOW** (seam). Перед блокирующим решением сверить со спецификацией.
- [Microsoft Learn: NumberingProperties (ISO/IEC 29500-1 remarks)](https://learn.microsoft.com/en-us/dotnet/api/documentformat.openxml.wordprocessing.numberingproperties) — открыт через WebFetch; правило про `ilvl` в стилях. Tier `webfetch`: **LOW** (seam).
- [LibreOffice bug 115856 (локализованные имена встроенных стилей)](https://bugs.libreoffice.org/show_bug.cgi?id=115856) — найден поиском, не открыт. Tier `websearch --verified`: **MEDIUM** (seam). Использовано только как «локализованные имена существуют».
- [ericwhite.com: Markup Compatibility, Open XML SDK](https://www.ericwhite.com/blog/?p=2711) и [openxmldeveloper.org: MCE](https://openxmldeveloper.org/blog/b/openxmldeveloper/archive/2015/02/02/screen-cast-using-the-markup-compatibility-extensibility-feature-of-the-open-xml-sdk.html) — найдены поиском, не открыты. Tier `websearch --verified`: **MEDIUM**. Использовано для структуры `Choice`/`Fallback`; факт дублирования текста — `[ASSUMED]` (A2).
- Правило OPC о кодировках XML-частей (UTF-8 или UTF-16) — поиск, вторичные цитаты (igniteui, Wikipedia-подобные описания); первоисточник ECMA-376 Part 2 не открыт. Tier: **MEDIUM** по seam, но содержание вторичное — поэтому в тексте помечено `[ASSUMED]` (A10).

### Tiers, полученные из seam, и расхождение
- `classify-confidence --provider webfetch` → LOW (с `--verified` тоже LOW); `websearch --verified` → MEDIUM; `context7` → MEDIUM.
- Провайдеров для локальных проб (`local`, `codebase`, `probe`) в seam нет: они возвращают LOW. Пробы в этом отчёте помечены `[VERIFIED: local probe]` по правилу провенанса (запуск, вывод приведён дословно), а не по tier seam. Это расхождение отмечено в итоговом сообщении.

## Metadata

**Confidence breakdown:**
- Поведение QXmlStreamReader (DTD, сущности, пробелы, кодировки, ошибки, пространства имён): **HIGH** для Qt 6.11.1 — проверено пробами; **[ASSUMED]** для минимума 6.7 (A8).
- Структура модели и геометрия таблиц: **MEDIUM** — правила из спецификации по вторичным цитатам Microsoft Learn (LOW по seam).
- Правила Word (русские имена стилей, текстовые поля в `mc:AlternateContent`, колонтитулы в файле): **LOW** — требуют файла, сохранённого Word (`[ASSUMED]`, A1, A2).
- Лимиты и пределы (глубина 64, текст 200000): **MEDIUM** — текст 200000 из репозитория; глубина — предложение.

**Research date:** 2026-10-24
**Valid until:** 30 дней для поведения Qt 6.11.1 (стабильный API); правила Word до проверки на реальном файле не считаются действующими.
