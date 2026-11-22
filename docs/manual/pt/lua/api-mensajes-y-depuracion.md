# Mensagens e depuração

| Função | O que faz |
|---|---|
| `print(...)` | Escreve no registro (log) do motor, não na tela. |
| `info(msg)`, `warning(msg)`, `error(msg)` | Mensagens de informação, aviso ou erro no registro. `error` **não** interrompe o script. |
| `debugLog(msg)`, `isDebug()` | Mensagens só em versões de depuração. |
| `logCount()`, `logLine(i)` | Ler o registro a partir do jogo (um console próprio). |
