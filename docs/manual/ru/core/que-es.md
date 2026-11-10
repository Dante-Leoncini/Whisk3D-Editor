# Что такое Whisk3D Core

**Whisk3D Core** — это слой, который отделяет Whisk3D от каждой конкретной машины. Это секретный ингредиент: благодаря ему один и тот же код работает на **Windows**, **Linux**, **macOS**, **Android**, в **браузере** (WebGL), на **Nokia N95** и **N8** (Symbian), **Sega Dreamcast**, **Sony PSP**, **PS Vita** и **Nintendo 3DS**.

Core — это библиотека на C++ со своим репозиторием, [Whisk3D-Core](https://github.com/Dante-Leoncini/Whisk3D-Core), под лицензией MIT. Её устройство вдохновлено классическими движками вроде **RenderWare**: сверху небольшой стабильный API, а снизу своя реализация для каждой системы.

![Архитектура Whisk3D в три слоя: сверху Editor (зелёный), внутри него твоя игра или проект на Lua; посередине Whisk3D UI (жёлтый); внизу Whisk3D Core (красный). Editor и UI необязательны: снаружи игра на C++ может использовать UI, а другая — напрямую Core. Core общается с каждой системой через свой бэкенд](img/whisk3d-capas.webp)

## Идея: просить, а не делать

ПК с современной видеокартой, телефон 2007 года и Dreamcast рисуют способами, которые ничем не похожи друг на друга. ПК говорит на OpenGL или Vulkan; у PSP свой чип (GE) и своя библиотека (sceGu); 3DS использует citro3d; N95 — OpenGL ES 1.1. Если бы игра обращалась к каждой из них, её пришлось бы писать одиннадцать раз.

Эту проблему решает **слой абстракции**. Игра не рисует, а **просит**. Она говорит: «нарисуй эти треугольники с этой текстурой и этим светом», а часть, которая знает эту систему, — **бэкенд** — знает, как это сделать. То же самое со звуком, файлами, логом и сетью: один способ просить и одна реализация на систему.

Правила дома:

- **Код сверху никогда не обращается к системе.** Ни к OpenGL, ни к sceGu, ни к Symbian. Всё проходит через функции Core.
- **Заголовки не показывают системные типы.** Текстура — это `unsigned int`, а возможности — собственные enum (`w3dEngine::DepthTest`, `w3dEngine::Blend`), а не enum OpenGL. Так тот, кто использует Core, не зависит ни от одного графического заголовка.
- **Каждая система добавляет минимум.** Свой запуск (окно, контекст GPU) и то, чему нет места в общем коде. В репозитории редактора это находится в `platform/<sistema>/` и в блоках `#ifdef W3D_PSP`, `#ifdef W3D_3DS` и т. д.

Небольшой пример игры, написанной для Whisk3D Core, которая работает на всех системах:

```cpp
#include "graphics/w3dGraphics.h"   // рисование
#include "graphics/w3dTexture.h"    // текстуры
#include "io/w3dFilesystem.h"       // файлы
#include "base/w3dlog.h"            // лог
#include "audio/W3dAudio.h"         // звук

namespace gfx = w3dEngine;

// файл: может лежать на диске, внутри .w3d, внутри APK или быть упакован в игру
std::vector<unsigned char> nivel;
if (!w3dFileSystem::ReadFileBytes("niveles/uno.txt", nivel))
    w3dLogfE("no encontre %s", "niveles/uno.txt");

// текстура: её декодирует stb (или ICL в Symbian), а загружает бэкенд
unsigned int pasto = 0;
gfx::LoadTexture("texturas/pasto.png", pasto);

// звук: его микширует Core; вывод у каждой системы свой
w3dEngine::W3dSound* salto = w3dEngine::W3dSoundLoad("sonidos/salto.wav");
w3dEngine::W3dSoundPlay(salto, 1.0f, false);

// рисование: ни одного вызова OpenGL, sceGu или citro3d
gfx::Enable(gfx::Texture2D);
gfx::BindTexture(pasto);
gfx::EnableArray(gfx::VertexArray);   gfx::VertexPointer3f(0, posiciones);
gfx::EnableArray(gfx::TexCoordArray); gfx::TexCoordPointer2f(0, uvs);
gfx::DrawTriangles(cantidadDeIndices, indices);
```

## Что внутри

Каждая папка Core — это модуль:

| Папка | Что решает |
|---|---|
| `graphics/` | API рисования (`w3dEngine`), текстуры, экран и бэкенды: [Графика](graficos.md) |
| `audio/` | Эффекты, музыка и громкость: [Звук и музыка](sonido.md) |
| `io/` | Файлы, контейнеры (.w3d, zip, pak), меши и ресурсы: [Файлы](archivos.md) |
| `base/` | Лог, сохраняемые настройки и то, что общее для всех: [Логи](logs.md) |
| `input/` | Сенсорный экран, геймпады, наклон, вибрация и буфер обмена: [Ввод](entrada.md) |
| `net/` | Сетевая игра: [Сеть](red.md) |
| `physics/` | Физика, столкновения, хитбоксы, рэгдоллы и навигация: [Физика](fisica.md) |
| `script/` | Lua: [Скрипты Lua](lua.md) |
| `math/` | Векторы, матрицы и кватернионы: [Математика, сцена и форматы](formatos.md) |
| `objects/`, `animation/` | Сцена: объекты, меши, материалы, источники света, камеры и анимации: [Математика, сцена и форматы](formatos.md) |
| `tools/` | Генератор таблицы диспетчеризации, проверка независимости и упаковщик |
