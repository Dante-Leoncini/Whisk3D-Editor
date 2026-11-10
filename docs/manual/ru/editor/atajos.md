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

## Файл atajos.ini

Изменения сохраняются автоматически в `res/atajos.ini`, рядом с `config.ini`. В нём есть только изменённые функции, и его можно редактировать вручную:

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

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в atajos.ini |
|---|---|---|---|
| Сохранить проект | <kbd>Ctrl+S</kbd> | — | `global.guardar` |
| Отменить | <kbd>Ctrl+Z</kbd> | <kbd>C+Влево</kbd> | `global.deshacer` |
| Повторить | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | <kbd>C+Вправо</kbd> | `global.rehacer` |
| Объект / Редактирование | <kbd>Tab</kbd> | <kbd>OK</kbd> | `global.modo` |
| Вкл/выкл привязку | <kbd>Shift+Tab</kbd> | — | `global.snap` |
| Меню привязки | <kbd>Shift+S</kbd> | — | `global.menuSnap` |
| Пуск / Пауза | <kbd>Space</kbd> | <kbd>Зелёная+Лев. софт</kbd> | `global.play` |
| Строка меню | <kbd>M</kbd> | <kbd>Лев. софт</kbd> | `global.barraMenu` |
| Родитель | <kbd>Ctrl+P</kbd> | — | `global.emparentar` |
| Снять родителя | <kbd>Alt+P</kbd> | — | `global.desemparentar` |
| Полный экран | <kbd>F11</kbd> | — | `global.pantalla` |
| Рендер изображения | <kbd>F12</kbd> | — | `global.render` |
| Рендер анимации | <kbd>Ctrl+F12</kbd> | — | `global.renderAnim` |

### 3D-вьюпорт

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в atajos.ini |
|---|---|---|---|
| Выделить всё | <kbd>A</kbd> | <kbd>Карандаш+Вверх</kbd> | `3d.seleccionarTodo` |
| Снять всё выделение | <kbd>Alt+A</kbd> | <kbd>Карандаш+Вверх</kbd> | `3d.deseleccionar` |
| Инвертировать выделение | <kbd>Ctrl+I</kbd> | — | `3d.invertir` |
| Выделение рамкой | <kbd>B</kbd> | — | `3d.caja` |
| Выделить связанное | <kbd>L</kbd> | — | `3d.vinculado` |
| Добавить | <kbd>Shift+A</kbd> | — | `3d.agregar` |
| Применить | <kbd>Ctrl+A</kbd> | — | `3d.aplicar` |
| Переместить | <kbd>G</kbd> | <kbd>1</kbd> | `3d.mover` |
| Вращать | <kbd>R</kbd> | <kbd>2</kbd> | `3d.rotar` |
| Масштаб | <kbd>S</kbd> | <kbd>3</kbd> | `3d.escalar` |
| Сбросить положение (поза) | <kbd>Alt+G</kbd> | — | `3d.limpiarPos` |
| Сбросить вращение (поза) | <kbd>Alt+R</kbd> | — | `3d.limpiarRot` |
| Сжать/раздуть / Сбросить масштаб | <kbd>Alt+S</kbd> | — | `3d.limpiarEsc` |
| Дублировать | <kbd>Shift+D</kbd> | — | `3d.duplicar` |
| Дублировать связанно | <kbd>Alt+D</kbd> | — | `3d.instancia` |
| Удалить | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `3d.borrar` |
| Скрыть / Показать | <kbd>H</kbd> | — | `3d.ocultar` |
| Объединить | <kbd>Ctrl+J</kbd> | — | `3d.unir` |
| Вставить ключевой кадр | <kbd>I</kbd> | — | `3d.keyframe` |
| Вставить ключевой кадр (всё) | <kbd>Shift+I</kbd> | — | `3d.keyframeTodo` |
| Пропорциональное редактирование | <kbd>O</kbd> | — | `3d.proporcional` |
| Наложения | <kbd>K</kbd> | — | `3d.overlays` |
| Локальный вид | <kbd>/</kbd> | — | `3d.vistaLocal` |
| Показать выделенное | <kbd>Num.</kbd> | <kbd>0</kbd> | `3d.encuadrar` |
| Вид спереди | <kbd>Num1</kbd> | <kbd>#+Влево/Вправо</kbd> | `3d.vistaFrente` |
| Вид сзади | <kbd>Ctrl+Num1</kbd> | <kbd>#+Влево/Вправо</kbd> | `3d.vistaAtras` |
| Вид справа | <kbd>Num3</kbd> | <kbd>#+Влево/Вправо</kbd> | `3d.vistaDerecha` |
| Вид слева | <kbd>Ctrl+Num3</kbd> | <kbd>#+Влево/Вправо</kbd> | `3d.vistaIzquierda` |
| Вид сверху | <kbd>Num7</kbd> | <kbd>#+Вверх</kbd> | `3d.vistaArriba` |
| Вид снизу | <kbd>Ctrl+Num7</kbd> | <kbd>#+Вниз</kbd> | `3d.vistaAbajo` |
| Перспектива / Орто | <kbd>Num5</kbd> | <kbd>#</kbd> | `3d.perspectiva` |
| Крен влево | <kbd>Num4</kbd> | — | `3d.rodarIzq` |
| Крен вправо | <kbd>Num6</kbd> | — | `3d.rodarDer` |
| Вид из камеры | <kbd>Num0</kbd> | <kbd>8</kbd> | `3d.vistaCamara` |
| Сделать активной камерой | <kbd>Ctrl+Num0</kbd> | — | `3d.camaraActiva` |
| Выдавить | <kbd>E</kbd> | <kbd>7</kbd> | `3d.extruir` |
| Новая грань из вершин | <kbd>F</kbd> | — | `3d.cara` |
| Пересчитать нормали | <kbd>Shift+N</kbd> | — | `3d.normales` |
| Петлевой разрез и сдвиг | <kbd>Ctrl+R</kbd> | <kbd>8</kbd> | `3d.loopcut` |
| Соединить путь вершин | <kbd>J</kbd> | — | `3d.conectar` |
| Разорвать | <kbd>V</kbd> | — | `3d.rip` |
| Сдвиг вершин | <kbd>Shift+V</kbd> | — | `3d.slide` |
| Разделить | <kbd>P</kbd> | — | `3d.separar` |
| Слить | <kbd>M</kbd> | — | `3d.merge` |
| Контекстное меню | <kbd>W</kbd> | — | `3d.menuContexto` |
| Триангулировать | <kbd>Ctrl+T</kbd> | — | `3d.triangular` |
| Меню UV | <kbd>U</kbd> | — | `3d.menuUV` |

