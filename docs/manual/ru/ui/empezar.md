# Использовать UI напрямую

Для тех, кто программирует на C или C++ и хочет получить интерфейс без редактора. Понадобится:

- **Core с его моделью сцены.** UI рисует через `w3dEngine`, а некоторые строки читают объекты сцены (`objects/`, `animation/`, `script/`). См. [Использовать Core напрямую](../core/empezar.md).
- **Исходники UI.** Все `.cpp` из `libs/WhiskUI/`. Каталоги заголовков — `libs/`, для `"WhiskUI/draw/icons.h"`, и подпапки `widgets/`, `text/`, `draw/`, `theme/` и `core/`, потому что внутри файлы подключают друг друга без папки (`"icons.h"`, `"card.h"`).
- **Файлы скина.** `font.png` (буквы и края карточек), `font-ext.png` (расширенный атлас), `skin.ini` (цвета) и, для новых иконок, папка `atlas/icons/`. С библиотекой они не поставляются: они лежат в репозитории редактора, в `res/Skins/Whisk3D/`.

## Запуск

То же, что делает редактор при запуске, без того, что относится к нему самому:

```cpp
#include "w3dTexture.h"
#include "WhiskUI/core/UI.h"
#include "WhiskUI/theme/colors.h"
#include "WhiskUI/draw/icons.h"
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/text/font.h"
#include "WhiskUI/text/W3dFont.h"

loadColorsW3d("Skins/Whisk3D/skin.ini");    // без файла остаются цвета из кода

unsigned int atlas = 0; int w = 0, h = 0;
w3dEngine::LoadTexture("Skins/Whisk3D/font.png", atlas, &w, &h);

CalcCardUV(w, h);          // скруглённые края
CreateIcons(w, h);         // иконки, которые лежат внутри font.png
W3dFontSetTexSize(w, h);
SetIconScale(GlobalScale);

WhiskFont = new Font(w, h, atlas);
WhiskFont->SetScale(GlobalScale);
WhiskFont->SetAtlasExtended("Skins/Whisk3D/font-ext.png");   // загружается только когда понадобится
```

`GlobalScale` начинается с 1. Для другого масштаба см. [Цвета и масштаб](tema.md). Для отдельных иконок из `atlas/icons/` см. [Рисование и атлас](dibujo.md).

Каждый раз, когда меняется размер окна:

```cpp
W3dScreenHeight = height;    // чтобы переводить Y интерфейса в Y для Scissor и Viewport
MenuScreenW = width;     // чтобы меню не выходили за пределы экрана
MenuScreenH = height;
```

## 2D-состояние

UI рисует в пикселях, с осью Y вниз и началом координат в левом верхнем углу. Каждый виджет рисуется в начале координат текущей матрицы: его размещают с помощью `Translatef`. Перед рисованием:

```cpp
namespace gfx = w3dEngine;

gfx::Viewport(0, 0, width, height);
gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
gfx::Ortho(0, width, height, 0, -1, 1);       // Y вниз
gfx::MatrixMode(gfx::ModelView);  gfx::LoadIdentity();
gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::CullFace);
gfx::Disable(gfx::Lighting);  gfx::Disable(gfx::Fog);
gfx::EnableArray(gfx::VertexArray); gfx::EnableArray(gfx::TexCoordArray);
gfx::DisableArray(gfx::NormalArray);
gfx::Enable(gfx::Texture2D);
gfx::BindTexture(atlas);
gfx::TexFilter(false);                       // чёткие пиксели
gfx::Enable(gfx::Blend); gfx::BlendAlpha();
```

## Карточка

Карточка с числом, флажком и кнопкой. Каждая строка указывает на твою переменную:

```cpp
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropFloat.h"
#include "WhiskUI/Propieties/PropButton.h"

static float volume = 0.8f;
static bool  fullscreen = false;
static void Exit() { /* ... */ }

GroupPropertie* options = new GroupPropertie("Opciones");

PropFloat* vol = new PropFloat("Volumen");
vol->value = &volume;
vol->SetRange(0.0f, 1.0f);
options->properties.push_back(vol);

PropBool* pc = new PropBool("Pantalla completa");
pc->value = &fullscreen;
options->properties.push_back(pc);

PropButton* exit = new PropButton("Salir", IconType::deleteValue);
exit->action = Exit;
options->properties.push_back(exit);

options->Resize(200 * GlobalScale, 0);    // высоту рассчитывает карточка

// в каждом кадре, с установленным 2D-состоянием
gfx::PushMatrix();
gfx::Translatef(10, 10, 0);
options->Render();
gfx::PopMatrix();
```

UI не читает клавиатуру и мышь: их передаёшь ты сам. С клавиатурой:

```cpp
// стрелка вниз
if (options->editing) options->button_down();   // меняет значение
else options->NextSelect();                        // строка ниже
// OK или Enter: на заголовке сворачивает карточку; на строке — редактирует или нажимает её
options->EnterPropertieSelect();
// Отмена: оставляет строку как была
options->Cancel();
```

С мышью строку под указателем вычисляешь ты: см. [Карточки и свойства](propiedades.md). А вот меню принимают координаты: см. [Выпадающие меню](menus.md).

## Текущее состояние, без прикрас

- **Она не готова для продакшена.** Так сказано в `README.md` репозитория, и это правда: внутри есть код редактора, который сейчас отделяется.
- **Нет примера, который использовал бы её отдельно.** Примеры из `ui/` в [Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples) — это интерфейсы `.w3dui` с Lua, которые запускает среда выполнения игры: это относится к редактору. Код на этой странице составлен из тех же вызовов, что использует редактор, а не взят из проверенного примера.
- **Сейчас она не линкуется отдельно.** `PropList.cpp` вызывает `RebindMaterialMeshPart()`, которая находится в панели свойств редактора. Функции фокуса из `TextField.h` (`TextFieldFocus`, `TextFieldCancel`, `TextFieldInputChar`, `TextFieldFrameEnd`) определены в редакторе, в `main/ui/ViewPorts/NumInput.cpp`.
- **Она зависит от модели сцены Core.** `PropertieBase.h` подключает `objects/Objects.h`, а список свойств читает меши, скелеты, анимации и скрипты.
- **Масштаб пересчитывает редактор.** `SetGlobalScale` находится в `main/ui/ViewPorts/ViewPorts.cpp`. Без редактора размеры нужно пересчитывать вручную ([Цвета и масштаб](tema.md)).
- **Всё глобальное.** На программу приходится один шрифт (`WhiskFont`), одна таблица иконок (`IconsUV`), одно открытое меню (`OpenMenu`) и одно текстовое поле в фокусе (`g_activeTextField`).
- **Заголовки показывают типы OpenGL** (`GLfloat`, `GLshort`, `GLuint`), хотя к OpenGL они уже не обращаются.
- **Что дальше**, согласно `README.md`: вычистить код, который относится к редактору, и сделать примеры использования.

Что относится к UI, а что к редактору, по пунктам: [Что относится к UI, а что к редактору](ui-y-editor.md). Вопросы — в группе в [Telegram](https://t.me/Whisk3D).
