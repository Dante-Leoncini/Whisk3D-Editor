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

Così lo stesso script serve per molti oggetti con valori diversi: il campo `velocidad` di un nemico può valere 200 e quello di un altro 350.

> [!NOTE]
> Per leggere `propiedades`, l'editor **esegue il file** anche se il gioco non è in esecuzione. Per questo, fuori dalle funzioni metti solo dichiarazioni (tabelle, costanti, `local`), mai codice che fa qualcosa.

Gli script non condividono variabili tra loro. Per passarsi dati (i punti, se il giocatore ha vinto…) usano uno spazio condiviso: `setCompartido("puntos", 10)` scrive e `compartido("puntos")` legge. Memorizza numeri, sì/no e testi, e si svuota quando si ferma il gioco.
