# Riquadri e proprietà

## Il riquadro

`GroupPropertie` (`Propieties/GroupPropertie.h`) è un riquadro con un titolo e delle righe. Ogni riga è una proprietà, una classe che eredita da `PropertieBase` e punta a una tua variabile: il riquadro la mostra e la modifica direttamente lì.

```cpp
GroupPropertie* player = new GroupPropertie("Jugador");
player->icon = IconType::gamepad;        // opcional: entre la flecha y el título

PropFloat* vel = new PropFloat("Velocidad", " m/s");
vel->value = &velocidad;
player->properties.push_back(vel);

player->Resize(width, 0);    // el alto lo calcula la tarjeta
player->Render();            // en el origen actual
```

Il riquadro possiede le sue righe: quando lo si elimina, elimina anche loro.

- Il titolo ha una freccia per comprimerlo (`open`). Con `arrow = false` non ce l'ha: per una finestra fluttuante.
- Sotto ci sono due colonne: l'etichetta a sinistra e il valore a destra. `valueWidth` è la parte della larghezza occupata dal valore: 0.5 per impostazione predefinita.
- `reserveKeyBtn = true` lascia all'estrema destra una colonna per il rombo dell'animazione.
- Quasi tutte le righe sono alte quanto una riga di testo più un `gap`. Un numero, una casella o un colore senza variabile (`value` a `NULL`) non occupa spazio e non si disegna.
- `visible = false` nasconde l'intero riquadro.

La riga scelta ha l'etichetta in `accent`, il valore in bianco e il bordo bianco; durante la modifica, il valore e il bordo passano ad `accent`. La riga sotto il mouse è in bianco: la segna chi contiene il riquadro, con `PropHoverGroup` e `PropHoverRow`.

## Scegliere e modificare

`selectIndex` è la riga scelta: `-2` nessuna, `-1` il titolo, da `0` in poi una riga.

| Funzione | Cosa fa |
|---|---|
| `NextSelect()`, `PrevSelect()` | Passano alla riga sotto o sopra. Saltano gli spazi e le righe senza variabile. Restituiscono `true` quando si esce dal riquadro, per passare al successivo |
| `EnterPropertieSelect()` | Sul titolo, comprime o espande il riquadro. Su una riga, la mette in modifica |
| `button_up()`, `button_down()`, `button_left()`, `button_right()` | Durante la modifica, vanno alla riga |
| `Cancel()` | Lascia la riga com'era |

`editing` dice se c'è una riga in modifica. In base a questo, le frecce cambiano il valore o spostano la selezione:

```cpp
if (player->editing) player->button_up();   // cambia el valor
else player->PrevSelect();                    // la fila de arriba
```

Il mouse lo gestisce chi contiene il riquadro: il riquadro non sa dove è finito sullo schermo. In alto ci sono il bordo (`borderGS`) e il titolo (`RowHeightGS + gapGS`). Ogni riga è alta quanto restituisce il suo `Resize(width)`.

## Le righe

| Classe | Cos'è | Come si modifica |
|---|---|---|
| `PropFloat` | Un numero | Con le frecce, o digitandolo |
| `PropBool` | Una casella di controllo, allineata a destra | OK la attiva o la disattiva |
| `PropColor` | Un quadrato del colore, allineato a destra | OK entra in modifica e salva il colore, nel caso si annulli |
| `PropButton` | Un pulsante | OK chiama la sua azione |
| `PropButtonRow` | Più pulsanti in una riga | Sinistra e destra ne scelgono uno; OK lo preme |
| `PropText` | Un campo di testo | OK gli dà il focus per scrivere |
| `PropListMeshParts` | Un elenco con scorrimento | Su e giù scelgono |
| `PropLabel` | Un testo da leggere, su una o più righe | Non si sceglie |
| `PropSeparator` | Una linea | Non si sceglie |
| `PropGap` | Uno spazio di un `gap` | Non si sceglie |
| `PropSelectText` | Un campo con la sua etichetta, senza valore | Per ora non fa niente |

### Numero

```cpp
PropFloat* life = new PropFloat("Vida");
life->value = &lifePlayer;
life->SetRange(0, 100);           // no pasa de ahí
life->integer = true;              // se muestra y avanza de a enteros
life->fineStep = 1;               // arriba y abajo
life->thickStep = 10;            // izquierda y derecha
life->onChange = OnChangeLife;   // void AlCambiarVida()
```

| Campo | Cosa fa |
|---|---|
| `unit` | Un testo attaccato al numero, come `°`. È il secondo argomento del costruttore |
| `fineStep`, `thickStep` | Quanto aggiungono le frecce: 0.01 e 0.1 per impostazione predefinita |
| `integer` | Mostra il numero arrotondato, e le frecce partono dal valore arrotondato. Senza questo ha quattro decimali |
| `centered` | Il numero al centro del suo campo |
| `arrows` | Disegna `<` e `>` ai lati. Il clic su di esse lo gestisce il contenitore |
| `options`, `nOptions` | Con `integer`: il numero è un indice e si mostra il nome di quell'opzione, tradotto con `W3dMenuTranslate` |
| `accelerates` | Sinistra e destra partono da 1 e, se si ripetono, salgono fino a 10 |
| `dragStep` | Quanto cambia per pixel quando si trascina. Il trascinamento lo fa il contenitore |

