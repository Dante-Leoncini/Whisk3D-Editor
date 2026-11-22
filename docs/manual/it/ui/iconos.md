# Icone

## I file

Ogni icona è un PNG di **10x10** in `res/Skins/Whisk3D/atlas/icons/`, nel repository dell'editor. Oggi sono 106. I pixel sono bianchi su sfondo trasparente: l'icona prende il colore con cui viene disegnata. Così la stessa icona va bene in grigio, in bianco o nel colore di accento.

## L'enum e i nomi

`draw/icons.h` contiene l'enum `IconType` e il totale, `ICON_TOTAL` (106). In `draw/icons.cpp`, `KNames` dà il nome di ciascuna, nello stesso ordine: è il nome del file senza `.png`.

```cpp
IconName(IconType::folder);   // "carpeta"
IconIndex("carpeta");           // IconType::carpeta, o -1 si no existe
```

Quasi tutte si chiamano come nell'enum. L'eccezione è `lockOpen`, che corrisponde al file `candado_abierto.png`.

Molte sono dell'editor: strumenti di modellazione, tipi di oggetto, curve di animazione. Ci sono anche quelle generali: `arrow` e `arrowRight` (le frecce per comprimere), `visible` e `hidden`, `folder`, `save`, `trash`, `plus` e `minus`, `list` e `grid`, `lock`, `notifOk` e `notifError`, `scissors`, `copy` e `paste`.

## Le coordinate

`IconsUV` è un `std::vector<IconUV*>`, con le coordinate di ogni icona nell'atlas (`uvs`, 8 numeri). Si riempie in due modi:

- `CreateIcons(width, height)`: con la tabella che indica dove si trova ogni icona dentro `font.png` (`TableLegacy`, in `icons.cpp`). È il modo senza la cartella `atlas/icons/`.
- `CreateIconsFrom(rects, width, height)`: con un elenco proprio di rettangoli, quello dell'atlas costruito. Un rettangolo con `x = -1` usa quello di `font.png`: un'icona senza il suo PNG continua ad apparire.

`SetIconScale(scale)` regola `IconSizeGS` e le due mesh: `IconMesh`, il quadrato di un'icona, e `IconLineMesh`, un quadrato alto quanto una riga di testo, per le linee degli alberi.

## Disegnare un'icona

Con l'atlas impostato, nell'origine attuale:

```cpp
SetColorID(ColorID::grayUI);
W3dDrawStrip4(IconMesh, IconsUV[IconType::folder]->uvs);
```

`Button`, `Tab`, `PopupMenu` e `GroupPropertie` ricevono l'icona come un `int` dell'enum, o `-1` per nessuna.

## Aggiungerne una

1. Il PNG di 10x10, in `atlas/icons/`.
2. Una riga alla fine dell'enum `IconType`, e aggiungere uno a `ICON_TOTAL`.
3. Il suo nome alla fine di `KNames`.
4. Una riga alla fine di `TableLegacy`, con l'icona di `font.png` che la sostituisce se manca il PNG.

Senza la riga 4, l'icona resta senza coordinate quando non c'è un atlas costruito.