### Во время трансформации (перемещение / вращение / масштабирование)

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в atajos.ini |
|---|---|---|---|
| Подтвердить | <kbd>Enter</kbd>, <kbd>NumEnter</kbd> | <kbd>OK</kbd> | `xf.confirmar` |
| Отмена | <kbd>Esc</kbd> | <kbd>C</kbd> | `xf.cancelar` |
| Ось X | <kbd>X</kbd> | <kbd>1</kbd> | `xf.ejeX` |
| Ось Y | <kbd>Y</kbd> | <kbd>2</kbd> | `xf.ejeY` |
| Ось Z | <kbd>Z</kbd> | <kbd>3</kbd> | `xf.ejeZ` |
| Исключить X (плоскость) | <kbd>Shift+X</kbd> | — | `xf.planoX` |
| Исключить Y (плоскость) | <kbd>Shift+Y</kbd> | — | `xf.planoY` |
| Исключить Z (плоскость) | <kbd>Shift+Z</kbd> | — | `xf.planoZ` |
| Пропорциональный радиус + | <kbd>PageUp</kbd> | — | `xf.radioMas` |
| Пропорциональный радиус - | <kbd>PageDown</kbd> | — | `xf.radioMenos` |

### UV-редактор

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в atajos.ini |
|---|---|---|---|
| Выделить всё | <kbd>A</kbd> | — | `uv.seleccionarTodo` |
| Снять всё выделение | <kbd>Alt+A</kbd> | — | `uv.deseleccionar` |
| Инвертировать выделение | <kbd>Ctrl+I</kbd> | — | `uv.invertir` |
| Выделить связанное | <kbd>L</kbd> | — | `uv.vinculado` |
| Выделение рамкой | <kbd>B</kbd> | — | `uv.caja` |
| Переместить | <kbd>G</kbd> | <kbd>1</kbd> | `uv.mover` |
| Вращать | <kbd>R</kbd> | <kbd>2</kbd> | `uv.rotar` |
| Масштаб | <kbd>S</kbd> | <kbd>3</kbd> | `uv.escalar` |
| Вставить ключевой кадр | <kbd>I</kbd> | — | `uv.keyframe` |
| Вставить ключевой кадр (всё) | <kbd>Shift+I</kbd> | — | `uv.keyframeTodo` |
| Выдавить | <kbd>E</kbd> | — | `uv.extruir` |
| Дублировать | <kbd>Shift+D</kbd> | — | `uv.duplicar` |
| Удалить | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `uv.borrar` |
| Показать выделенное | <kbd>Num.</kbd> | — | `uv.encuadrar` |

