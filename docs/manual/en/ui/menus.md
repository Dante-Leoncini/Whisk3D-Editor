# Drop-down menus

`PopupMenu` (`widgets/PopupMenu.h`) is a floating list of options: a bar's menu, a context menu, a picker. Each option (`MenuItem`) has a text and can have:

- an **icon** on the left,
- a **submenu**, with an arrow on the right,
- a **checkbox**, which shows and toggles a `bool`,
- a **slider**, which edits a `float` between two values,
- a **shortcut**, on the right in faint gray ("Shift D"),
- `verde`: it's the active option, with the icon and text in `accent`,
- `gris`: it points to a `bool`; while it's `false`, the option is shown at half opacity and doesn't respond,
- `lineaArriba`: a line that separates it from the group above.

The menu can have a **title** (`titulo`): it's white, with a line below.

## Building it with a table

The recommended way is a table of `MenuDef`: the text and what each option does go in the same row. There's no number to connect to a `switch` somewhere else.

```cpp
static bool grilla = true;
static PopupMenu* recientes = NULL;
static void Nuevo() { /* ... */ }
static void Abrir() { /* ... */ }

static const MenuDef ARCHIVO[] = {
    // text      action  shortcut  icon                     submenu     checkbox
    { "Nuevo",   Nuevo,  "Ctrl N", ICONO(IconType::mas) },
    { "Abrir",   Abrir,  "Ctrl O", ICONO(IconType::carpeta) },
    { "Recientes", NULL, NULL,     ICONO(IconType::lista),  &recientes },
    { "Grilla",  NULL,   NULL,     0,                       NULL,       &grilla },
};

recientes = new PopupMenu();          // the submenu has to exist before building
PopupMenu* archivo = new PopupMenu();
archivo->titulo = "Archivo";
archivo->Construir(ARCHIVO, 4);
```

`ICONO(x)` adds 1 to the icon: in a table, 0 means "no icon". Each option's id is its row.

Two optional hooks, installed by the program:

```cpp
W3dMenuTraducir = MiTraduccion;   // const char* MiTraduccion(const char* texto)
W3dMenuAtajoTexto = MiAtajo;      // std::string MiAtajo(const std::string& atajo)
```

With `W3dMenuTraducir`, the table's texts come out translated when the menu is built. With `W3dMenuAtajoTexto`, a shortcut can be a name starting with `@` and the hook returns the matching key. Without that hook, a shortcut with `@` isn't shown.

## Building it by hand

```cpp
PopupMenu* vista = new PopupMenu();
vista->Agregar("Frente", 0, IconType::camera);
vista->AgregarCheck("Mostrar piso", 1, &verPiso);
vista->AgregarFloat("Tamaño", 2, &tamanio, 0.0f, 4.0f);
MenuItem* it = vista->Agregar("Alambre", 3);
it->verde = esAlambre;
it->lineaArriba = true;
vista->action = AlElegir;              // void AlElegir(int id)
```

`Agregar` returns the `MenuItem*`, so you can change the rest. When an option is chosen, `Ejecutar(id)` calls its `accion` if it has one and, if not, the menu's `action(id)`. `Limpiar()` removes all the options, for menus that change.

## Opening it

```cpp
archivo->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
MenuAbierto = archivo;
```

It opens at the point you pass: the cursor, or the edge of a button. If it doesn't fit, it shifts back inside the screen. `Abrir` doesn't touch `MenuAbierto`: whoever opens it sets it. `Cerrar()` sets it back to `NULL`.

When it opens, it measures itself:

- The width is that of the longest row, with its text and its right column: the shortcut, the arrow, the checkbox or the slider. If one row has an icon, all of them leave room for it, so the texts stay aligned.
- It's never wider than the screen. If it doesn't fit, each row's text is cut before its right column: the checkboxes stay visible.
- It's never taller than the screen. If it has more rows, it shows the ones that fit, with a scroll bar on the right. It scrolls with the wheel (`Wheel`).

A submenu opens next to its row: to the right if it fits, otherwise to the left. If it doesn't fit on either side, it gets narrower and leaves a column of the menu behind it visible, so you can move the mouse to another option.

## Mouse and finger

```cpp
// on move
if (MenuAbierto) MenuAbierto->MouseMove(mx, my);

// on click or touch
if (MenuAbierto) {
    PopupMenu* m = MenuAbierto;
    int id = m->Click(mx, my);
    if (id >= 0) m->Ejecutar(id);
}

// when drawing, with a full-screen projection
if (MenuAbierto) MenuAbierto->Render();
```

`Click` returns the chosen id, `-1` if nothing was chosen or `-2` if it opened a submenu.

- Hovering over a row highlights it and opens its submenu, without clicking.
- If the mouse moves more than `60 * GlobalScale` pixels away, the menu closes by itself. With `cerrarLejos = false`, it doesn't: for a menu that wasn't opened at the mouse.
- A click outside closes it.
- A regular option closes the menu. A checkbox toggles and leaves it open, so you can check several. A click on a slider sets the value according to where it landed.
- Holding down on a slider and dragging changes the value live. Whoever handles the mouse passes each movement to `MenuSliderDragMover(mx)` while `MenuSliderDragActivo()` is `true`, and on release calls `MenuSliderDragSoltar()`.
- On a touch screen, the tap that opens a submenu doesn't choose anything inside it, even if the submenu ended up under the finger. A second tap is needed.
- What's chosen in a submenu is executed in that submenu: each menu has its own ids.

## Keyboard

The focus is held by the innermost open submenu:

```cpp
PopupMenu* m = MenuAbierto;
while (m->submenuAbierto && m->submenuAbierto->abierto) m = m->submenuAbierto;
MenuItem* it = m->ItemActual();      // the highlighted option, or NULL
```

| Function | What it does |
|---|---|
| `button_up()`, `button_down()` | Move the highlight, wrapping around. They don't open submenus |
| `AbrirSubmenuActual()` | Opens the highlighted option's submenu and highlights its first option |
| `AjustarSlider(+1)`, `AjustarSlider(-1)` | Move the slider by a twentieth of its range |
| `Enter()` | Like a click on the highlighted option, and returns the same. Called on `MenuAbierto`, it goes straight to the innermost submenu |
| `Cerrar()` | Closes the menu and its submenus |

With the keyboard, the highlighted option always stays in view. Which key calls what is up to whoever uses the menu.

`MenuPantallaW` and `MenuPantallaH` must hold the screen size: they're used to fit the menu.
