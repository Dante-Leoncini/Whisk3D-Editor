# Quando il motore chiama il tuo script

Il tuo script definisce funzioni con nomi speciali e il motore le chiama da solo, al momento giusto. Sono tutte facoltative: scrivi solo quelle che ti servono.

| Funzione | Quando viene chiamata |
|---|---|
| `inicio()` | Una volta, quando premi Play (o appena l'oggetto viene creato con `instanciar`). Serve per cercare oggetti e preparare variabili. |
| `actualizar(dt)` | A ogni fotogramma, finché l'oggetto è **visibile** e nella scena attiva. `dt` sono i secondi passati dal fotogramma precedente (tra 0.001 e 0.1). |
| `alEntrar(otro, miHitbox, suHitbox)` | Quando un hitbox inizia a toccarne un altro. |
| `alQuedarse(otro, miHitbox, suHitbox)` | A ogni fotogramma finché continuano a toccarsi. |
| `alSalir(otro, miHitbox, suHitbox)` | Quando smettono di toccarsi (anche se uno dei due viene nascosto o disattivato). |
| `alTocar(otro, impulso, x, y, z, nx, ny, nz)` | Quando due **corpi rigidi** si scontrano. `x, y, z` è il punto di contatto e `nx, ny, nz` la direzione verso l'altro. |

Gli eventi di hitbox li ricevono gli script dell'hitbox e quelli del suo oggetto genitore. Se l'altro è un corpo rigido, `suHitbox` arriva come `nil`.

## Muovere le cose con dt

Moltiplica sempre le velocità per `dt`. Così il gioco va alla stessa velocità su un PC potente e su un telefono lento:

```lua
function actualizar(dt)
  mover(yo(), 3 * dt, 0, 0)   -- 3 unità al secondo verso +X
end
```

## Ordine all'interno di un fotogramma

1. Fisica (velocità, corpi rigidi) e gli eventi di collisione: `alTocar`, `alSalir`, `alEntrar`, `alQuedarse`.
2. `actualizar(dt)` di tutti gli script, nell'ordine dell'outliner.
3. Animazioni (scena, clip, armature).
4. Fine del fotogramma: si applicano i `destruir()` in sospeso, entrano gli oggetti istanziati e si esegue il `cambiarEscena()` richiesto.

## Quando si ferma il gioco

Quando premi Stop si chiudono tutti gli script, si cancella lo spazio condiviso e la scena torna com'era prima del Play. Se modifichi uno script mentre il gioco è in esecuzione, quell'oggetto lo ricarica: le sue variabili tornano a zero e `inicio()` viene chiamata di nuovo.
