# Colores y escala

## La paleta

Los colores de la UI son roles, no colores fijos: el enum `ColorID` (`theme/colores.h`).

| Rol | Para qué se usa |
|---|---|
| `background` | El fondo. La caja de una casilla apagada y el borde de un botón en reposo |
| `blanco` | El texto y el borde de lo que está bajo el mouse o elegido |
| `accent` | El foco: el borde de los menús, lo que se está editando, la opción activa, el relleno de un deslizador |
| `accentDark` | El fondo del texto seleccionado y la barra de un botón numérico |
| `negro` | La línea de `PropSeparator` |
| `gris` | El cuerpo de las tarjetas, los botones y los menús |
| `headerColor` | La fila resaltada de un menú y la pista de un deslizador |
| `negroTransparente` | Negro al 25 %. La UI no lo usa |
| `grisUI` | El texto normal y la caja de una casilla prendida |
| `grisLinea` | Las líneas que separan grupos en los menús |

`ListaColores` los tiene en `float` y `ListaColoresUbyte` en bytes. `SetColorID` pone uno como color de dibujo, con un alfa opcional:

```cpp
SetColorID(ColorID::accent);         // opaco
SetColorID(ColorID::grisUI, 0.5f);   // a media opacidad
```

`loadColorsW3d(ruta)` los lee de un archivo de texto, un color por línea: el nombre y los cuatro componentes de 0 a 1, como `accent 0.0 1.0 0.0 1.0`. Ignora los nombres que no conoce, así el mismo `skin.ini` puede traer colores de otro programa. Devuelve `false` si no pudo abrir el archivo. Sin el archivo quedan los colores del código (`theme/colores.cpp`); el skin de Whisk3D los cambia, y por ejemplo el `accent` pasa a verde.

Algunos colores están escritos en el código y no salen de la paleta: el verde del rombo de un cambio sin guardar, el rojo de un `PropLabel` de error, el borde rojo de un `PropText` incompleto y el gris de un `PropButton` apagado.

## La escala

Las medidas base están en `core/UI.h`, en píxeles:

| Medida | Valor |
|---|---|
| `margin`, `padding` | 10 |
| `border` | 4 |
| `gap` | 5 |
| `RenglonHeight` | 12 |
| `LetterWidth`, `CharacterWidth` | 6 |
| `LetterHeight` | 11 |

`GlobalScale` las multiplica, siempre por un entero: así las letras de píxeles siguen nítidas. Por ejemplo, 1 en la pantalla de 240 píxeles del N95 y 3 en una PC.

Cada medida tiene su versión ya multiplicada, con `GS` al final: `RenglonHeightGS`, `gapGS`, `borderGS`, `bordersGS` (el doble del borde), `CharacterWidthGS`, `LetterHeightGS`... Los widgets usan esas.

La UI no trae la función que las recalcula: la del editor es `SetGlobalScale`, en `main/ui/ViewPorts/ViewPorts.cpp`. Sola, se hace así:

```cpp
void Escala(int s) {
    GlobalScale = s;
    marginGS = margin * s;  paddingGS = padding * s;  gapGS = gap * s;
    borderGS = border * s;  bordersGS = borderGS * 2;
    RenglonHeightGS = RenglonHeight * s;
    LetterWidthGS = LetterWidth * s;  LetterHeightGS = LetterHeight * s;
    CharacterWidthGS = CharacterWidth * s;
    SetIconScale(s);
    if (WhiskFont) WhiskFont->SetScale(s);
}
```

Los tamaños de cada widget se calculan en su `Resize`: después de cambiar la escala, hay que volver a llamarlo en cada tarjeta, botón y menú.

## Compacto o táctil

`UIBotonAltura()` da el alto de un botón y `UIBarPadding()` el aire arriba y abajo de un botón en una barra. Los dos dependen de `UITactil`:

- `false`, el modo compacto, pensado para el N95: el botón mide un renglón más los bordes, menos `2 * GlobalScale`, y la barra deja `GlobalScale` arriba y abajo.
- `true`: el botón y la barra suman un `gap`, para que entre el dedo. Los bordes no engordan.

`UITactil` arranca en `false` y hoy nada la prende.
