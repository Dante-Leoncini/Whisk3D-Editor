# Tempo, acaso e utilitários

| Função | O que faz |
|---|---|
| `random() → 0..1`, `random(min, max) → integer` | Números aleatórios. |
| `clockMs() → ms` | Milissegundos, para medir quanto algo demora. |
| `unixTime() → seconds, ms` | A hora do sistema como tempo Unix: os segundos desde 1/1/1970 em UTC, e à parte os milissegundos (0–999). |
| `unixTimeMs() → ms`, `unixTimeSec() → seconds` | O mesmo tempo Unix em um só número: em milissegundos (inteiro) ou em segundos com decimais. Medir é uma subtração: `t = unixTimeSec() - inicio`. Exatos com Lua de 64 bits (todas as plataformas menos o Dreamcast; lá, `unixTime()`). |
| `date([seconds [, utc]]) → table` | A data e a hora de um tempo Unix (agora, se não passar), local, ou em UTC com `utc = true`: `year`, `month`, `day`, `hour`, `min`, `sec`, `wday` (1 = domingo), `yday`. |
| `luaProfile(frames [, label])` | Mede quais partes do script são lentas (`luaProfile(0)` o desliga). |
| `createLine(name, points)`, `linePoint(id, s)` | Uma curva suave fechada calculada pelo motor (percursos). |

O tempo desde o início: guarde `unixTimeSec()` em `start()` e depois subtraia (tempo real), ou some `dt` em uma variável (tempo de jogo).
