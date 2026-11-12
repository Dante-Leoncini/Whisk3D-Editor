# Uno script è un componente di un oggetto

In Whisk3D uno script appartiene sempre a un **oggetto** della scena (una mesh, un empty, un pulsante 2D…). Un oggetto può avere più script; ognuno viene eseguito separatamente, con le proprie variabili, nell'ordine in cui compaiono.

Dallo script, `yo()` restituisce l'oggetto proprietario. Per usare *altri* oggetti, lo script dichiara una tabella globale chiamata `propiedades`. L'editor la legge e mostra un campo per ogni voce nel riquadro dello script, nel pannello Proprietà:

```lua
propiedades = {
  pelota     = "objeto",                          -- un campo per scegliere un oggetto della scena
  dificultad = { "facil", "normal", "dificil" },  -- un menu a tendina (la prima è quella predefinita)
  velocidad  = 200,                               -- un numero proprio di ogni oggetto che usa lo script
  activo     = true,                              -- un sì/no
  etiqueta   = "caja",                            -- un testo
}

function inicio()
  local p   = objeto("pelota")         -- l'oggetto scelto nel campo "pelota"
  local dif = opcion("dificultad")     -- "facil", "normal" o "dificil"
  local v   = propiedad("velocidad")   -- 200, o il valore impostato in quell'oggetto
end
```

> [!NOTE]
> Per leggere `propiedades`, l'editor **esegue il file** anche se il gioco non è in esecuzione. Per questo, fuori dalle funzioni metti solo dichiarazioni (tabelle, costanti, `local`), mai codice che fa qualcosa.

Ecco come appare nel riquadro dello script, in Proprietà: un campo per ogni proprietà, nello stesso ordine del codice.

![Il riquadro dello script in Proprietà: pelota (un oggetto da scegliere), dificultad (un elenco), velocidad (200), activo (una casella spuntata) ed etiqueta (caja)](../tutoriales/img/lua-propiedades.webp)

Così lo stesso script serve per molti oggetti con valori diversi: il campo `velocidad` di un nemico può valere 200 e quello di un altro 350.

## Cambiare le proprietà in tempo reale

`propiedad("velocidad")` legge il valore in quel momento. Se lo cambi in Proprietà con il gioco in esecuzione, lo script lo vede nel fotogramma successivo, ma solo se lo rilegge con `propiedad()`. Se l'hai salvato in una variabile (`local v = propiedad("velocidad")` in `inicio()`), la variabile resta con il valore iniziale.

```lua
function actualizar(dt)
  -- si legge a ogni fotogramma: vede quello che cambi in Proprietà con il gioco in esecuzione
  mover(yo(), propiedad("velocidad") * dt, 0, 0)
end
```

Leggere `propiedad()` a ogni fotogramma è più lento che usare una variabile. Non conviene lavorare così in tutto il gioco: usalo mentre regoli i valori e, quando sono a posto, salvali in variabili in `inicio()`.

## Pulsanti

Se una proprietà è una **funzione**, nel riquadro compare un **pulsante**: toccarlo chiama quella funzione. Perché il pulsante mostri qualcosa di diverso dal nome, metti il testo e la funzione in una tabella:

```lua
propiedades = {
  velocidad = 90,
  cero = { "Torna a zero", function()
    setRotacion(yo(), 0, 0, 0)
  end },
}
```

![Il riquadro dello script con velocidad e il pulsante Torna a zero](../tutoriales/img/lua-boton.webp)

Con il gioco in esecuzione, la funzione viene eseguita nello script in gioco. Senza il gioco, lo script viene caricato solo per quella chiamata: così un pulsante funziona come un **componente aggiuntivo dell'editor**, che fa qualcosa alla scena. Per esempio, con `seleccionados()`, gli oggetti selezionati nell'editor:

```lua
propiedades = {
  subir = { "Alza la selezione", function()
    for _, o in ipairs(seleccionados()) do
      mover(o, 0, 0, 1)   -- un metro verso l'alto
    end
  end },
}
```

> [!NOTE]
> Quello che fa un pulsante con il gioco fermo resta nella scena, come se l'avessi fatto a mano, e si annulla allo stesso modo: <kbd>Ctrl</kbd> + <kbd>Z</kbd> annulla in un colpo tutto quello che ha fatto il pulsante. Mentre il gioco è in corso non va niente nella cronologia: quando tocchi **Ferma**, tutto torna com'era.

Gli script non condividono variabili tra loro. Per passarsi dati (i punti, se il giocatore ha vinto…) usano uno spazio condiviso: `setCompartido("puntos", 10)` scrive e `compartido("puntos")` legge. Memorizza numeri, sì/no e testi, e si svuota quando si ferma il gioco.