### Таймлайн

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в atajos.ini |
|---|---|---|---|
| Выделить всё | <kbd>A</kbd> | — | `tl.seleccionarTodo` |
| Снять всё выделение | <kbd>Alt+A</kbd> | — | `tl.deseleccionar` |
| Инвертировать выделение | <kbd>Ctrl+I</kbd> | — | `tl.invertir` |
| Переместить | <kbd>G</kbd> | <kbd>1</kbd> | `tl.mover` |
| Масштаб | <kbd>S</kbd> | <kbd>3</kbd> | `tl.escalar` |
| Вращать рукоятки | <kbd>R</kbd> | <kbd>2</kbd> | `tl.rotar` |
| Дублировать | <kbd>Shift+D</kbd> | — | `tl.duplicar` |
| Удалить | <kbd>X</kbd> | — | `tl.borrar` |
| Интерполяция | <kbd>T</kbd> | — | `tl.interp` |
| Тип рукоятки | <kbd>V</kbd> | — | `tl.handle` |
| Показать выделенное | <kbd>Num.</kbd> | <kbd>0</kbd> | `tl.encuadrar` |
| Предыдущий кадр | <kbd>Left</kbd> | — | `tl.frameAnt` |
| Следующий кадр | <kbd>Right</kbd> | — | `tl.frameSig` |
| Предыдущий ключевой кадр | <kbd>Down</kbd> | — | `tl.keyAnt` |
| Следующий ключевой кадр | <kbd>Up</kbd> | — | `tl.keySig` |
| В начало | <kbd>Home</kbd> | — | `tl.inicio` |
| В конец | <kbd>End</kbd> | — | `tl.fin` |

### Аутлайнер

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в atajos.ini |
|---|---|---|---|
| Выделить всё | <kbd>A</kbd> | — | `out.seleccionarTodo` |
| Снять всё выделение | <kbd>Alt+A</kbd> | — | `out.deseleccionar` |
| Инвертировать выделение | <kbd>Ctrl+I</kbd> | — | `out.invertir` |
| Переместить (порядок) | <kbd>G</kbd> | <kbd>1</kbd> | `out.mover` |
| Переименовать | <kbd>F2</kbd> | — | `out.renombrar` |
| Дублировать | <kbd>Shift+D</kbd> | — | `out.duplicar` |
| Дублировать связанно | <kbd>Alt+D</kbd> | — | `out.instancia` |
| Скрыть / Показать | <kbd>H</kbd> | <kbd>2</kbd> | `out.ocultar` |
| Удалить | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `out.borrar` |
| Показать активный | <kbd>Num.</kbd> | — | `out.encuadrar` |

### 2D-редактор

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в atajos.ini |
|---|---|---|---|
| Выделить всё | <kbd>A</kbd> | — | `2d.seleccionarTodo` |
| Снять всё выделение | <kbd>Alt+A</kbd> | — | `2d.deseleccionar` |
| Инвертировать выделение | <kbd>Ctrl+I</kbd> | — | `2d.invertir` |
| Переместить | <kbd>G</kbd> | <kbd>1</kbd> | `2d.mover` |
| Вращать | <kbd>R</kbd> | <kbd>2</kbd> | `2d.rotar` |
| Масштаб | <kbd>S</kbd> | <kbd>3</kbd> | `2d.escalar` |
| Дублировать | <kbd>Shift+D</kbd> | — | `2d.duplicar` |
| Удалить | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `2d.borrar` |
| Показать выделенное | <kbd>Num.</kbd> | — | `2d.encuadrar` |
| Вид игры | <kbd>Num0</kbd> | — | `2d.vistaJuego` |

### Свойства (шаги процедуры)

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в atajos.ini |
|---|---|---|---|
| Захватить шаг | <kbd>G</kbd> | — | `props.agarrar` |
| Удалить шаг | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `props.borrar` |
| Вставить ключевой кадр | <kbd>I</kbd> | — | `props.keyframe` |

### Редактор скриптов

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в atajos.ini |
|---|---|---|---|
| Сохранить скрипт | <kbd>Ctrl+S</kbd> | — | `ide.guardar` |
| Отменить | <kbd>Ctrl+Z</kbd> | — | `ide.deshacer` |
| Копировать | <kbd>Ctrl+C</kbd> | — | `ide.copiar` |
| Вырезать | <kbd>Ctrl+X</kbd> | — | `ide.cortar` |
| Вставить | <kbd>Ctrl+V</kbd> | — | `ide.pegar` |
| Выделить всё | <kbd>Ctrl+A</kbd> | — | `ide.todo` |
| Повторить | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | — | `ide.rehacer` |
| Найти | <kbd>Ctrl+F</kbd> | — | `ide.buscar` |
| Заменить | <kbd>Ctrl+H</kbd> | — | `ide.reemplazar` |

<!--ATAJOS-FIN-->

## Фиксированные клавиши (не переназначаются)

Некоторых клавиш нет в списке, потому что их действие зависит от того, что ты делаешь: стрелки при навигации по меню и панелям, <kbd>Enter</kbd> и <kbd>Esc</kbd> в меню и окнах, стрелки в Аутлайнере во время перемещения строки, клавиатура в IDE при наборе текста и клавиатура телефона на N95 (см. [Навигация](navegacion.md)).
