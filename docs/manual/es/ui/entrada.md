# Entrada y cajas de texto

## La UI no lee eventos

Whisk3D UI no abre ventanas ni escucha al mouse, al teclado o a la pantalla táctil. Los eventos los recibe el programa, en casi todos los sistemas por SDL2 ([Entrada](../core/entrada.md) del Core), y le pasa a cada widget lo que corresponde:

| Evento | A quién va |
|---|---|
| Se mueve el mouse o el dedo | `PopupMenu::MouseMove(x, y)`; el `hover` de botones y pestañas; `PropHoverGroup` y `PropHoverFila` en las tarjetas |
| Click o toque | `PopupMenu::Click(x, y)`; `Button::Contains` y `Tab::Contains`; en una tarjeta, la fila que calcule el programa |
| Rueda | `PopupMenu::Wheel(dir)` |
| Flechas | `button_up`, `button_down`, `button_left` y `button_right` de menús, tarjetas y filas |
| OK o Enter | `PopupMenu::Enter()`, `GroupPropertie::EnterPropertieSelect()` |
| Cancelar o Esc | `PopupMenu::Cerrar()`, `GroupPropertie::Cancel()` |
| Una letra | La caja de texto enfocada |

Las coordenadas son píxeles de pantalla, con la Y para abajo.

## Sin mouse

Todo se puede usar con cuatro flechas, OK y Cancelar: así se usa en el N95. Lo que pone la UI para eso:

- Las tarjetas se recorren fila por fila y saltean lo que no se puede elegir.
- Los menús se recorren con las flechas, y la opción resaltada nunca se sale de la vista.
- Los números se cambian con las flechas, sin tipear.
- El foco del teclado se ve: `focoMenu` en los botones y `foco` en las pestañas, en `accent`, y el borde blanco de `g_kfFocoProp` sobre un rombo.

Qué tecla del teléfono es cada una, las soft keys o el OK, no es de la UI: lo traducen el editor y cada sistema.

## Pantalla táctil

- Un toque es un click.
- El toque que abre un submenú no elige nada adentro de él ([Menús desplegables](menus.md)).
- `UITactil` agranda los botones y las barras para el dedo ([Colores y escala](tema.md)). Hoy nada la prende.

Arrastrar para desplazar, mantener apretado o tocar dos veces lo resuelve quien usa la UI.

## Cajas de texto

`TextField` (`widgets/TextField.h`) es el estado de una caja de texto: el texto (`text`), la posición del cursor (`caret`) y si está todo seleccionado (`selectAll`). No dibuja ni lee teclas: es solo el dato.

| Función | Qué hace |
|---|---|
| `SetText(t)` | Pone el texto, con el cursor al final |
| `SelectAll()` | Selecciona todo: la primera tecla lo reemplaza |
| `InsertChar(c)` | Escribe un byte donde está el cursor |
| `Backspace()`, `DelForward()` | Borran para atrás o para adelante |
| `CaretIzq()`, `CaretDer()` | Mueven el cursor. Con todo seleccionado, sacan la selección |

Borrar y mover el cursor tratan una letra de varios bytes, como una `ñ` o una cirílica, como una sola. Para escribir una de esas, se pasa cada byte a `InsertChar`.

Hay una sola caja enfocada a la vez: `g_textFieldActivo`, o `NULL` si no hay ninguna. Las teclas van a esa.

```cpp
TextField nombre;
nombre.SetText("Jugador 1");
nombre.SelectAll();
g_textFieldActivo = &nombre;

// con cada letra
if (g_textFieldActivo) g_textFieldActivo->InsertChar(c);

// al dibujar, con el color del texto puesto
RenderTextField(nombre, g_textFieldActivo == &nombre, textAlign::left, 200);
```

`RenderTextField(caja, foco, alineación, ancho)` la dibuja en el origen actual:

- Sin foco, es el texto.
- Con foco y todo seleccionado, el texto va en blanco sobre `accentDark`.
- Con foco y sin selección, aparece el cursor: una rayita que titila cada 530 ms.

`TextFieldParpadeoTick()` devuelve `true` cuando la rayita cambia: es el momento de redibujar.

`TextField.h` también declara `TextFieldEnfocar`, `TextFieldCancelar` (Esc: vuelve al texto de antes de enfocar), `TextFieldInputChar` y `TextFieldFinDeCuadro`. Hoy están definidas en el editor, en `main/ui/ViewPorts/NumInput.cpp`: la UI sola no las trae.
