# Usar la UI directamente

Para quien programa en C o C++ y quiere la interfaz sin el editor. Hace falta:

- **El Core, con su modelo de escena.** La UI dibuja con `w3dEngine` y algunas filas leen objetos de la escena (`objects/`, `animation/`, `script/`). Ver [Usar el Core directamente](../core/empezar.md).
- **Las fuentes de la UI.** Todas las `.cpp` de `libs/WhiskUI/`. Los directorios de includes son `libs/`, para `"WhiskUI/draw/icons.h"`, y las subcarpetas `widgets/`, `text/`, `draw/`, `theme/` y `core/`, porque adentro se incluyen entre ellas sin la carpeta (`"icons.h"`, `"card.h"`).
- **Los archivos del skin.** `font.png` (las letras y los bordes de las tarjetas), `font-ext.png` (el atlas extendido), `skin.ini` (los colores) y, para los íconos nuevos, la carpeta `atlas/iconos/`. No vienen con la biblioteca: están en el repositorio del editor, en `res/Skins/Whisk3D/`.

## Arrancar

Lo mismo que hace el editor al arrancar, sin lo que es suyo:

```cpp
#include "w3dTexture.h"
#include "WhiskUI/core/UI.h"
#include "WhiskUI/theme/colores.h"
#include "WhiskUI/draw/icons.h"
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/text/font.h"
#include "WhiskUI/text/W3dFont.h"

loadColorsW3d("Skins/Whisk3D/skin.ini");    // sin el archivo quedan los colores del código

unsigned int atlas = 0; int w = 0, h = 0;
w3dEngine::LoadTexture("Skins/Whisk3D/font.png", atlas, &w, &h);

CalcCardUV(w, h);          // los bordes redondeados
CrearIconos(w, h);         // los íconos que están adentro de font.png
W3dFontSetTexSize(w, h);
SetIconScale(GlobalScale);

WhiskFont = new Font(w, h, atlas);
WhiskFont->SetScale(GlobalScale);
WhiskFont->SetAtlasExtendido("Skins/Whisk3D/font-ext.png");   // se carga recién cuando hace falta
```

`GlobalScale` arranca en 1. Para otra escala, ver [Colores y escala](tema.md). Para los íconos sueltos de `atlas/iconos/`, ver [Dibujo y atlas](dibujo.md).

Cada vez que cambia el tamaño de la ventana:

```cpp
W3dPantallaAlto = alto;    // para pasar la Y de la interfaz a la de Scissor y Viewport
MenuPantallaW = ancho;     // para que los menús no se salgan de la pantalla
MenuPantallaH = alto;
```

## El estado 2D

La UI dibuja en píxeles, con la Y para abajo y el origen arriba a la izquierda. Cada widget se dibuja en el origen de la matriz actual: se lo ubica con `Translatef`. Antes de dibujar:

```cpp
namespace gfx = w3dEngine;

gfx::Viewport(0, 0, ancho, alto);
gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
gfx::Ortho(0, ancho, alto, 0, -1, 1);       // Y para abajo
gfx::MatrixMode(gfx::ModelView);  gfx::LoadIdentity();
gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::CullFace);
gfx::Disable(gfx::Lighting);  gfx::Disable(gfx::Fog);
gfx::EnableArray(gfx::VertexArray); gfx::EnableArray(gfx::TexCoordArray);
gfx::DisableArray(gfx::NormalArray);
gfx::Enable(gfx::Texture2D);
gfx::BindTexture(atlas);
gfx::TexFilter(false);                       // píxeles nítidos
gfx::Enable(gfx::Blend); gfx::BlendAlpha();
```

## Una tarjeta

Una tarjeta con un número, una casilla y un botón. Cada fila apunta a una variable tuya:

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

opciones->Resize(200 * GlobalScale, 0);    // el alto lo calcula la tarjeta

// en cada cuadro, con el estado 2D puesto
gfx::PushMatrix();
gfx::Translatef(10, 10, 0);
opciones->Render();
gfx::PopMatrix();
```

La UI no lee el teclado ni el mouse: se los pasás vos. Con el teclado:

```cpp
// flecha abajo
if (opciones->editando) opciones->button_down();   // cambia el valor
else opciones->NextSelect();                        // la fila de abajo
// OK o Enter: en el título pliega la tarjeta; en una fila, la edita o la aprieta
opciones->EnterPropertieSelect();
// Cancelar: deja la fila como estaba
opciones->Cancel();
```

Con el mouse, la fila bajo el puntero la calculás vos: ver [Tarjetas y propiedades](propiedades.md). Los menús sí reciben coordenadas: ver [Menús desplegables](menus.md).

## El estado actual, sin vueltas

- **No está lista para producción.** Lo dice el `README.md` del repositorio, y es así: tiene código del editor adentro, que se está separando.
- **No hay un ejemplo que la use sola.** Los ejemplos de `ui/` de [Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples) son interfaces `.w3dui` con Lua que corre el runtime del juego: eso es del editor. El código de esta página está armado con las mismas llamadas que usa el editor, no sale de un ejemplo probado.
- **Hoy no enlaza sola.** `PropList.cpp` llama a `RebindMaterialMeshPart()`, que está en el panel de propiedades del editor. Las funciones de foco de `TextField.h` (`TextFieldEnfocar`, `TextFieldCancelar`, `TextFieldInputChar`, `TextFieldFinDeCuadro`) están definidas en el editor, en `main/ui/ViewPorts/NumInput.cpp`.
- **Depende del modelo de escena del Core.** `PropertieBase.h` incluye `objects/Objects.h`, y la lista de propiedades lee mallas, esqueletos, animaciones y scripts.
- **La escala la recalcula el editor.** `SetGlobalScale` está en `main/ui/ViewPorts/ViewPorts.cpp`. Sola, hay que recalcular las medidas a mano ([Colores y escala](tema.md)).
- **Todo es global.** Hay una fuente (`WhiskFont`), una tabla de íconos (`IconsUV`), un menú abierto (`MenuAbierto`) y una caja de texto enfocada (`g_textFieldActivo`) por programa.
- **Los headers muestran tipos de OpenGL** (`GLfloat`, `GLshort`, `GLuint`), aunque ya no llaman a OpenGL.
- **Lo que viene,** según el `README.md`: limpiar el código que es del editor y hacer ejemplos de uso.

Qué es de la UI y qué del editor, cosa por cosa: [Qué es de la UI y qué del editor](ui-y-editor.md). Las dudas, en el grupo de [Telegram](https://t.me/Whisk3D).
