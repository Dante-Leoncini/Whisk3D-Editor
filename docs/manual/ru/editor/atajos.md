# Горячие клавиши

Все функции из таблиц ниже можно переназначить, и у каждой может быть **одно или несколько** сочетаний (или ни одного). Это работает на ПК, Android (с физической клавиатурой) и Web. На N95 окно не может перехватывать клавиши: там файл `atajos.ini` редактируется вручную.

## Как изменить сочетание клавиш

1. Открыть **Свойства → вкладка Файл/Рендер → карточка Настройки → Сочетания клавиш…**
2. Выбрать **Контекст** (Общие, 3D-вьюпорт, Во время трансформации, UV-редактор, Таймлайн, Аутлайнер, 2D-редактор, Свойства, Редактор скриптов). Стрелки влево/вправо тоже его переключают.
3. Выбрать функцию в списке и нажать **Добавить сочетание** (или ещё раз коснуться выбранной строки). Появится "Нажмите клавишу…".
4. Нажать новое сочетание (например, <kbd>Ctrl+M</kbd>). <kbd>Esc</kbd> отменяет, ничего не меняя.

| Кнопка | Что делает |
|---|---|
| Добавить сочетание | Добавляет сочетание к функции. Если оно было у другой функции того же контекста, оно у неё снимается, и выводится предупреждение. |
| Очистить | Оставляет функцию без сочетания. |
| Сбросить | Возвращает функции её сочетание по умолчанию. |
| Сбросить всё | Все функции возвращаются к сочетаниям по умолчанию. |

Меню показывают рядом с каждым пунктом **настроенное** сочетание: если ты переназначишь функцию, меню покажет новое сочетание. На N95 меню показывают клавишу телефона (например, <kbd>1</kbd> для «Переместить» или <kbd>C</kbd> для «Удалить»); если подключена Bluetooth-клавиатура, они показывают клавишу этой клавиатуры, когда могут её распознать.

Изменённые сочетания выделяются зелёным. Если у функции убрать сочетание по умолчанию, эта клавиша освобождается: в этом контексте она больше ничего не делает.

Сочетания каждого контекста действуют, когда указатель находится над этой панелью; сочетания **Общие** действуют везде. Сочетания **Во время трансформации** действуют, пока что-то перемещается, вращается или масштабируется, и имеют приоритет. В Редакторе скриптов буквы всегда вводят текст: там можно переназначать только сочетания с <kbd>Ctrl</kbd>, <kbd>Alt</kbd> или F-клавишами.

## Файл shortcuts.ini

Изменения сохраняются автоматически в `res/shortcuts.ini`, рядом с `config.ini`. В нём есть только изменённые функции, и его можно редактировать вручную:

```ini
# функция = сочетание, другое сочетание   (пусто = без сочетания)
3d.mover = G, Ctrl+M
3d.ocultar = Ctrl+Q
xf.cancelar = Esc, Q
3d.overlays =
```

Имена клавиш: буквы и цифры как есть, `Enter`, `Esc`, `Tab`, `Space`, `Backspace`, `Delete`, `Home`, `End`, `PageUp`, `PageDown`, `Left`, `Right`, `Up`, `Down`, `F1`…`F12`, `Num0`…`Num9`, `Num.`, `NumEnter`, `Comma`, `Plus`. Модификаторы: `Ctrl+`, `Shift+`, `Alt+`.

## Сочетания по умолчанию

<!-- Таблицы ниже, до ATAJOS-FIN, генерирует tools/genmanual_atajos.py из кода: не редактируй их вручную. -->

<!--ATAJOS-INICIO-->

### Общие (любая панель)

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в shortcuts.ini |
|---|---|---|---|
| Сохранить проект | <kbd>Ctrl+S</kbd> | — | `global.save` |
| Отменить | <kbd>Ctrl+Z</kbd> | <kbd>C+Влево</kbd> | `global.undo` |
| Повторить | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | <kbd>C+Вправо</kbd> | `global.redo` |
| Объект / Редактирование | <kbd>Tab</kbd> | <kbd>OK</kbd> | `global.mode` |
| Вкл/выкл привязку | <kbd>Shift+Tab</kbd> | — | `global.snap` |
| Меню привязки | <kbd>Shift+S</kbd> | — | `global.menuSnap` |
| Пуск / Пауза | <kbd>Space</kbd> | <kbd>Зелёная+Лев. софт</kbd> | `global.play` |
| Строка меню | <kbd>M</kbd> | <kbd>Лев. софт</kbd> | `global.menuBar` |
| Родитель | <kbd>Ctrl+P</kbd> | — | `global.parent` |
| Снять родителя | <kbd>Alt+P</kbd> | — | `global.clearParent` |
| Полный экран | <kbd>F11</kbd> | — | `global.screen` |
| Рендер изображения | <kbd>F12</kbd> | — | `global.render` |
| Рендер анимации | <kbd>Ctrl+F12</kbd> | — | `global.renderAnim` |

