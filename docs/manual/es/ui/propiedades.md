# Tarjetas y propiedades

## La tarjeta

`GroupPropertie` (`Propieties/GroupPropertie.h`) es una tarjeta con un título y filas. Cada fila es una propiedad, una clase que hereda de `PropertieBase` y apunta a una variable tuya: la tarjeta la muestra y la cambia ahí mismo.

```cpp
GroupPropertie* jugador = new GroupPropertie("Jugador");
jugador->icono = IconType::gamepad;        // opcional: entre la flecha y el título

PropFloat* vel = new PropFloat("Velocidad", " m/s");
vel->value = &velocidad;
jugador->properties.push_back(vel);

jugador->Resize(ancho, 0);    // el alto lo calcula la tarjeta
jugador->Render();            // en el origen actual
```

La tarjeta es dueña de sus filas: al borrarla, las borra.

- El título lleva una flecha para plegarla (`open`). Con `flecha = false` no la tiene: para una ventana flotante.
- Debajo hay dos columnas: la etiqueta a la izquierda y el valor a la derecha. `anchoValores` es la parte del ancho que se lleva el valor: 0.5 por defecto.
- `reservaKeyBtn = true` deja a la derecha de todo una columna para el rombo de animación.
- Casi todas las filas miden un renglón más un `gap`. Un número, una casilla o un color sin variable (`value` en `NULL`) no ocupa lugar ni se dibuja.
- `visible = false` esconde la tarjeta entera.

La fila elegida tiene la etiqueta en `accent`, el valor en blanco y el borde blanco; mientras se edita, el valor y el borde pasan a `accent`. La fila bajo el mouse va en blanco: la marca quien contiene la tarjeta, con `PropHoverGroup` y `PropHoverFila`.

## Elegir y editar

`selectIndex` es la fila elegida: `-2` ninguna, `-1` el título, de `0` en adelante una fila.

| Función | Qué hace |
|---|---|
| `NextSelect()`, `PrevSelect()` | Pasan a la fila de abajo o de arriba. Saltean los espacios y las filas sin variable. Devuelven `true` al salir de la tarjeta, para pasar a la siguiente |
| `EnterPropertieSelect()` | En el título, pliega o despliega la tarjeta. En una fila, la edita |
| `button_up()`, `button_down()`, `button_left()`, `button_right()` | Mientras se edita, van a la fila |
| `Cancel()` | Deja la fila como estaba |

`editando` dice si hay una fila en edición. Con eso, las flechas cambian el valor o mueven la elección:

```cpp
if (jugador->editando) jugador->button_up();   // cambia el valor
else jugador->PrevSelect();                    // la fila de arriba
```

El mouse lo resuelve quien contiene la tarjeta: ella no sabe dónde quedó en la pantalla. Arriba están el borde (`borderGS`) y el título (`RenglonHeightGS + gapGS`). Cada fila mide lo que devuelve su `Resize(ancho)`.

## Las filas

| Clase | Qué es | Cómo se edita |
|---|---|---|
| `PropFloat` | Un número | Con las flechas, o tipeado |
| `PropBool` | Una casilla, pegada a la derecha | OK la prende o la apaga |
| `PropColor` | Un cuadrado del color, pegado a la derecha | OK entra en edición y guarda el color, por si se cancela |
| `PropButton` | Un botón | OK llama a su acción |
| `PropButtonRow` | Varios botones en una fila | Izquierda y derecha eligen uno; OK lo aprieta |
| `PropText` | Una caja de texto | OK la enfoca para escribir |
| `PropListMeshParts` | Una lista con desplazamiento | Arriba y abajo eligen |
| `PropLabel` | Un texto para leer, en una o varias líneas | No se elige |
| `PropSeparator` | Una línea | No se elige |
| `PropGap` | Un espacio de un `gap` | No se elige |
| `PropSelectText` | Una caja con su etiqueta, sin valor | No hace nada todavía |

### Número

```cpp
PropFloat* vida = new PropFloat("Vida");
vida->value = &vidaJugador;
vida->SetRango(0, 100);           // no pasa de ahí
vida->entero = true;              // se muestra y avanza de a enteros
vida->stepFino = 1;               // arriba y abajo
vida->stepGrueso = 10;            // izquierda y derecha
vida->onChange = AlCambiarVida;   // void AlCambiarVida()
```

| Campo | Qué hace |
|---|---|
| `unit` | Un texto pegado al número, como `°`. Es el segundo argumento del constructor |
| `stepFino`, `stepGrueso` | Lo que suman las flechas: 0.01 y 0.1 por defecto |
| `entero` | Muestra el número redondeado, y las flechas arrancan del redondeado. Sin esto lleva cuatro decimales |
| `centrado` | El número en el medio de su caja |
| `flechas` | Dibuja `<` y `>` a los lados. El click en ellas lo resuelve el contenedor |
| `opciones`, `nOpciones` | Con `entero`: el número es un índice y se muestra el nombre de esa opción, traducido con `W3dMenuTraducir` |
| `acelera` | Izquierda y derecha arrancan de a 1 y, si se repiten, suben hasta 10 |
| `dragStep` | Cuánto cambia por píxel al arrastrar. El arrastre lo hace el contenedor |

