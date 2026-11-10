# Usare la UI direttamente

Per chi programma in C o C++ e vuole l'interfaccia senza l'editor. Serve:

- **Il Core, con il suo modello di scena.** La UI disegna con `w3dEngine` e alcune righe leggono oggetti della scena (`objects/`, `animation/`, `script/`). Vedi [Usare il Core direttamente](../core/empezar.md).
- **I sorgenti della UI.** Tutti i `.cpp` di `libs/WhiskUI/`. Le directory di include sono `libs/`, per `"WhiskUI/draw/icons.h"`, e le sottocartelle `widgets/`, `text/`, `draw/`, `theme/` e `core/`, perché all'interno i file si includono a vicenda senza la cartella (`"icons.h"`, `"card.h"`).
- **I file della skin.** `font.png` (le lettere e i bordi dei riquadri), `font-ext.png` (l'atlas esteso), `skin.ini` (i colori) e, per le icone nuove, la cartella `atlas/iconos/`. Non sono inclusi nella libreria: stanno nel repository dell'editor, in `res/Skins/Whisk3D/`.

## Avvio

Ciò che fa l'editor all'avvio, senza la parte che è sua:

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

`GlobalScale` parte da 1. Per un'altra scala, vedi [Colori e scala](tema.md). Per le icone separate di `atlas/iconos/`, vedi [Disegno e atlas](dibujo.md).

Ogni volta che cambia la dimensione della finestra:

```cpp
W3dPantallaAlto = alto;    // para pasar la Y de la interfaz a la de Scissor y Viewport
MenuPantallaW = ancho;     // para que los menús no se salgan de la pantalla
MenuPantallaH = alto;
```

## Lo stato 2D

La UI disegna in pixel, con la Y verso il basso e l'origine in alto a sinistra. Ogni widget si disegna nell'origine della matrice attuale: lo si posiziona con `Translatef`. Prima di disegnare:

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

## Un riquadro

Un riquadro con un numero, una casella di controllo e un pulsante. Ogni riga punta a una tua variabile:

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

La UI non legge la tastiera né il mouse: glieli passi tu. Con la tastiera:

```cpp
// flecha abajo
if (opciones->editando) opciones->button_down();   // cambia el valor
else opciones->NextSelect();                        // la fila de abajo
// OK o Enter: en el título pliega la tarjeta; en una fila, la edita o la aprieta
opciones->EnterPropertieSelect();
// Cancelar: deja la fila como estaba
opciones->Cancel();
```

Con il mouse, la riga sotto il puntatore la calcoli tu: vedi [Riquadri e proprietà](propiedades.md). I menu invece ricevono le coordinate: vedi [Menu a tendina](menus.md).

## Lo stato attuale, senza giri di parole

- **Non è pronta per la produzione.** Lo dice il `README.md` del repository, ed è così: contiene codice dell'editor, che si sta separando.
- **Non c'è un esempio che la usi da sola.** Gli esempi di `ui/` di [Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples) sono interfacce `.w3dui` con Lua eseguite dal runtime del gioco: questo riguarda l'editor. Il codice di questa pagina è costruito con le stesse chiamate che usa l'editor, non viene da un esempio collaudato.
- **Oggi non si linka da sola.** `PropList.cpp` chiama `RebindMaterialMeshPart()`, che sta nel pannello delle proprietà dell'editor. Le funzioni di focus di `TextField.h` (`TextFieldEnfocar`, `TextFieldCancelar`, `TextFieldInputChar`, `TextFieldFinDeCuadro`) sono definite nell'editor, in `main/ui/ViewPorts/NumInput.cpp`.
- **Dipende dal modello di scena del Core.** `PropertieBase.h` include `objects/Objects.h`, e l'elenco delle proprietà legge mesh, armature, animazioni e script.
- **La scala la ricalcola l'editor.** `SetGlobalScale` sta in `main/ui/ViewPorts/ViewPorts.cpp`. Usando la UI da sola, bisogna ricalcolare le misure a mano ([Colori e scala](tema.md)).
- **Tutto è globale.** C'è un solo font (`WhiskFont`), una tabella di icone (`IconsUV`), un menu aperto (`MenuAbierto`) e un campo di testo con il focus (`g_textFieldActivo`) per programma.
- **Gli header mostrano tipi di OpenGL** (`GLfloat`, `GLshort`, `GLuint`), anche se non chiamano più OpenGL.
- **Cosa arriverà,** secondo il `README.md`: ripulire il codice che appartiene all'editor e fare esempi d'uso.

Cosa è della UI e cosa dell'editor, punto per punto: [Cosa è della UI e cosa dell'editor](ui-y-editor.md). Per i dubbi, c'è il gruppo [Telegram](https://t.me/Whisk3D).
