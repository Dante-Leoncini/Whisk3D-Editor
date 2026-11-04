# Quando o motor chama o seu script

O seu script define funções com nomes especiais, e o motor as chama sozinho, no momento certo. Todas são opcionais: escreva só as que você precisar.

| Função | Quando é chamada |
|---|---|
| `inicio()` | Uma vez, ao dar Play (ou assim que o objeto é criado com `instanciar`). Serve para buscar objetos e preparar variáveis. |
| `actualizar(dt)` | A cada quadro, enquanto o objeto estiver **visível** e na cena ativa. `dt` são os segundos que passaram desde o quadro anterior (entre 0.001 e 0.1). |
| `alEntrar(otro, miHitbox, suHitbox)` | Quando um hitbox começa a tocar em outro. |
| `alQuedarse(otro, miHitbox, suHitbox)` | A cada quadro, enquanto continuam se tocando. |
| `alSalir(otro, miHitbox, suHitbox)` | Quando deixam de se tocar (também se um deles é ocultado ou desativado). |
| `alTocar(otro, impulso, x, y, z, nx, ny, nz)` | Quando dois **corpos rígidos** colidem. `x, y, z` é o ponto de contato e `nx, ny, nz` a direção para o outro. |

Os eventos de hitbox são recebidos pelos scripts do hitbox e pelos do seu objeto pai. Se o outro for um corpo rígido, `suHitbox` chega como `nil`.

## Mover coisas com dt

Multiplique sempre as velocidades por `dt`. Assim o jogo roda igualmente rápido em um PC potente e em um celular lento:

```lua
function actualizar(dt)
  mover(yo(), 3 * dt, 0, 0)   -- 3 unidades por segundo em direção a +X
end
```

## Ordem dentro de um quadro

1. Física (velocidades, corpos rígidos) e os eventos de colisão: `alTocar`, `alSalir`, `alEntrar`, `alQuedarse`.
2. `actualizar(dt)` de todos os scripts, na ordem do Outliner.
3. Animações (cena, clipes, armações).
4. Fim do quadro: são aplicados os `destruir()` pendentes, entram os objetos instanciados e é feito o `cambiarEscena()` pedido.

## Ao parar o jogo

Ao apertar Stop, todos os scripts são fechados, o espaço compartilhado é apagado e a cena volta a ficar como estava antes do Play. Se você editar um script enquanto o jogo roda, esse objeto o recarrega: suas variáveis voltam a zero e `inicio()` é chamada de novo.
