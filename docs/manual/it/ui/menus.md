# Menu a tendina

`PopupMenu` (`widgets/PopupMenu.h`) è un elenco fluttuante di opzioni: il menu di una barra, un menu contestuale, un selettore. Ogni opzione (`MenuItem`) ha un testo e può avere:

- un'**icona** a sinistra,
- un **sottomenu**, con una freccia a destra,
- una **casella di controllo**, che mostra e cambia un `bool`,
- uno **slider**, che modifica un `float` tra due valori,
- una **scorciatoia**, a destra e in grigio tenue ("Shift D"),
- `green`: è l'opzione attiva, con l'icona e il testo in `accent`,
- `gray`: punta a un `bool`; finché vale `false`, l'opzione appare a metà opacità e non risponde,
- `lineUp`: una linea che la separa dal gruppo di sopra.

Il menu può avere un **titolo** (`title`): è in bianco, con una linea sotto.

## Costruirlo con una tabella

Il modo consigliato è una tabella di `MenuDef`: il testo e ciò che fa ogni opzione stanno nella stessa riga. Non c'è un numero da collegare a uno `switch` da un'altra parte.

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

`ICON(x)` aggiunge 1 all'icona: in una tabella, lo 0 significa "senza icona". L'id di ogni opzione è la sua riga.

Due hook opzionali, che installa il programma:

```cpp
W3dMenuTranslate = MiTraduccion;   // const char* MiTraduccion(const char* texto)
W3dMenuShortcutText = MiShortcut;      // std::string MiAtajo(const std::string& atajo)
```

Con `W3dMenuTranslate`, i testi della tabella escono tradotti quando si costruisce il menu. Con `W3dMenuShortcutText`, una scorciatoia può essere un nome che inizia con `@` e l'hook restituisce il tasto corrispondente. Senza quell'hook, una scorciatoia con `@` non viene mostrata.

## Costruirlo a mano

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

`Add` restituisce il `MenuItem*`, per cambiarne il resto. Quando si sceglie un'opzione, `Run(id)` chiama la sua `action` se ne ha una e, altrimenti, l'`action(id)` del menu. `Clear()` elimina tutte le opzioni, per i menu che cambiano.

## Aprirlo

```cpp
archivo->Open(mx, my, MenuScreenW, MenuScreenH);
OpenMenu = archivo;
```

Si apre nel punto che gli passi: il cursore, o il bordo di un pulsante. Se non ci sta, si sposta verso l'interno dello schermo. `Open` non tocca `OpenMenu`: lo imposta chi lo apre. `Close()` lo rimette a `NULL`.

Quando si apre, prende le misure:

- La larghezza è quella della riga più lunga, con il suo testo e la sua colonna destra: la scorciatoia, la freccia, la casella o lo slider. Se una riga ha un'icona, tutte le lasciano lo spazio, così i testi restano allineati.
- Non è mai più largo dello schermo. Se non ci sta, il testo di ogni riga viene tagliato prima della sua colonna destra: le caselle restano visibili.
- Non è mai più alto dello schermo. Se ha più righe, mostra quelle che ci stanno, con una barra di scorrimento a destra. Si scorre con la rotella (`Wheel`).

Un sottomenu si apre accanto alla sua riga: a destra se ci sta, altrimenti a sinistra. Se non ci sta da nessuna parte, si restringe e lascia visibile una colonna del menu che sta dietro, per poter passare con il mouse su un'altra opzione.

## Mouse e dito

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

`Click` restituisce l'id scelto, `-1` se non ha scelto niente o `-2` se ha aperto un sottomenu.

- Passare con il mouse evidenzia la riga e apre il suo sottomenu, senza clic.
- Se il mouse si allontana più di `60 * GlobalScale` pixel, il menu si chiude da solo. Con `closeFar = false`, no: per un menu che non si è aperto sul mouse.
- Un clic fuori lo chiude.
- Un'opzione normale chiude il menu. Una casella cambia e lo lascia aperto, per poterne attivare diverse. Un clic su uno slider imposta il valore in base a dove è caduto.
- Tenere premuto su uno slider e trascinare cambia il valore in tempo reale. Chi gestisce il mouse passa ogni movimento a `MenuSliderDragMove(mx)`, finché `MenuSliderDragActive()` è `true`, e al rilascio chiama `MenuSliderDragRelease()`.
- Su uno schermo touch, il tocco che apre un sottomenu non sceglie niente al suo interno, anche se il sottomenu è finito sotto il dito. Serve un secondo tocco.
- Ciò che si sceglie in un sottomenu si esegue in quel sottomenu: ogni menu ha i propri id.

## Tastiera

Il focus ce l'ha il sottomenu aperto più interno:

```cpp
PopupMenu* m = OpenMenu;
while (m->openSubmenu && m->openSubmenu->open) m = m->openSubmenu;
MenuItem* it = m->CurrentItem();      // la opción resaltada, o NULL
```

| Funzione | Cosa fa |
|---|---|
| `button_up()`, `button_down()` | Spostano l'evidenziazione, ricominciando dall'altro capo. Non aprono sottomenu |
| `OpenSubmenuCurrent()` | Apre il sottomenu dell'opzione evidenziata ed evidenzia la sua prima opzione |
| `FitSlider(+1)`, `FitSlider(-1)` | Spostano lo slider di un ventesimo del suo intervallo |
| `Enter()` | Come un clic sull'opzione evidenziata, e restituisce lo stesso valore. Chiamato su `OpenMenu`, arriva da solo al sottomenu più interno |
| `Close()` | Chiude il menu e i suoi sottomenu |

Con la tastiera, l'opzione evidenziata resta sempre visibile. Quale tasto chiama cosa lo decide chi usa il menu.

`MenuScreenW` e `MenuScreenH` devono contenere la dimensione dello schermo: servono a sistemare il menu.