`Set(v)` asigna el valor dentro del rango y llama a `onChange`. Las flechas pasan por ahí.

Para tipearlo:

```cpp
vida->IniciarEdicionTexto();   // muestra el valor, todo seleccionado
// las teclas van a g_textFieldActivo: ver Entrada y cajas de texto
NumEditCommit();               // Enter: lo aplica
NumEditCancel();               // Esc: queda como estaba
```

`NumEditActivo()` dice si hay un número en edición. Un texto que no es un número vale 0.

### Casilla

`value` apunta a un `bool`. `onChange` se llama cada vez que cambia. La etiqueta ocupa todo el lugar a su izquierda y, si no entra, se corta con `..`.

### Color

`value` apunta a cuatro `float` (RGBA). La UI dibuja el cuadrado; el selector de color no es de la UI. Mientras se edita, las flechas mueven solo el primer componente.

### Botón

`action` es la función que llama. `gris = true` lo atenúa y no hace nada. `oculto = true` no ocupa fila.

Con `conLabel = true`, la fila es un nombre y un botón: el nombre a la izquierda, del ancho de su texto, hasta el 55 % de la fila, y el botón en todo el resto. `labelChars` fija el ancho del nombre, para alinear varias filas. Con `button->desplegable` lleva la flechita de un menú. Además:

- `AgregarPrefijo(accion, ayuda)` pone un cuadrado antes del botón, para mostrar un color.
- `AgregarExtra(icono, accion, ayuda)` pone hasta dos botones chicos de solo ícono a la derecha. `slotsExtras` les reserva lugar, para que queden alineados entre filas. `ExtraEnX(x)` dice cuál quedó en esa x, para el click.

### Fila de botones

```cpp
PropButtonRow* fila = new PropButtonRow();
fila->Agregar("Aceptar", Aceptar);
fila->Agregar("Cancelar", Cancelar);
jugador->properties.push_back(fila);
```

Los botones se reparten el ancho en partes iguales. Los invisibles se saltean y los demás se reparten su lugar. `Disparar(i)` aprieta el botón `i`: lo llama el contenedor cuando el click cae en esa parte.

### Caja de texto

```cpp
PropText* nombre = new PropText("Nombre", "Jugador 1");
// nombre->field.text tiene lo que se escribió
```

`onClick`: en vez de editar, llama a esa función, por ejemplo para abrir un explorador de archivos. `error = true` pinta de rojo el borde de la fila elegida, hasta que se escriba algo. `oculto = true` no ocupa fila. OK la enfoca con `TextFieldEnfocar`, que hoy está definida en el editor. Para escribir, ver [Entrada y cajas de texto](entrada.md).

### Texto

`PropLabel("texto")` es una línea. `PropLabel("texto", true)` corta en los espacios para que entre en el ancho; una palabra que sola no entra queda entera. `rojo = true`, para un error. `oculto = true` no ocupa fila.

### Lista

`PropListMeshParts` muestra `filasMax` filas, 3 por defecto, con una barra chica si hay más. La elegida siempre queda a la vista. `Cancel` vuelve `selectIndex` al que tenía al entrar en edición.

Todavía no sirve para una lista cualquiera: según `modo`, lee una lista de la escena (las partes de una malla, sus mapas UV, sus capas de color, los modificadores, los grupos de vértices, los huesos, las animaciones, las restricciones, los scripts) o la pide a ganchos de funciones del editor (`MixFila*`, `RutinaFila*`, `PostFila*`).

## El rombo de animación

Una fila animable (`animProp` de 0 en adelante, en `PropFloat`, `PropBool`, `PropColor` y `PropButton`) muestra un rombo a la derecha si la tarjeta tiene `reservaKeyBtn`. Su color dice el estado:

| Estado | Color | Qué quiere decir |
|---|---|---|
| 0 | Gris | Sin animación |
| 1 | `accent`, a media luz | Animada, sin un cuadro clave acá |
| 2 | `accent` | Hay un cuadro clave acá |
| 3 | Verde | El valor cambió y no se guardó |

El rombo es de la UI; qué estado tiene, no. Lo pregunta a un gancho que instala el programa, y el click llama al otro:

```cpp
W3dKeyframeEstado = MiEstado;   // int MiEstado(int prop, int comp): de 0 a 3
W3dKeyframeToggle = MiToggle;   // void MiToggle(int prop, int comp): pone o saca el cuadro clave
```

Sin ganchos, el rombo queda gris. La tarjeta no llama a `W3dKeyframeToggle`: lo llama quien resuelve el click. `g_kfFocoProp` apunta a la fila cuyo rombo tiene el foco del teclado, y lo marca con un borde blanco. `W3dRenderRomboKey(estado)` dibuja un rombo suelto.