### 3D-вьюпорт

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в shortcuts.ini |
|---|---|---|---|
| Выделить всё | <kbd>A</kbd> | <kbd>Карандаш+Вверх</kbd> | `3d.selectAll` |
| Снять всё выделение | <kbd>Alt+A</kbd> | <kbd>Карандаш+Вверх</kbd> | `3d.deselect` |
| Инвертировать выделение | <kbd>Ctrl+I</kbd> | — | `3d.invert` |
| Выделение рамкой | <kbd>B</kbd> | — | `3d.box` |
| Выделить связанное | <kbd>L</kbd> | — | `3d.linked` |
| Добавить | <kbd>Shift+A</kbd> | — | `3d.add` |
| Применить | <kbd>Ctrl+A</kbd> | — | `3d.apply` |
| Переместить | <kbd>G</kbd> | <kbd>1</kbd> | `3d.move` |
| Вращать | <kbd>R</kbd> | <kbd>2</kbd> | `3d.rotate` |
| Масштаб | <kbd>S</kbd> | <kbd>3</kbd> | `3d.scale` |
| Сбросить положение (поза) | <kbd>Alt+G</kbd> | — | `3d.clearPos` |
| Сбросить вращение (поза) | <kbd>Alt+R</kbd> | — | `3d.clearRot` |
| Сжать/раздуть / Сбросить масштаб | <kbd>Alt+S</kbd> | — | `3d.clearScale` |
| Дублировать | <kbd>Shift+D</kbd> | — | `3d.duplicate` |
| Дублировать связанно | <kbd>Alt+D</kbd> | — | `3d.instance` |
| Удалить | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `3d.delete` |
| Скрыть / Показать | <kbd>H</kbd> | — | `3d.hide` |
| Объединить | <kbd>Ctrl+J</kbd> | — | `3d.join` |
| Вставить ключевой кадр | <kbd>I</kbd> | — | `3d.keyframe` |
| Вставить ключевой кадр (всё) | <kbd>Shift+I</kbd> | — | `3d.keyframeAll` |
| Пропорциональное редактирование | <kbd>O</kbd> | — | `3d.proportional` |
| Наложения | <kbd>K</kbd> | — | `3d.overlays` |
| Локальный вид | <kbd>/</kbd> | — | `3d.viewLocal` |
| Показать выделенное | <kbd>Num.</kbd> | <kbd>0</kbd> | `3d.focus` |
| Вид спереди | <kbd>Num1</kbd> | <kbd>#+Влево/Вправо</kbd> | `3d.viewFront` |
| Вид сзади | <kbd>Ctrl+Num1</kbd> | <kbd>#+Влево/Вправо</kbd> | `3d.viewBack` |
| Вид справа | <kbd>Num3</kbd> | <kbd>#+Влево/Вправо</kbd> | `3d.viewRight` |
| Вид слева | <kbd>Ctrl+Num3</kbd> | <kbd>#+Влево/Вправо</kbd> | `3d.viewLeft` |
| Вид сверху | <kbd>Num7</kbd> | <kbd>#+Вверх</kbd> | `3d.viewTop` |
| Вид снизу | <kbd>Ctrl+Num7</kbd> | <kbd>#+Вниз</kbd> | `3d.viewBottom` |
| Перспектива / Орто | <kbd>Num5</kbd> | <kbd>#</kbd> | `3d.perspective` |
| Крен влево | <kbd>Num4</kbd> | — | `3d.rollLeft` |
| Крен вправо | <kbd>Num6</kbd> | — | `3d.rollRight` |
| Вид из камеры | <kbd>Num0</kbd> | <kbd>8</kbd> | `3d.viewCamera` |
| Сделать активной камерой | <kbd>Ctrl+Num0</kbd> | — | `3d.activeCamera` |
| Выдавить | <kbd>E</kbd> | <kbd>7</kbd> | `3d.extrude` |
| Новая грань из вершин | <kbd>F</kbd> | — | `3d.face` |
| Пересчитать нормали | <kbd>Shift+N</kbd> | — | `3d.normals` |
| Петлевой разрез и сдвиг | <kbd>Ctrl+R</kbd> | <kbd>8</kbd> | `3d.loopcut` |
| Соединить путь вершин | <kbd>J</kbd> | — | `3d.connect` |
| Разорвать | <kbd>V</kbd> | — | `3d.rip` |
| Сдвиг вершин | <kbd>Shift+V</kbd> | — | `3d.slide` |
| Разделить | <kbd>P</kbd> | — | `3d.separate` |
| Слить | <kbd>M</kbd> | — | `3d.merge` |
| Контекстное меню | <kbd>W</kbd> | — | `3d.contextMenu` |
| Триангулировать | <kbd>Ctrl+T</kbd> | — | `3d.triangulate` |
| Меню UV | <kbd>U</kbd> | — | `3d.menuUV` |