`Set(v)` assegna il valore entro l'intervallo e chiama `onChange`. Le frecce passano da lì.

Per digitarlo:

```cpp
life->StartEditText();   // muestra el valor, todo seleccionado
// las teclas van a g_activeTextField: ver Entrada y cajas de texto
NumEditCommit();               // Enter: lo aplica
NumEditCancel();               // Esc: queda como estaba
```

`NumEditActive()` dice se c'è un numero in modifica. Un testo che non è un numero vale 0.

### Casella di controllo

`value` punta a un `bool`. `onChange` viene chiamata ogni volta che cambia. L'etichetta occupa tutto lo spazio alla sua sinistra e, se non ci sta, viene tagliata con `..`.

### Colore

`value` punta a quattro `float` (RGBA). La UI disegna il quadrato; il selettore di colore non è della UI. Durante la modifica, le frecce muovono solo il primo componente.

### Pulsante

`action` è la funzione che chiama. `gray = true` lo attenua e non fa niente. `hidden = true` non occupa una riga.

Con `withLabel = true`, la riga è un nome e un pulsante: il nome a sinistra, largo quanto il suo testo, fino al 55 % della riga, e il pulsante in tutto il resto. `labelChars` fissa la larghezza del nome, per allineare più righe. Con `button->dropdown` ha la piccola freccia di un menu. Inoltre:

- `AddPrefix(action, help)` mette un quadrato prima del pulsante, per mostrare un colore.
- `AddExtra(icon, action, help)` mette a destra fino a due piccoli pulsanti con la sola icona. `slotsExtras` riserva loro lo spazio, così restano allineati tra una riga e l'altra. `ExtraInX(x)` dice quale si trova in quella x, per il clic.

### Riga di pulsanti

```cpp
PropButtonRow* row = new PropButtonRow();
row->Add("Aceptar", Accept);
row->Add("Cancelar", Cancel);
player->properties.push_back(row);
```

I pulsanti si dividono la larghezza in parti uguali. Quelli invisibili vengono saltati e gli altri si dividono il loro spazio. `Fire(i)` preme il pulsante `i`: lo chiama il contenitore quando il clic cade in quella parte.

### Campo di testo

```cpp
PropText* name = new PropText("Nombre", "Jugador 1");
// name->field.text tiene lo que se escribió
```

`onClick`: invece di modificare, chiama quella funzione, per esempio per aprire un esploratore di file. `error = true` colora di rosso il bordo della riga scelta, finché non si scrive qualcosa. `hidden = true` non occupa una riga. OK gli dà il focus con `TextFieldFocus`, che oggi è definita nell'editor. Per scrivere, vedi [Input e campi di testo](entrada.md).

### Testo

`PropLabel("texto")` è una riga. `PropLabel("texto", true)` va a capo sugli spazi per stare nella larghezza; una parola che da sola non ci sta resta intera. `red = true`, per un errore. `hidden = true` non occupa una riga.

### Elenco

`PropListMeshParts` mostra `rowsMax` righe, 3 per impostazione predefinita, con una piccola barra se ce ne sono di più. Quella scelta resta sempre visibile. `Cancel` riporta `selectIndex` al valore che aveva quando è entrato in modifica.

Non serve ancora per un elenco qualsiasi: a seconda di `mode`, legge un elenco della scena (le parti di una mesh, le sue mappe UV, i suoi livelli di colore, i modificatori, i gruppi di vertici, le ossa, le animazioni, i vincoli, gli script) o lo chiede a hook di funzioni dell'editor (`MixRow*`, `RoutineRow*`, `PostRow*`).

## Il rombo dell'animazione

Una riga animabile (`animProp` da 0 in poi, in `PropFloat`, `PropBool`, `PropColor` e `PropButton`) mostra un rombo a destra se il riquadro ha `reserveKeyBtn`. Il suo colore indica lo stato:

| Stato | Colore | Cosa significa |
|---|---|---|
| 0 | Grigio | Senza animazione |
| 1 | `accent`, attenuato | Animata, senza un keyframe qui |
| 2 | `accent` | C'è un keyframe qui |
| 3 | Verde | Il valore è cambiato e non è stato salvato |

Il rombo è della UI; il suo stato, no. Lo chiede a un hook che installa il programma, e il clic chiama l'altro:

```cpp
W3dKeyframeState = MiState;   // int MiEstado(int prop, int comp): de 0 a 3
W3dKeyframeToggle = MiToggle;   // void MiToggle(int prop, int comp): pone o saca el cuadro clave
```

Senza hook, il rombo resta grigio. Il riquadro non chiama `W3dKeyframeToggle`: lo chiama chi gestisce il clic. `g_kfFocusProp` punta alla riga il cui rombo ha il focus della tastiera, e lo segna con un bordo bianco. `W3dRenderDiamondKey(state)` disegna un rombo isolato.
