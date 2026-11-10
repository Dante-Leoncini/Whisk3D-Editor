# Icone

## I file

Ogni icona è un PNG di **10x10** in `res/Skins/Whisk3D/atlas/iconos/`, nel repository dell'editor. Oggi sono 106. I pixel sono bianchi su sfondo trasparente: l'icona prende il colore con cui viene disegnata. Così la stessa icona va bene in grigio, in bianco o nel colore di accento.

## L'enum e i nomi

`draw/icons.h` contiene l'enum `IconType` e il totale, `ICON_TOTAL` (106). In `draw/icons.cpp`, `KNombres` dà il nome di ciascuna, nello stesso ordine: è il nome del file senza `.png`.

```cpp
IconoNombre(IconType::carpeta);   // "carpeta"
IconoIndice("carpeta");           // IconType::carpeta, o -1 si no existe
```

Quasi tutte si chiamano come nell'enum. L'eccezione è `candadoAbierto`, che corrisponde al file `candado_abierto.png`.

Molte sono dell'editor: strumenti di modellazione, tipi di oggetto, curve di animazione. Ci sono anche quelle generali: `arrow` e `arrowRight` (le frecce per comprimere), `visible` e `hidden`, `carpeta`, `guardar`, `borrar`, `mas` e `menos`, `lista` e `cuadricula`, `candado`, `notifOk` e `notifError`, `tijera`, `copiar` e `pegar`.

## Le coordinate

`IconsUV` è un `std::vector<IconUV*>`, con le coordinate di ogni icona nell'atlas (`uvs`, 8 numeri). Si riempie in due modi:

- `CrearIconos(ancho, alto)`: con la tabella che indica dove si trova ogni icona dentro `font.png` (`TablaLegacy`, in `icons.cpp`). È il modo senza la cartella `atlas/iconos/`.
- `CrearIconosDesde(rects, ancho, alto)`: con un elenco proprio di rettangoli, quello dell'atlas costruito. Un rettangolo con `x = -1` usa quello di `font.png`: un'icona senza il suo PNG continua ad apparire.

`SetIconScale(escala)` regola `IconSizeGS` e le due mesh: `IconMesh`, il quadrato di un'icona, e `IconLineMesh`, un quadrato alto quanto una riga di testo, per le linee degli alberi.

## Disegnare un'icona

Con l'atlas impostato, nell'origine attuale:

```cpp
SetColorID(ColorID::grisUI);
W3dDrawStrip4(IconMesh, IconsUV[IconType::carpeta]->uvs);
```

`Button`, `Tab`, `PopupMenu` e `GroupPropertie` ricevono l'icona come un `int` dell'enum, o `-1` per nessuna.

## Aggiungerne una

1. Il PNG di 10x10, in `atlas/iconos/`.
2. Una riga alla fine dell'enum `IconType`, e aggiungere uno a `ICON_TOTAL`.
3. Il suo nome alla fine di `KNombres`.
4. Una riga alla fine di `TablaLegacy`, con l'icona di `font.png` che la sostituisce se manca il PNG.

Senza la riga 4, l'icona resta senza coordinate quando non c'è un atlas costruito.
