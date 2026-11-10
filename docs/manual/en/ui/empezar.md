# Using the UI directly

For people who program in C or C++ and want the interface without the editor. You need:

- **The Core, with its scene model.** The UI draws with `w3dEngine` and some rows read objects from the scene (`objects/`, `animation/`, `script/`). See [Using the Core directly](../core/empezar.md).
- **The UI sources.** All the `.cpp` files in `libs/WhiskUI/`. The include directories are `libs/`, for `"WhiskUI/draw/icons.h"`, and the subfolders `widgets/`, `text/`, `draw/`, `theme/` and `core/`, because inside they include each other without the folder (`"icons.h"`, `"card.h"`).
- **The skin files.** `font.png` (the letters and the card borders), `font-ext.png` (the extended atlas), `skin.ini` (the colors) and, for the new icons, the `atlas/iconos/` folder. They don't come with the library: they're in the editor's repository, in `res/Skins/Whisk3D/`.

## Starting up

The same thing the editor does at startup, minus what belongs to it:

```cpp
#include "w3dTexture.h"
#include "WhiskUI/core/UI.h"
#include "WhiskUI/theme/colores.h"
#include "WhiskUI/draw/icons.h"
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/text/font.h"
#include "WhiskUI/text/W3dFont.h"

loadColorsW3d("Skins/Whisk3D/skin.ini");    // without the file, the colors in the code stay

unsigned int atlas = 0; int w = 0, h = 0;
w3dEngine::LoadTexture("Skins/Whisk3D/font.png", atlas, &w, &h);

CalcCardUV(w, h);          // the rounded edges
CrearIconos(w, h);         // the icons that are inside font.png
W3dFontSetTexSize(w, h);
SetIconScale(GlobalScale);

WhiskFont = new Font(w, h, atlas);
WhiskFont->SetScale(GlobalScale);
WhiskFont->SetAtlasExtendido("Skins/Whisk3D/font-ext.png");   // it's only loaded when needed
```

`GlobalScale` starts at 1. For another scale, see [Colors and scale](tema.md). For the separate icons in `atlas/iconos/`, see [Drawing and atlas](dibujo.md).

Every time the window size changes:

```cpp
W3dPantallaAlto = alto;    // to convert the interface's Y to Scissor's and Viewport's
MenuPantallaW = ancho;     // so the menus don't go off the screen
MenuPantallaH = alto;
```

## The 2D state

The UI draws in pixels, with Y pointing down and the origin at the top left. Each widget is drawn at the origin of the current matrix: you position it with `Translatef`. Before drawing:

```cpp
namespace gfx = w3dEngine;

gfx::Viewport(0, 0, ancho, alto);
gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
gfx::Ortho(0, ancho, alto, 0, -1, 1);       // Y pointing down
gfx::MatrixMode(gfx::ModelView);  gfx::LoadIdentity();
gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::CullFace);
gfx::Disable(gfx::Lighting);  gfx::Disable(gfx::Fog);
gfx::EnableArray(gfx::VertexArray); gfx::EnableArray(gfx::TexCoordArray);
gfx::DisableArray(gfx::NormalArray);
gfx::Enable(gfx::Texture2D);
gfx::BindTexture(atlas);
gfx::TexFilter(false);                       // crisp pixels
gfx::Enable(gfx::Blend); gfx::BlendAlpha();
```

## A card

A card with a number, a checkbox and a button. Each row points to a variable of yours:

```cpp
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropFloat.h"
#include "WhiskUI/Propieties/PropButton.h"

static float volumen = 0.8f;
static bool  pantallaCompleta = false;
static void Salir() { /* ... */ }

GroupPropertie* opciones = new GroupPropertie("Opciones");

PropFloat* vol = new PropFloat("Volumen");
vol->value = &volumen;
vol->SetRango(0.0f, 1.0f);
opciones->properties.push_back(vol);

PropBool* pc = new PropBool("Pantalla completa");
pc->value = &pantallaCompleta;
opciones->properties.push_back(pc);

PropButton* salir = new PropButton("Salir", IconType::borrar);
salir->action = Salir;
opciones->properties.push_back(salir);

opciones->Resize(200 * GlobalScale, 0);    // the card works out the height

// every frame, with the 2D state set
gfx::PushMatrix();
gfx::Translatef(10, 10, 0);
opciones->Render();
gfx::PopMatrix();
```

The UI doesn't read the keyboard or the mouse: you pass them in. With the keyboard:

```cpp
// down arrow
if (opciones->editando) opciones->button_down();   // changes the value
else opciones->NextSelect();                        // the row below
// OK or Enter: on the title it folds the card; on a row, it edits or presses it
opciones->EnterPropertieSelect();
// Cancel: leaves the row as it was
opciones->Cancel();
```

With the mouse, you work out the row under the pointer yourself: see [Cards and properties](propiedades.md). Menus do take coordinates: see [Drop-down menus](menus.md).

## The current state, plainly

- **It isn't production-ready.** The repository's `README.md` says so, and it's true: it has editor code inside, which is being separated.
- **There's no example that uses it on its own.** The `ui/` examples in [Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples) are `.w3dui` interfaces with Lua run by the game runtime: that belongs to the editor. The code on this page is put together with the same calls the editor uses; it doesn't come from a tested example.
- **Today it doesn't link on its own.** `PropList.cpp` calls `RebindMaterialMeshPart()`, which is in the editor's properties panel. The focus functions in `TextField.h` (`TextFieldEnfocar`, `TextFieldCancelar`, `TextFieldInputChar`, `TextFieldFinDeCuadro`) are defined in the editor, in `main/ui/ViewPorts/NumInput.cpp`.
- **It depends on the Core's scene model.** `PropertieBase.h` includes `objects/Objects.h`, and the property list reads meshes, armatures, animations and scripts.
- **The editor recalculates the scale.** `SetGlobalScale` is in `main/ui/ViewPorts/ViewPorts.cpp`. On its own, you have to recalculate the measurements by hand ([Colors and scale](tema.md)).
- **Everything is global.** There's one font (`WhiskFont`), one icon table (`IconsUV`), one open menu (`MenuAbierto`) and one focused text box (`g_textFieldActivo`) per program.
- **The headers expose OpenGL types** (`GLfloat`, `GLshort`, `GLuint`), even though they no longer call OpenGL.
- **What's coming,** according to the `README.md`: cleaning out the code that belongs to the editor and making usage examples.

What belongs to the UI and what to the editor, item by item: [What belongs to the UI and what to the editor](ui-y-editor.md). Questions go to the [Telegram](https://t.me/Whisk3D) group.
