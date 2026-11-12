# O script e a comunicação

| Função | O que faz |
|---|---|
| `yo() → obj` | O objeto dono deste script. |
| `objeto(prop) → obj \| nil` | O objeto escolhido no campo `prop` de `propiedades`. |
| `opcion(prop) → texto` | A opção escolhida em uma lista suspensa de `propiedades`; se nenhuma foi escolhida, a primeira da lista (a padrão). |
| `propiedad(prop) → valor` | O número, sim/não ou texto próprio deste objeto (ou o padrão declarado). |
| `parametro(prop [, def]) → número` | Como `propiedad`, mas sempre número; se não existir, devolve `def` (ou 0). |
| `objetivo() → obj \| nil` | O alvo ("target") do objeto dono. |
| `compartido(clave) → valor` | Lê o espaço compartilhado por todos os scripts. |
| `setCompartido(clave, valor)` | Escreve um número, sim/não ou texto no espaço compartilhado; `nil` o apaga. |
