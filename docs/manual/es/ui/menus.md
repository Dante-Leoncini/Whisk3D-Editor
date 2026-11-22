# Menús desplegables

`PopupMenu` (`widgets/PopupMenu.h`) es una lista flotante de opciones: el menú de una barra, un menú contextual, un selector. Cada opción (`MenuItem`) tiene un texto y puede tener:

- un **ícono** a la izquierda,
- un **submenú**, con una flecha a la derecha,
- una **casilla**, que muestra y cambia un `bool`,
- un **deslizador**, que edita un `float` entre dos valores,
- un **atajo**, a la derecha y en gris tenue ("Shift D"),
- `green`: es la opción activa, con el ícono y el texto en `accent`,
- `gray`: apunta a un `bool`; mientras valga `false`, la opción se ve a media opacidad y no responde,
- `lineUp`: una línea que la separa del grupo de arriba.

El menú puede tener un **título** (`title`): va en blanco, con una línea abajo.

## Armarlo con una tabla

La forma recomendada es una tabla de `MenuDef`: el texto y lo que hace cada opción van en la misma fila. No hay un número que conectar con un `switch` en otro lado.

```cpp
static bool grid = true;
static PopupMenu* recent = NULL;
static void New() { /* ... */ }
static void Open() { /* ... */ }

static const MenuDef ARCHIVO[] = {
    // texto     acción  atajo     ícono                    submenú     casilla
    { "Nuevo",   New,  "Ctrl N", ICON(IconType::more) },
    { "Abrir",   Open,  "Ctrl O", ICON(IconType::folder) },
    { "Recientes", NULL, NULL,     ICON(IconType::list),  &recent },
    { "Grilla",  NULL,   NULL,     0,                       NULL,       &grid },
};

recent = new PopupMenu();          // el submenú tiene que existir antes de construir
PopupMenu* archivo = new PopupMenu();
archivo->title = "Archivo";
archivo->Build(ARCHIVO, 4);
```

`ICON(x)` le suma 1 al ícono: en una tabla, el 0 quiere decir "sin ícono". El id de cada opción es su fila.

Dos ganchos opcionales, que instala el programa:

```cpp
W3dMenuTranslate = MiTraduccion;   // const char* MiTraduccion(const char* texto)
W3dMenuShortcutText = MiShortcut;      // std::string MiAtajo(const std::string& atajo)
```

Con `W3dMenuTranslate`, los textos de la tabla salen traducidos al construir el menú. Con `W3dMenuShortcutText`, un atajo puede ser un nombre que empieza con `@` y el gancho devuelve la tecla que corresponde. Sin ese gancho, un atajo con `@` no se muestra.

## Armarlo a mano

```cpp
PopupMenu* view = new PopupMenu();
view->Add("Frente", 0, IconType::camera);
view->AddCheck("Mostrar piso", 1, &showFloor);
view->AddFloat("Tamaño", 2, &size, 0.0f, 4.0f);
MenuItem* it = view->Add("Alambre", 3);
it->green = isWire;
it->lineUp = true;
view->action = OnPick;              // void AlElegir(int id)
```

`Add` devuelve el `MenuItem*`, para cambiarle lo demás. Al elegir una opción, `Run(id)` llama a su `action` si tiene una y, si no, al `action(id)` del menú. `Clear()` borra todas las opciones, para los menús que cambian.

## Abrirlo

```cpp
archivo->Open(mx, my, MenuScreenW, MenuScreenH);
OpenMenu = archivo;
```

Se abre en el punto que le pasás: el cursor, o el borde de un botón. Si no entra, se corre para adentro de la pantalla. `Open` no toca `OpenMenu`: lo pone quien lo abre. `Close()` lo vuelve a `NULL`.

Al abrirse se mide:

- El ancho es el de la fila más larga, con su texto y su columna derecha: el atajo, la flecha, la casilla o el deslizador. Si una fila tiene ícono, todas le dejan el lugar, así los textos quedan alineados.
- Nunca es más ancho que la pantalla. Si no entra, el texto de cada fila se corta antes de su columna derecha: las casillas se siguen viendo.
- Nunca es más alto que la pantalla. Si tiene más filas, muestra las que entran, con una barra de desplazamiento a la derecha. Se mueve con la rueda (`Wheel`).

Un submenú se abre al lado de su fila: a la derecha si entra, si no a la izquierda. Si no entra de ningún lado, se angosta y deja a la vista una columna del menú de atrás, para poder pasar el mouse a otra opción.

## Mouse y dedo

```cpp
// al mover
if (OpenMenu) OpenMenu->MouseMove(mx, my);

// al hacer click o tocar
if (OpenMenu) {
    PopupMenu* m = OpenMenu;
    int id = m->Click(mx, my);
    if (id >= 0) m->Run(id);
}

// al dibujar, con una proyección de pantalla completa
if (OpenMenu) OpenMenu->Render();
```

`Click` devuelve el id elegido, `-1` si no eligió nada o `-2` si abrió un submenú.

- Pasar el mouse resalta la fila y abre su submenú, sin click.
- Si el mouse se aleja más de `60 * GlobalScale` píxeles, el menú se cierra solo. Con `closeFar = false`, no: para un menú que no se abrió en el mouse.
- Un click afuera lo cierra.
- Una opción común cierra el menú. Una casilla cambia y lo deja abierto, para prender varias. Un click en un deslizador pone el valor según dónde cayó.
- Mantener apretado sobre un deslizador y arrastrar cambia el valor en vivo. Quien maneja el mouse le pasa cada movimiento a `MenuSliderDragMove(mx)`, mientras `MenuSliderDragActive()` sea `true`, y al soltar llama a `MenuSliderDragRelease()`.
- En una pantalla táctil, el toque que abre un submenú no elige nada adentro de él, aunque el submenú haya quedado bajo el dedo. Hace falta un segundo toque.
- Lo que se elige en un submenú se ejecuta en ese submenú: cada menú tiene sus propios ids.

## Teclado

El foco lo tiene el submenú abierto más adentro:

```cpp
PopupMenu* m = OpenMenu;
while (m->openSubmenu && m->openSubmenu->open) m = m->openSubmenu;
MenuItem* it = m->CurrentItem();      // la opción resaltada, o NULL
```

| Función | Qué hace |
|---|---|
| `button_up()`, `button_down()` | Mueven el resaltado, dando la vuelta. No abren submenús |
| `OpenSubmenuCurrent()` | Abre el submenú de la opción resaltada y resalta su primera opción |
| `FitSlider(+1)`, `FitSlider(-1)` | Mueven el deslizador un veinteavo de su rango |
| `Enter()` | Como un click en la opción resaltada, y devuelve lo mismo. Llamado en `OpenMenu`, llega solo al submenú de más adentro |
| `Close()` | Cierra el menú y sus submenús |

Con el teclado, la opción resaltada siempre queda a la vista. Qué tecla llama a qué lo decide quien usa el menú.

`MenuScreenW` y `MenuScreenH` tienen que tener el tamaño de la pantalla: con ellos se acomoda el menú.
