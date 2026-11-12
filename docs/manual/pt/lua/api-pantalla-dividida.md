# Tela dividida (dois jogadores no mesmo aparelho)

| Função | O que faz |
|---|---|
| `setVistas(n [, "lado" \| "arriba"])` | `2` divide a tela em duas (uma ao lado da outra ou uma em cima da outra); `1` volta para a tela inteira. A interface 2D (o HUD) é desenhada uma vez, por cima das duas. |
| `setVistaCamara(i, camara)` | A câmera da vista `i` (1 ou 2). Se não for definida, a câmera ativa. |
| `vista() → i, n` | A vista que está sendo desenhada (1 ou 2; 0 fora do desenho) e quantas há. |
| `antesDeVista(i)` | (Definida pelo seu script.) O motor a chama antes de desenhar cada vista, no script que chamou `setVistas`. Ali você vira para ESSA câmera o que olha para a câmera (billboards, partículas) ou esconde o que essa vista não deve ver. Não mova nada que avance com o tempo: isso vai em `actualizar`. |

Cada metade é uma imagem inteira com sua câmera: o quadro do jogo (o aspecto da câmera) se divide em dois.

## Um controle para cada um

`boton`, `botonApretado`, `stick` e `gatillo` aceitam o **número do controle** (1 a 4, na ordem em que foram movidos
pela primeira vez): `boton("a", 2)`, `stick("izq", 1)`. Sem número, qualquer um (todos juntos), como sempre. O teclado
não tem número: divida-o você mesmo (por exemplo, o jogador 1 com as setas e os números, o 2 com WASD e as letras;
num Nokia com teclado QWERTY, as letras também chegam).

```lua
function actualizar(dt)
  local x1, y1 = stick("izq", 1)
  local x2, y2 = stick("izq", 2)
  if tecla("w") then y2 = -1 end
  mover(jugador1, x1 * dt * 5, y1 * dt * 5, 0)
  mover(jugador2, x2 * dt * 5, y2 * dt * 5, 0)
end
```
