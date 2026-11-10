# Red

**w3dRed** juega en red local, wifi o internet, por UDP. Uno **hostea** la partida (puerto 47023) y los demás la **buscan** en la red local o se **unen** con una dirección. Todo pasa por el host, hasta 8 jugadores. Los mensajes pueden ser **confiables** (llegan todos y en orden: "listo", "empezar") o **no confiables** (lo que se manda cada cuadro, donde el siguiente reemplaza al perdido).

```cpp
w3dRed::Hostear("mi_juego", "La partida de Dante", 2);
// o, del otro lado:
w3dRed::Buscar("mi_juego");   // ... w3dRed::Partidas() ... w3dRed::Unirse(direccion);
w3dRed::Enviar(-1, datos, bytes, true);   // -1 = a todos los demas
```

| Sistema | Sockets |
|---|---|
| Windows | Winsock |
| Linux, macOS, Android | POSIX (la lista de interfaces, desde Android 7) |
| Symbian | Los sockets de Open C |
| Dreamcast | Los sockets de KallistiOS, por el adaptador de red |
| Navegador, PSP, PS Vita, 3DS | Sin red por ahora: `Soportada()` da `false` |

Un juego no usa la red si la aplicación no se lo permite (`SetPermitida`): el editor pregunta al abrir un proyecto que la usa. Desde Lua: [Jugar en red](../lua/api-red.md).