### Во время трансформации (перемещение / вращение / масштабирование)

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в shortcuts.ini |
|---|---|---|---|
| Подтвердить | <kbd>Enter</kbd>, <kbd>NumEnter</kbd> | <kbd>OK</kbd> | `xf.confirm` |
| Отмена | <kbd>Esc</kbd> | <kbd>C</kbd> | `xf.cancel` |
| Ось X | <kbd>X</kbd> | <kbd>1</kbd> | `xf.axisX` |
| Ось Y | <kbd>Y</kbd> | <kbd>2</kbd> | `xf.axisY` |
| Ось Z | <kbd>Z</kbd> | <kbd>3</kbd> | `xf.axisZ` |
| Исключить X (плоскость) | <kbd>Shift+X</kbd> | — | `xf.planeX` |
| Исключить Y (плоскость) | <kbd>Shift+Y</kbd> | — | `xf.planeY` |
| Исключить Z (плоскость) | <kbd>Shift+Z</kbd> | — | `xf.planeZ` |
| Пропорциональный радиус + | <kbd>PageUp</kbd> | — | `xf.radiusPlus` |
| Пропорциональный радиус - | <kbd>PageDown</kbd> | — | `xf.radiusMinus` |

### UV-редактор

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в shortcuts.ini |
|---|---|---|---|
| Выделить всё | <kbd>A</kbd> | — | `uv.selectAll` |
| Снять всё выделение | <kbd>Alt+A</kbd> | — | `uv.deselect` |
| Инвертировать выделение | <kbd>Ctrl+I</kbd> | — | `uv.invert` |
| Выделить связанное | <kbd>L</kbd> | — | `uv.linked` |
| Выделение рамкой | <kbd>B</kbd> | — | `uv.box` |
| Переместить | <kbd>G</kbd> | <kbd>1</kbd> | `uv.move` |
| Вращать | <kbd>R</kbd> | <kbd>2</kbd> | `uv.rotate` |
| Масштаб | <kbd>S</kbd> | <kbd>3</kbd> | `uv.scale` |
| Вставить ключевой кадр | <kbd>I</kbd> | — | `uv.keyframe` |
| Вставить ключевой кадр (всё) | <kbd>Shift+I</kbd> | — | `uv.keyframeAll` |
| Выдавить | <kbd>E</kbd> | — | `uv.extrude` |
| Дублировать | <kbd>Shift+D</kbd> | — | `uv.duplicate` |
| Удалить | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `uv.delete` |
| Показать выделенное | <kbd>Num.</kbd> | — | `uv.focus` |

