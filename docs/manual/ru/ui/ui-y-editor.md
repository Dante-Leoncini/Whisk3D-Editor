# Что относится к UI, а что к редактору

Редактор сделан с помощью Whisk3D UI, но многое из того, что в нём видно, принадлежит ему самому. То, что относится к UI, лежит в `libs/WhiskUI/`; то, что относится к редактору, — в `main/` репозитория Whisk3D.

## Из UI

- 2D-рисование, карточки с рамкой и прямоугольники.
- Таблица иконок и упаковщик атласа.
- Растровый шрифт, расширенный атлас и текст запечёнными шрифтами TTF.
- Палитра цветов и размеры вместе с их масштабом.
- Кнопки, вкладки, флажки и выпадающие меню.
- Карточки свойств и их строки, с ромбом анимации.
- Состояние текстовых полей и их отрисовка.

## Из редактора

| Что | Где |
|---|---|
| Вьюпорты: разделение экрана, их рамки, их строки меню и панели инструментов | `main/ui/ViewPorts/` (`ViewPorts.cpp`, `WithBorder.cpp`, `ToolbarBase.cpp`) |
| Панель свойств: какие в ней карточки, её вкладки, клик и перетаскивание по строкам | `main/ui/ViewPorts/Properties.cpp` |
| Модальные окна: выбор цвета, файловый браузер, подтверждения, индикатор выполнения, экранные клавиатуры | `main/ui/ViewPorts/PopUp/` |
| Уведомления | `main/ui/ViewPorts/Notificaciones.cpp` |
| Полосы прокрутки панелей | `main/ui/ViewPorts/ScrollBar.cpp` |
| Всплывающие подсказки. Текст хранит `Button::ayuda` | `W3dAyudaRender`, в `main/ui/ViewPorts/LayoutInput.cpp` |
| Какая клавиша, клик или касание идёт к какому виджету, и клавиши N95 | `main/ui/ViewPorts/LayoutInput.cpp`, `main/app/controles.cpp`, `platform/symbian/src/` |
| Функции фокуса текстовых полей | `main/ui/ViewPorts/NumInput.cpp` |
| Переводы, `T()`. У UI есть только хук `W3dMenuTraducir` | `main/config/W3dLang.cpp` |
| Запуск UI | `main/app/W3dInitUI.cpp` |
| Сборка атласа скина из отдельных иконок | `main/io/SkinAtlas.cpp` |
| Пересчёт масштаба | `SetGlobalScale`, в `main/ui/ViewPorts/ViewPorts.cpp` |
| Чтение `skin.ini` на ПК и на Android | `loadColors`, в `main/app/main.cpp` |
| Собственные цвета редактора: пол, оси, нормали | `main/ui/W3dColors.cpp` |
| Файлы скина: `font.png`, `font-ext.png`, `atlas/iconos/`, `skin.ini` | `res/Skins/Whisk3D/` |
| Генератор расширенного атласа | `tools/fuente_ext.py` |
| [2D-редактор](../editor/editor-2d.md) и интерфейсы игр `.w3dui`: тексты, изображения, прямоугольники, контейнеры, slice 9, кнопки и видео | `main/objects/` (`UI.h`, `Texto2D.h`, `Boton2D.h`...), `main/render/UIOverlay.cpp`, `main/io/UI2DFormato.cpp`, `main/io/Fuente2D.cpp`, `main/ui/ViewPorts/Editor2D.cpp` |

2D-редактор рисует текст с помощью `W3dTextAtlas`, который относится к UI. А запекание TTF во время работы, через stb_truetype, относится к редактору (`main/io/Fuente2D.cpp`).

## В UI, но от редактора

То, что лежит в `libs/WhiskUI/`, но пока ещё принадлежит редактору:

- `PropListMeshParts` читает меши, скелеты, анимации, ограничители и скрипты и вызывает `RebindMaterialMeshPart()` из панели свойств.
- `PropColor` хранит палитру и объект, из которого берётся цвет (`palRef`, `palObj`), для выбора цвета.
- `GroupProperties` — глобальный список карточек панели свойств.
- В `core/UI.h` есть состояние редактора: режим сенсорного вида в Режиме редактирования (`g_viewEditMode`), движение мыши (`dx`, `dy`), клавиши Alt, Shift и Ctrl (`LAltPressed`, `LShiftPressed`, `LCtrlPressed`) и размеры вьюпортов (`paddingViewport`, `MinViewportHeight`, `MinViewportWidth`).
- Большинство иконок — это инструменты редактора.

Что ещё осталось разделить и в каком состоянии библиотека: в конце страницы [Использовать UI напрямую](empezar.md).
