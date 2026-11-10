# Rete

**w3dRed** gioca in rete locale, Wi-Fi o Internet, via UDP. Uno fa da **host** alla partita (porta 47023) e gli altri la **cercano** nella rete locale o si **uniscono** con un indirizzo. Tutto passa per l'host, fino a 8 giocatori. I messaggi possono essere **affidabili** (arrivano tutti e in ordine: "pronto", "inizia") o **non affidabili** (ciò che si invia a ogni fotogramma, dove il successivo sostituisce quello perso).

```cpp
w3dRed::Hostear("mi_juego", "La partida de Dante", 2);
// o, del otro lado:
w3dRed::Buscar("mi_juego");   // ... w3dRed::Partidas() ... w3dRed::Unirse(direccion);
w3dRed::Enviar(-1, datos, bytes, true);   // -1 = a todos los demas
```

| Sistema | Socket |
|---|---|
| Windows | Winsock |
| Linux, macOS, Android | POSIX (l'elenco delle interfacce, da Android 7) |
| Symbian | I socket di Open C |
| Dreamcast | I socket di KallistiOS, tramite l'adattatore di rete |
| Browser, PSP, PS Vita, 3DS | Per ora senza rete: `Soportada()` restituisce `false` |

Un gioco non usa la rete se l'applicazione non glielo permette (`SetPermitida`): l'editor lo chiede quando si apre un progetto che la usa. Da Lua: [Giocare in rete](../lua/api-red.md).
