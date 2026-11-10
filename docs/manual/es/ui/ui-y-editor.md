# Qué es de la UI y qué del editor

El editor está hecho con Whisk3D UI, pero mucho de lo que se ve en él es suyo. Lo de la UI está en `libs/WhiskUI/`; lo del editor, en `main/` del repositorio de Whisk3D.

## De la UI

- El dibujo 2D, las tarjetas con borde y los rectángulos.
- La tabla de íconos y el empaquetador de atlas.
- La fuente bitmap, el atlas extendido y el texto con fuentes TTF horneadas.
- La paleta de colores y las medidas, con su escala.
- Los botones, las pestañas, las casillas y los menús desplegables.
- Las tarjetas de propiedades y sus filas, con el rombo de animación.
- El estado de las cajas de texto y su dibujo.

## Del editor

| Qué | Dónde |
|---|---|
| Los viewports: dividir la pantalla, sus bordes, sus barras de menús y de herramientas | `main/ui/ViewPorts/` (`ViewPorts.cpp`, `WithBorder.cpp`, `ToolbarBase.cpp`) |
| El panel de propiedades: qué tarjetas hay, sus pestañas, el click y el arrastre sobre las filas | `main/ui/ViewPorts/Properties.cpp` |
| Las ventanas modales: el selector de color, el explorador de archivos, las confirmaciones, la barra de progreso, los teclados en pantalla | `main/ui/ViewPorts/PopUp/` |
| Las notificaciones | `main/ui/ViewPorts/Notificaciones.cpp` |
| Las barras de desplazamiento de los paneles | `main/ui/ViewPorts/ScrollBar.cpp` |
| Los cartelitos de ayuda. El texto lo guarda `Button::ayuda` | `W3dAyudaRender`, en `main/ui/ViewPorts/LayoutInput.cpp` |
| Qué tecla, click o toque va a cada widget, y las teclas del N95 | `main/ui/ViewPorts/LayoutInput.cpp`, `main/app/controles.cpp`, `platform/symbian/src/` |
| Las funciones de foco de las cajas de texto | `main/ui/ViewPorts/NumInput.cpp` |
| Las traducciones, `T()`. La UI solo tiene el gancho `W3dMenuTraducir` | `main/config/W3dLang.cpp` |
| El arranque de la UI | `main/app/W3dInitUI.cpp` |
| Armar el atlas del skin con los íconos sueltos | `main/io/SkinAtlas.cpp` |
| Recalcular la escala | `SetGlobalScale`, en `main/ui/ViewPorts/ViewPorts.cpp` |
| Leer `skin.ini` en la PC y en Android | `loadColors`, en `main/app/main.cpp` |
| Los colores propios del editor: el piso, los ejes, las normales | `main/ui/W3dColors.cpp` |
| Los archivos del skin: `font.png`, `font-ext.png`, `atlas/iconos/`, `skin.ini` | `res/Skins/Whisk3D/` |
| El generador del atlas extendido | `tools/fuente_ext.py` |
| El [Editor 2D](../editor/editor-2d.md) y las interfaces `.w3dui` de los juegos: textos, imágenes, rectángulos, contenedores, slice 9, botones y videos | `main/objects/` (`UI.h`, `Texto2D.h`, `Boton2D.h`...), `main/render/UIOverlay.cpp`, `main/io/UI2DFormato.cpp`, `main/io/Fuente2D.cpp`, `main/ui/ViewPorts/Editor2D.cpp` |

El Editor 2D dibuja el texto con `W3dTextAtlas`, que es de la UI. Hornear una TTF mientras corre, con stb_truetype, es del editor (`main/io/Fuente2D.cpp`).

## En la UI, pero del editor

Cosas que están en `libs/WhiskUI/` y todavía son del editor:

- `PropListMeshParts` lee mallas, esqueletos, animaciones, restricciones y scripts, y llama a `RebindMaterialMeshPart()` del panel de propiedades.
- `PropColor` guarda la paleta y el objeto de donde sale el color (`palRef`, `palObj`), para el selector de color.
- `GroupProperties` es la lista global de tarjetas del panel de propiedades.
- `core/UI.h` tiene estado del editor: el modo de vista táctil del Modo Edición (`g_viewEditMode`), el movimiento del mouse (`dx`, `dy`), las teclas Alt, Shift y Ctrl (`LAltPressed`, `LShiftPressed`, `LCtrlPressed`) y las medidas de los viewports (`paddingViewport`, `MinViewportHeight`, `MinViewportWidth`).
- La mayoría de los íconos son herramientas del editor.

Lo que falta separar, y el estado de la biblioteca: el final de [Usar la UI directamente](empezar.md).