### Таймлайн

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в shortcuts.ini |
|---|---|---|---|
| Выделить всё | <kbd>A</kbd> | — | `tl.selectAll` |
| Снять всё выделение | <kbd>Alt+A</kbd> | — | `tl.deselect` |
| Инвертировать выделение | <kbd>Ctrl+I</kbd> | — | `tl.invert` |
| Переместить | <kbd>G</kbd> | <kbd>1</kbd> | `tl.move` |
| Масштаб | <kbd>S</kbd> | <kbd>3</kbd> | `tl.scale` |
| Вращать рукоятки | <kbd>R</kbd> | <kbd>2</kbd> | `tl.rotate` |
| Дублировать | <kbd>Shift+D</kbd> | — | `tl.duplicate` |
| Удалить | <kbd>X</kbd> | — | `tl.delete` |
| Интерполяция | <kbd>T</kbd> | — | `tl.interp` |
| Тип рукоятки | <kbd>V</kbd> | — | `tl.handle` |
| Показать выделенное | <kbd>Num.</kbd> | <kbd>0</kbd> | `tl.focus` |
| Предыдущий кадр | <kbd>Left</kbd> | — | `tl.prevFrame` |
| Следующий кадр | <kbd>Right</kbd> | — | `tl.nextFrame` |
| Предыдущий ключевой кадр | <kbd>Down</kbd> | — | `tl.prevKey` |
| Следующий ключевой кадр | <kbd>Up</kbd> | — | `tl.nextKey` |
| В начало | <kbd>Home</kbd> | — | `tl.start` |
| В конец | <kbd>End</kbd> | — | `tl.end` |

### Аутлайнер

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в shortcuts.ini |
|---|---|---|---|
| Выделить всё | <kbd>A</kbd> | — | `out.selectAll` |
| Снять всё выделение | <kbd>Alt+A</kbd> | — | `out.deselect` |
| Инвертировать выделение | <kbd>Ctrl+I</kbd> | — | `out.invert` |
| Переместить (порядок) | <kbd>G</kbd> | <kbd>1</kbd> | `out.move` |
| Переименовать | <kbd>F2</kbd> | — | `out.rename` |
| Дублировать | <kbd>Shift+D</kbd> | — | `out.duplicate` |
| Дублировать связанно | <kbd>Alt+D</kbd> | — | `out.instance` |
| Скрыть / Показать | <kbd>H</kbd> | <kbd>2</kbd> | `out.hide` |
| Удалить | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `out.delete` |
| Показать активный | <kbd>Num.</kbd> | — | `out.focus` |

### 2D-редактор

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в shortcuts.ini |
|---|---|---|---|
| Выделить всё | <kbd>A</kbd> | — | `2d.selectAll` |
| Снять всё выделение | <kbd>Alt+A</kbd> | — | `2d.deselect` |
| Инвертировать выделение | <kbd>Ctrl+I</kbd> | — | `2d.invert` |
| Переместить | <kbd>G</kbd> | <kbd>1</kbd> | `2d.move` |
| Вращать | <kbd>R</kbd> | <kbd>2</kbd> | `2d.rotate` |
| Масштаб | <kbd>S</kbd> | <kbd>3</kbd> | `2d.scale` |
| Дублировать | <kbd>Shift+D</kbd> | — | `2d.duplicate` |
| Удалить | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `2d.delete` |
| Показать выделенное | <kbd>Num.</kbd> | — | `2d.focus` |
| Вид игры | <kbd>Num0</kbd> | — | `2d.gameView` |

### Свойства (шаги процедуры)

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в shortcuts.ini |
|---|---|---|---|
| Захватить шаг | <kbd>G</kbd> | — | `props.grab` |
| Удалить шаг | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `props.delete` |
| Вставить ключевой кадр | <kbd>I</kbd> | — | `props.keyframe` |

### Редактор скриптов

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в shortcuts.ini |
|---|---|---|---|
| Сохранить скрипт | <kbd>Ctrl+S</kbd> | — | `ide.save` |
| Отменить | <kbd>Ctrl+Z</kbd> | — | `ide.undo` |
| Копировать | <kbd>Ctrl+C</kbd> | — | `ide.copy` |
| Вырезать | <kbd>Ctrl+X</kbd> | — | `ide.cut` |
| Вставить | <kbd>Ctrl+V</kbd> | — | `ide.paste` |
| Выделить всё | <kbd>Ctrl+A</kbd> | — | `ide.all` |
| Повторить | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | — | `ide.redo` |
| Найти | <kbd>Ctrl+F</kbd> | — | `ide.find` |
| Заменить | <kbd>Ctrl+H</kbd> | — | `ide.replace` |

<!--ATAJOS-FIN-->

## Фиксированные клавиши (не переназначаются)

Некоторых клавиш нет в списке, потому что их действие зависит от того, что ты делаешь: стрелки при навигации по меню и панелям, <kbd>Enter</kbd> и <kbd>Esc</kbd> в меню и окнах, стрелки в Аутлайнере во время перемещения строки, клавиатура в IDE при наборе текста и клавиатура телефона на N95 (см. [Навигация](navegacion.md)).
