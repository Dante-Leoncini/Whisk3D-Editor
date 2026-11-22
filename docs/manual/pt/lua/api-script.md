# O script e a comunicação

| Função | O que faz |
|---|---|
| `self() → obj` | O objeto dono deste script. |
| `object(prop) → obj \| nil` | O objeto escolhido no campo `prop` de `properties`. |
| `option(prop) → text` | A opção escolhida em uma lista suspensa de `properties`; se nenhuma foi escolhida, a primeira da lista (a padrão). |
| `property(prop) → value` | O número, sim/não ou texto próprio deste objeto (ou o padrão declarado). |
| `param(prop [, def]) → número` | Como `property`, mas sempre número; se não existir, devolve `def` (ou 0). |
| `target() → obj \| nil` | O alvo ("target") do objeto dono. |
| `getShared(clave) → value` | Lê o espaço compartilhado por todos os scripts. |
| `setShared(clave, value)` | Escreve um número, sim/não ou texto no espaço compartilhado; `nil` o apaga. |
