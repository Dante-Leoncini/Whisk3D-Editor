# Colori e scala

## La tavolozza

I colori della UI sono ruoli, non colori fissi: l'enum `ColorID` (`theme/colores.h`).

| Ruolo | A cosa serve |
|---|---|
| `background` | Lo sfondo. Il quadratino di una casella di controllo disattivata e il bordo di un pulsante a riposo |
| `blanco` | Il testo e il bordo di ciò che è sotto il mouse o selezionato |
| `accent` | Il focus: il bordo dei menu, ciò che si sta modificando, l'opzione attiva, il riempimento di uno slider |
| `accentDark` | Lo sfondo del testo selezionato e la barra di un pulsante numerico |
| `negro` | La linea di `PropSeparator` |
| `gris` | Il corpo dei riquadri, dei pulsanti e dei menu |
| `headerColor` | La riga evidenziata di un menu e la guida di uno slider |
| `negroTransparente` | Nero al 25 %. La UI non lo usa |
| `grisUI` | Il testo normale e il quadratino di una casella di controllo attiva |
| `grisLinea` | Le linee che separano i gruppi nei menu |

`ListaColores` li contiene in `float` e `ListaColoresUbyte` in byte. `SetColorID` ne imposta uno come colore di disegno, con un alfa opzionale:

```cpp
SetColorID(ColorID::accent);         // opaco
SetColorID(ColorID::grisUI, 0.5f);   // a media opacidad
```

`loadColorsW3d(ruta)` li legge da un file di testo, un colore per riga: il nome e le quattro componenti da 0 a 1, come `accent 0.0 1.0 0.0 1.0`. Ignora i nomi che non conosce, così lo stesso `skin.ini` può contenere colori di un altro programma. Restituisce `false` se non è riuscita ad aprire il file. Senza il file restano i colori del codice (`theme/colores.cpp`); la skin di Whisk3D li cambia e, per esempio, l'`accent` diventa verde.

Alcuni colori sono scritti nel codice e non vengono dalla tavolozza: il verde del rombo di una modifica non salvata, il rosso di un `PropLabel` di errore, il bordo rosso di un `PropText` incompleto e il grigio di un `PropButton` disattivato.

## La scala

Le misure base sono in `core/UI.h`, in pixel:

| Misura | Valore |
|---|---|
| `margin`, `padding` | 10 |
| `border` | 4 |
| `gap` | 5 |
| `RenglonHeight` | 12 |
| `LetterWidth`, `CharacterWidth` | 6 |
| `LetterHeight` | 11 |

`GlobalScale` le moltiplica, sempre per un intero: così le lettere fatte di pixel restano nitide. Per esempio, 1 sullo schermo da 240 pixel dell'N95 e 3 su un PC.

Ogni misura ha la sua versione già moltiplicata, con `GS` alla fine: `RenglonHeightGS`, `gapGS`, `borderGS`, `bordersGS` (il doppio del bordo), `CharacterWidthGS`, `LetterHeightGS`... I widget usano queste.

La UI non include la funzione che le ricalcola: quella dell'editor è `SetGlobalScale`, in `main/ui/ViewPorts/ViewPorts.cpp`. Usando la UI da sola, si fa così:

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

Le dimensioni di ogni widget si calcolano nel suo `Resize`: dopo aver cambiato la scala, bisogna richiamarlo su ogni riquadro, pulsante e menu.

## Compatto o touch

`UIBotonAltura()` dà l'altezza di un pulsante e `UIBarPadding()` lo spazio sopra e sotto un pulsante in una barra. Entrambe dipendono da `UITactil`:

- `false`, la modalità compatta, pensata per l'N95: il pulsante è alto una riga di testo più i bordi, meno `2 * GlobalScale`, e la barra lascia `GlobalScale` sopra e sotto.
- `true`: il pulsante e la barra aggiungono un `gap`, perché ci stia il dito. I bordi non si ingrossano.

`UITactil` parte da `false` e oggi niente la attiva.
