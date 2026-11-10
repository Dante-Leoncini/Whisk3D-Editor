# Menús desplegables

`PopupMenu` (`widgets/PopupMenu.h`) es una lista flotante de opciones: el menú de una barra, un menú contextual, un selector. Cada opción (`MenuItem`) tiene un texto y puede tener:

- un **ícono** a la izquierda,
- un **submenú**, con una flecha a la derecha,
- una **casilla**, que muestra y cambia un `bool`,
- un **deslizador**, que edita un `float` entre dos valores,
- un **atajo**, a la derecha y en gris tenue ("Shift D"),
- `verde`: es la opción activa, con el ícono y el texto en `accent`,
- `gris`: apunta a un `bool`; mientras valga `false`, la opción se ve a media opacidad y no responde,
- `lineaArriba`: una línea que la separa del grupo de arriba.

El menú puede tener un **título** (`titulo`): va en blanco, con una línea abajo.

## Armarlo con una tabla

La forma recomendada es una tabla de `MenuDef`: el texto y lo que hace cada opción van en la misma fila. No hay un número que conectar con un `switch` en otro lado.

```cpp
static bool grilla = true;
static PopupMenu* recientes = NULL;
static void Nuevo() { /* ... */ }
static void Abrir() { /* ... */ }

static const MenuDef ARCHIVO[] = {
    // texto     acción  atajo     ícono                    submenú     casilla
    { "Nuevo",   Nuevo,  "Ctrl N", ICONO(IconType::mas) },
    { "Abrir",   Abrir,  "Ctrl O", ICONO(IconType::carpeta) },
    { "Recientes", NULL, NULL,     ICONO(IconType::lista),  &recientes },
    { "Grilla",  NULL,   NULL,     0,                       NULL,       &grilla },
};

recientes = new PopupMenu();          // el submenú tiene que existir antes de construir
PopupMenu* archivo = new PopupMenu();
archivo->titulo = "Archivo";
archivo->Construir(ARCHIVO, 4);
```

`ICONO(x)` le suma 1 al ícono: en una tabla, el 0 quiere decir "sin ícono". El id de cada opción es su fila.

Dos ganchos opcionales, que instala el programa:

```cpp
W3dMenuTraducir = MiTraduccion;   // const char* MiTraduccion(const char* texto)
W3dMenuAtajoTexto = MiAtajo;      // std::string MiAtajo(const std::string& atajo)
```

Con `W3dMenuTraducir`, los textos de la tabla salen traducidos al construir el menú. Con `W3dMenuAtajoTexto`, un atajo puede ser un nombre que empieza con `@` y el gancho devuelve la tecla que corresponde. Sin ese gancho, un atajo con `@` no se muestra.

## Armarlo a mano

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

`Agregar` devuelve el `MenuItem*`, para cambiarle lo demás. Al elegir una opción, `Ejecutar(id)` llama a su `accion` si tiene una y, si no, al `action(id)` del menú. `Limpiar()` borra todas las opciones, para los menús que cambian.

## Abrirlo

```cpp
archivo->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
MenuAbierto = archivo;
```

Se abre en el punto que le pasás: el cursor, o el borde de un botón. Si no entra, se corre para adentro de la pantalla. `Abrir` no toca `MenuAbierto`: lo pone quien lo abre. `Cerrar()` lo vuelve a `NULL`.

Al abrirse se mide:

- El ancho es el de la fila más larga, con su texto y su columna derecha: el atajo, la flecha, la casilla o el deslizador. Si una fila tiene ícono, todas le dejan el lugar, así los textos quedan alineados.
- Nunca es más ancho que la pantalla. Si no entra, el texto de cada fila se corta antes de su columna derecha: las casillas se siguen viendo.
- Nunca es más alto que la pantalla. Si tiene más filas, muestra las que entran, con una barra de desplazamiento a la derecha. Se mueve con la rueda (`Wheel`).

Un submenú se abre al lado de su fila: a la derecha si entra, si no a la izquierda. Si no entra de ningún lado, se angosta y deja a la vista una columna del menú de atrás, para poder pasar el mouse a otra opción.

## Mouse y dedo

```cpp
// al mover
if (MenuAbierto) MenuAbierto->MouseMove(mx, my);

// al hacer click o tocar
if (MenuAbierto) {
    PopupMenu* m = MenuAbierto;
    int id = m->Click(mx, my);
    if (id >= 0) m->Ejecutar(id);
}

// al dibujar, con una proyección de pantalla completa
if (MenuAbierto) MenuAbierto->Render();
```

`Click` devuelve el id elegido, `-1` si no eligió nada o `-2` si abrió un submenú.

- Pasar el mouse resalta la fila y abre su submenú, sin click.
- Si el mouse se aleja más de `60 * GlobalScale` píxeles, el menú se cierra solo. Con `cerrarLejos = false`, no: para un menú que no se abrió en el mouse.
- Un click afuera lo cierra.
- Una opción común cierra el menú. Una casilla cambia y lo deja abierto, para prender varias. Un click en un deslizador pone el valor según dónde cayó.
- Mantener apretado sobre un deslizador y arrastrar cambia el valor en vivo. Quien maneja el mouse le pasa cada movimiento a `MenuSliderDragMover(mx)`, mientras `MenuSliderDragActivo()` sea `true`, y al soltar llama a `MenuSliderDragSoltar()`.
- En una pantalla táctil, el toque que abre un submenú no elige nada adentro de él, aunque el submenú haya quedado bajo el dedo. Hace falta un segundo toque.
- Lo que se elige en un submenú se ejecuta en ese submenú: cada menú tiene sus propios ids.

## Teclado

El foco lo tiene el submenú abierto más adentro:

```cpp
PopupMenu* m = MenuAbierto;
while (m->submenuAbierto && m->submenuAbierto->abierto) m = m->submenuAbierto;
MenuItem* it = m->ItemActual();      // la opción resaltada, o NULL
```

| Función | Qué hace |
|---|---|
| `button_up()`, `button_down()` | Mueven el resaltado, dando la vuelta. No abren submenús |
| `AbrirSubmenuActual()` | Abre el submenú de la opción resaltada y resalta su primera opción |
| `AjustarSlider(+1)`, `AjustarSlider(-1)` | Mueven el deslizador un veinteavo de su rango |
| `Enter()` | Como un click en la opción resaltada, y devuelve lo mismo. Llamado en `MenuAbierto`, llega solo al submenú de más adentro |
| `Cerrar()` | Cierra el menú y sus submenús |

Con el teclado, la opción resaltada siempre queda a la vista. Qué tecla llama a qué lo decide quien usa el menú.

`MenuPantallaW` y `MenuPantallaH` tienen que tener el tamaño de la pantalla: con ellos se acomoda el menú.
