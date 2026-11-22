# Red

**w3dRed** juega en red local, wifi o internet, por UDP. Uno **hostea** la partida (puerto 47023) y los demás la **buscan** en la red local o se **unen** con una dirección. Todo pasa por el host, hasta 8 jugadores. Los mensajes pueden ser **confiables** (llegan todos y en orden: "listo", "empezar") o **no confiables** (lo que se manda cada cuadro, donde el siguiente reemplaza al perdido).

```cpp
w3dNet::Host("mi_juego", "La partida de Dante", 2);
// o, del otro lado:
w3dNet::Find("mi_juego");   // ... w3dNet::Matches() ... w3dNet::Join(direccion);
w3dNet::Send(-1, data, bytes, true);   // -1 = a todos los demas
```

| Sistema | Sockets |
|---|---|
| Windows | Winsock |
| Linux, macOS, Android | POSIX (la lista de interfaces, desde Android 7) |
| Symbian | Los sockets de Open C |
| Dreamcast | Los sockets de KallistiOS, por el adaptador de red |
| Navegador, PSP, PS Vita, 3DS | Sin red por ahora: `Supported()` da `false` |

Un juego no usa la red si la aplicación no se lo permite (`SetAllowed`): el editor pregunta al abrir un proyecto que la usa. Desde Lua: [Jugar en red](../lua/api-red.md).
