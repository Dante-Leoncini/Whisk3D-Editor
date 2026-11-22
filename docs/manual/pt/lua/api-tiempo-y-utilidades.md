# Tempo, acaso e utilitários

| Função | O que faz |
|---|---|
| `random() → 0..1`, `random(min, max) → integer` | Números aleatórios. |
| `clockMs() → ms` | Milissegundos, para medir quanto algo demora. |
| `luaProfile(frames [, label])` | Mede quais partes do script são lentas (`luaProfile(0)` o desliga). |
| `createLine(name, points)`, `linePoint(id, s)` | Uma curva suave fechada calculada pelo motor (percursos). |

Não há uma função de "tempo desde o início": some `dt` em uma variável.
