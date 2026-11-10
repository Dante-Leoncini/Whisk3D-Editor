# Networking

**w3dRed** plays over a local network, wifi or the internet, using UDP. One player **hosts** the match (port 47023) and the others **search** for it on the local network or **join** with an address. Everything goes through the host, up to 8 players. Messages can be **reliable** (they all arrive, and in order: "ready", "start") or **unreliable** (what's sent every frame, where the next one replaces the lost one).

```cpp
w3dRed::Hostear("mi_juego", "La partida de Dante", 2);
// or, on the other side:
w3dRed::Buscar("mi_juego");   // ... w3dRed::Partidas() ... w3dRed::Unirse(direccion);
w3dRed::Enviar(-1, datos, bytes, true);   // -1 = to everyone else
```

| System | Sockets |
|---|---|
| Windows | Winsock |
| Linux, macOS, Android | POSIX (the interface list, from Android 7 on) |
| Symbian | Open C sockets |
| Dreamcast | KallistiOS sockets, through the network adapter |
| Browser, PSP, PS Vita, 3DS | No networking for now: `Soportada()` returns `false` |

A game doesn't use the network unless the application allows it (`SetPermitida`): the editor asks when you open a project that uses it. From Lua: [Network play](../lua/api-red.md).
