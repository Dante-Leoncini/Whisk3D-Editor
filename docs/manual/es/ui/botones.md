# Botones, pestañas y casillas

## Botón

`Button` (`widgets/Button.h`) es una tarjeta con texto y un ícono opcional a la izquierda. El borde oscuro es lo que lo hace ver como botón. Bajo el mouse (`hover`) el borde pasa a blanco y el fondo se aclara un poco.

```cpp
Button* guardar = new Button("Guardar", IconType::guardar);   // el ancho sigue al texto
guardar->Resize(200);                                         // nunca más ancho que esto

gfx::PushMatrix();
gfx::Translatef(x, y, 0);
guardar->Render();
gfx::PopMatrix();

guardar->sx = x; guardar->sy = y;      // dónde quedó, para el mouse
guardar->hover = guardar->Contains(mx, my);
```

`Render` dibuja en el origen actual y no sabe dónde quedó en la pantalla: `sx` y `sy` los pone quien lo dibuja, y `Contains` compara contra ellos. El tercer argumento del constructor es `adaptar`: en `false`, el botón ocupa todo el ancho que le da `Resize`.

| Campo | Qué hace |
|---|---|
| `centrado` | Ícono y texto en el medio, como en Aceptar o Cancelar |
| `cuadrado` | El ancho igual al alto: botones de solo ícono |
| `altoRenglon` | El alto de una fila de propiedades, en vez del de una barra |
| `caretMenu` | Una flechita a la derecha: avisa que abre un menú |
| `tinte` | El color del fondo, en vez de `gris` |
| `colorTexto`, `colorBorde` | El color del texto y el ícono, y el del borde |
| `focoMenu` | Enfocado con el teclado: borde y texto en `accent` |
| `alpha` | La opacidad del fondo y del borde, para que se vea lo de atrás |
| `iconFlip` | El ícono espejado: 1 horizontal, 2 vertical |
| `barra` | Un número de 0 a 1: el botón se llena hasta ahí, en `accentDark`, con el texto en el medio |
| `relleno` | Un cuadrado de ese color adentro: para elegir un color |
| `editField` | Un `TextField`: el botón se dibuja como caja de texto, para renombrar en el lugar |
| `visible` | En `false` no se dibuja |

Hay campos que el botón guarda y no dibuja: los usa quien lo contiene. `desplegable` dice que abre un menú. `separador` pide una línea antes, en una barra. `rol` sirve para encontrarlo en una barra sin depender del orden. `ayuda` es el texto del cartelito que aparece al dejar el mouse encima; el cartelito no es de la UI.

El alto es `UIBotonAltura()` ([Colores y escala](tema.md)), o `RenglonHeightGS + 2 * GlobalScale` con `altoRenglon`. Nunca es más angosto que un renglón.

## Pestaña

`Tab` (`widgets/Tab.h`) es un botón sin el borde de abajo: se funde con lo que tiene debajo. La activa (`activa`) es un poco más alta y del color del contenido; las demás, más oscuras. El texto va en `accent` si tiene el foco del teclado (`foco`), en blanco si es la activa o está bajo el mouse, y en gris si no.

Se usa como el botón: `Resize(maxW)`, `Render()` y `Contains(mx, my)`, con `sx` y `sy` que pone quien la dibuja.

## Casillas y muestras de color

Cómo se ve una casilla lo decide un solo lugar, `widgets/card.h`, así la de un menú y la de una tarjeta no pueden ser distintas.

| Función | Qué hace |
|---|---|
| `DibujarCheckbox(lado, prendida)` | Dibuja la caja y, si está prendida, el tilde |
| `DibujarMuestraColor(lado, rgba)` | Dibuja un cuadrado de ese color, con las esquinas de la casilla |
| `CheckboxLado()` | El lado: el alto de un renglón |
| `CheckboxColorCaja(prendida)`, `CheckboxColorTilde()` | Los colores de la caja y del tilde |
| `CheckboxTilde()` | El carácter del tilde, `✔` |

Prendida, la caja es `grisUI` y el tilde, del color del fondo. Apagada, la caja es del color del fondo y no tiene tilde.

```cpp
DibujarCheckbox(CheckboxLado(), sonidoPrendido);
```
