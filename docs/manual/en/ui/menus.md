# Drop-down menus

`PopupMenu` (`widgets/PopupMenu.h`) is a floating list of options: a bar's menu, a context menu, a picker. Each option (`MenuItem`) has a text and can have:

- an **icon** on the left,
- a **submenu**, with an arrow on the right,
- a **checkbox**, which shows and toggles a `bool`,
- a **slider**, which edits a `float` between two values,
- a **shortcut**, on the right in faint gray ("Shift D"),
- `green`: it's the active option, with the icon and text in `accent`,
- `gray`: it points to a `bool`; while it's `false`, the option is shown at half opacity and doesn't respond,
- `lineUp`: a line that separates it from the group above.

The menu can have a **title** (`title`): it's white, with a line below.

## Building it with a table

The recommended way is a table of `MenuDef`: the text and what each option does go in the same row. There's no number to connect to a `switch` somewhere else.

```cpp
static bool grid = true;
static PopupMenu* recent = NULL;
static void New() { /* ... */ }
static void Open() { /* ... */ }

static const MenuDef ARCHIVO[] = {
    // text      action  shortcut  icon                     submenu     checkbox
    { "Nuevo",   New,  "Ctrl N", ICON(IconType::more) },
    { "Abrir",   Open,  "Ctrl O", ICON(IconType::folder) },
    { "Recientes", NULL, NULL,     ICON(IconType::list),  &recent },
    { "Grilla",  NULL,   NULL,     0,                       NULL,       &grid },
};

recent = new PopupMenu();          // the submenu has to exist before building
PopupMenu* archivo = new PopupMenu();
archivo->title = "Archivo";
archivo->Build(ARCHIVO, 4);
```

`ICON(x)` adds 1 to the icon: in a table, 0 means "no icon". Each option's id is its row.

Two optional hooks, installed by the program:

```cpp
W3dMenuTranslate = MiTraduccion;   // const char* MiTraduccion(const char* texto)
W3dMenuShortcutText = MiShortcut;      // std::string MiAtajo(const std::string& atajo)
```

With `W3dMenuTranslate`, the table's texts come out translated when the menu is built. With `W3dMenuShortcutText`, a shortcut can be a name starting with `@` and the hook returns the matching key. Without that hook, a shortcut with `@` isn't shown.

## Building it by hand

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

`Add` returns the `MenuItem*`, so you can change the rest. When an option is chosen, `Run(id)` calls its `action` if it has one and, if not, the menu's `action(id)`. `Clear()` removes all the options, for menus that change.

## Opening it

```cpp
archivo->Open(mx, my, MenuScreenW, MenuScreenH);
OpenMenu = archivo;
```

It opens at the point you pass: the cursor, or the edge of a button. If it doesn't fit, it shifts back inside the screen. `Open` doesn't touch `OpenMenu`: whoever opens it sets it. `Close()` sets it back to `NULL`.

When it opens, it measures itself:

- The width is that of the longest row, with its text and its right column: the shortcut, the arrow, the checkbox or the slider. If one row has an icon, all of them leave room for it, so the texts stay aligned.
- It's never wider than the screen. If it doesn't fit, each row's text is cut before its right column: the checkboxes stay visible.
- It's never taller than the screen. If it has more rows, it shows the ones that fit, with a scroll bar on the right. It scrolls with the wheel (`Wheel`).

A submenu opens next to its row: to the right if it fits, otherwise to the left. If it doesn't fit on either side, it gets narrower and leaves a column of the menu behind it visible, so you can move the mouse to another option.

## Mouse and finger

```cpp
// on move
if (OpenMenu) OpenMenu->MouseMove(mx, my);

// on click or touch
if (OpenMenu) {
    PopupMenu* m = OpenMenu;
    int id = m->Click(mx, my);
    if (id >= 0) m->Run(id);
}

// when drawing, with a full-screen projection
if (OpenMenu) OpenMenu->Render();
```

`Click` returns the chosen id, `-1` if nothing was chosen or `-2` if it opened a submenu.

- Hovering over a row highlights it and opens its submenu, without clicking.
- If the mouse moves more than `60 * GlobalScale` pixels away, the menu closes by itself. With `closeFar = false`, it doesn't: for a menu that wasn't opened at the mouse.
- A click outside closes it.
- A regular option closes the menu. A checkbox toggles and leaves it open, so you can check several. A click on a slider sets the value according to where it landed.
- Holding down on a slider and dragging changes the value live. Whoever handles the mouse passes each movement to `MenuSliderDragMove(mx)` while `MenuSliderDragActive()` is `true`, and on release calls `MenuSliderDragRelease()`.
- On a touch screen, the tap that opens a submenu doesn't choose anything inside it, even if the submenu ended up under the finger. A second tap is needed.
- What's chosen in a submenu is executed in that submenu: each menu has its own ids.

## Keyboard

The focus is held by the innermost open submenu:

```cpp
PopupMenu* m = OpenMenu;
while (m->openSubmenu && m->openSubmenu->open) m = m->openSubmenu;
MenuItem* it = m->CurrentItem();      // the highlighted option, or NULL
```

| Function | What it does |
|---|---|
| `button_up()`, `button_down()` | Move the highlight, wrapping around. They don't open submenus |
| `OpenSubmenuCurrent()` | Opens the highlighted option's submenu and highlights its first option |
| `FitSlider(+1)`, `FitSlider(-1)` | Move the slider by a twentieth of its range |
| `Enter()` | Like a click on the highlighted option, and returns the same. Called on `OpenMenu`, it goes straight to the innermost submenu |
| `Close()` | Closes the menu and its submenus |

With the keyboard, the highlighted option always stays in view. Which key calls what is up to whoever uses the menu.

`MenuScreenW` and `MenuScreenH` must hold the screen size: they're used to fit the menu.
