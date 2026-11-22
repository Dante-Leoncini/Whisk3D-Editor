# Quando il motore chiama il tuo script

Il tuo script definisce funzioni con nomi speciali e il motore le chiama da solo, al momento giusto. Sono tutte facoltative: scrivi solo quelle che ti servono.

| Funzione | Quando viene chiamata |
|---|---|
| `start()` | Una volta, quando premi Play (o appena l'oggetto viene creato con `instantiate`). Serve per cercare oggetti e preparare variabili. |
| `update(dt)` | A ogni fotogramma, finché l'oggetto è **visibile** e nella scena attiva. `dt` sono i secondi passati dal fotogramma precedente (tra 0.001 e 0.1). |
| `onEnter(other, myHitbox, otherHitbox)` | Quando un hitbox inizia a toccarne un altro. |
| `onStay(other, myHitbox, otherHitbox)` | A ogni fotogramma finché continuano a toccarsi. |
| `onExit(other, myHitbox, otherHitbox)` | Quando smettono di toccarsi (anche se uno dei due viene nascosto o disattivato). |
| `onTouch(other, impulse, x, y, z, nx, ny, nz)` | Quando due **corpi rigidi** si scontrano. `x, y, z` è il punto di contatto e `nx, ny, nz` la direzione verso l'altro. |

Gli eventi di hitbox li ricevono gli script dell'hitbox e quelli del suo oggetto genitore. Se l'altro è un corpo rigido, `otherHitbox` arriva come `nil`.

## Muovere le cose con dt

Moltiplica sempre le velocità per `dt`. Così il gioco va alla stessa velocità su un PC potente e su un telefono lento:

```lua
function update(dt)
  move(self(), 3 * dt, 0, 0)   -- 3 unità al secondo verso +X
end
```

## Ordine all'interno di un fotogramma

1. Fisica (velocità, corpi rigidi) e gli eventi di collisione: `onTouch`, `onExit`, `onEnter`, `onStay`.
2. `update(dt)` di tutti gli script, nell'ordine dell'outliner.
3. Animazioni (scena, clip, armature).
4. Fine del fotogramma: si applicano i `destroy()` in sospeso, entrano gli oggetti istanziati e si esegue il `changeScene()` richiesto.

## Quando si ferma il gioco

Quando premi Stop si chiudono tutti gli script, si cancella lo spazio condiviso e la scena torna com'era prima del Play. Se modifichi uno script mentre il gioco è in esecuzione, quell'oggetto lo ricarica: le sue variabili tornano a zero e `start()` viene chiamata di nuovo.
