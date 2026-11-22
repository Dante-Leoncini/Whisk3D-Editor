# Escenas y cinemáticas

| Función | Qué hace |
|---|---|
| `changeScene(name [, restart])` | Pasa a otra escena (menú, nivel 2…) al final del cuadro. |
| `playScene(name [, "funcion"])` | Reproduce una escena 3D como cinemática con el juego en pausa; al terminar llama a esa función. |
| `stopScene3D()`, `scenePlaying()` | Corta la cinemática / dice cuál se está viendo. |
| `draw3D([on])` | Apaga o prende todo el 3D (por ejemplo, detrás de un menú opaco). |
