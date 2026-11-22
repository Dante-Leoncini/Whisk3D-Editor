# Réseau

**w3dRed** permet de jouer en réseau local, en Wi-Fi ou sur internet, en UDP. L'un **héberge** la partie (port 47023) et les autres la **cherchent** sur le réseau local ou la **rejoignent** avec une adresse. Tout passe par l'hôte, jusqu'à 8 joueurs. Les messages peuvent être **fiables** (ils arrivent tous et dans l'ordre : « prêt », « commencer ») ou **non fiables** (ce qui est envoyé à chaque image, où le suivant remplace celui qui s'est perdu).

```cpp
w3dNet::Host("mi_juego", "La partida de Dante", 2);
// ou, de l'autre côté :
w3dNet::Find("mi_juego");   // ... w3dNet::Matches() ... w3dNet::Join(direccion);
w3dNet::Send(-1, data, bytes, true);   // -1 = à tous les autres
```

| Système | Sockets |
|---|---|
| Windows | Winsock |
| Linux, macOS, Android | POSIX (la liste des interfaces, à partir d'Android 7) |
| Symbian | Les sockets d'Open C |
| Dreamcast | Les sockets de KallistiOS, via l'adaptateur réseau |
| Navigateur, PSP, PS Vita, 3DS | Pas de réseau pour l'instant : `Supported()` renvoie `false` |

Un jeu n'utilise pas le réseau si l'application ne le lui permet pas (`SetAllowed`) : l'éditeur pose la question à l'ouverture d'un projet qui l'utilise. Depuis Lua : [Jouer en réseau](../lua/api-red.md).
