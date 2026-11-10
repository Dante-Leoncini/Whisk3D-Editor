# Schermo diviso (due giocatori sullo stesso dispositivo)

| Funzione | Cosa fa |
|---|---|
| `setVistas(n [, "lado" \| "arriba"])` | `2` divide lo schermo in due (una vista accanto all'altra o una sopra l'altra); `1` torna allo schermo intero. L'interfaccia 2D (l'HUD) si disegna una volta sola, sopra entrambe. |
| `setVistaCamara(i, camara)` | La camera della vista `i` (1 o 2). Se non la imposti, quella attiva. |
| `vista() → i, n` | La vista che si sta disegnando (1 o 2; 0 fuori dal disegno) e quante ce ne sono. |
| `antesDeVista(i)` | (La definisce il tuo script.) Il motore la chiama prima di disegnare ogni vista, nello script che ha chiamato `setVistas`. Lì giri verso QUELLA camera ciò che guarda la camera (cartelli, particelle) o nascondi ciò che quella vista non deve vedere. Non muovere niente che avanza con il tempo: quello va in `actualizar`. |

Ogni metà è un'immagine intera con la sua camera: l'inquadratura del gioco (le proporzioni della camera) si divide in due.

## Un controller per ciascuno

`boton`, `botonApretado`, `stick` e `gatillo` accettano il **numero del controller** (da 1 a 4, nell'ordine in cui sono stati
mossi per la prima volta): `boton("a", 2)`, `stick("izq", 1)`. Senza numero, uno qualsiasi (tutti insieme), come sempre. La
tastiera non ha numero: dividila tu (per esempio, il giocatore 1 con le frecce e i numeri, il 2 con WASD e le lettere;
su un Nokia con tastiera QWERTY arrivano anche le lettere).

```lua
function actualizar(dt)
  local x1, y1 = stick("izq", 1)
  local x2, y2 = stick("izq", 2)
  if tecla("w") then y2 = -1 end
  mover(jugador1, x1 * dt * 5, 0, y1 * dt * 5)
  mover(jugador2, x2 * dt * 5, 0, y2 * dt * 5)
end
```
