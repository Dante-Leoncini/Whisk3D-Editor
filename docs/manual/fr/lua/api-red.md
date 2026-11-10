# Jouer en réseau (LAN, Wi-Fi ou internet)

Une partie en réseau a un **hôte** (celui qui la crée : le joueur 0) et ceux qui la **rejoignent** (1, 2…). Tout passe
par l'hôte : un joueur envoie à l'hôte et l'hôte redistribue. Ceux qui sont sur le même réseau (le même LAN ou le même
Wi-Fi) se trouvent tout seuls ; par internet, il faut rejoindre avec l'adresse de l'hôte (et celui-ci doit avoir ouvert
le port 47023 sur son routeur).

| Fonction | Ce qu'elle fait |
|---|---|
| `redDisponible() → bool [, motivo]` | Si ce jeu peut utiliser le réseau. Sinon, la raison : `"sin permiso"` (l'éditeur n'a pas donné la permission à ce projet) ou `"sin red"` (cette plateforme n'en a pas encore). |
| `redDispositivo() → texto` | Le nom de cet appareil (celui que voient les autres quand ils cherchent des parties). Il se change dans l'éditeur : carte **Réglages**, *Nom de l'appareil*. |
| `redDireccion() → texto` | L'IP de cette machine sur le réseau local (pour rejoindre par internet ou en la tapant). |
| `redHostear(nombre [, maximo [, juego]]) → bool [, error]` | Crée une partie et attend des joueurs (`maximo` en comptant l'hôte : 2 par défaut). `juego` est le nom de votre jeu : on ne voit que les parties du même jeu. |
| `redAbrir(bool)` | L'hôte apparaît dans les recherches et accepte des joueurs (`true`) ou non (par exemple, quand la partie a commencé). |
| `redBuscar([juego]) → bool` | Commence à chercher des parties sur le réseau local. L'appeler de nouveau **met à jour** la liste. |
| `redPartidas() → { {nombre, dispositivo, jugadores, maximo, direccion}, … }` | Les parties qui ont répondu (la liste se met à jour toute seule pendant que vous cherchez). |
| `redUnirse(direccion) → bool` | Entre dans une partie : la `direccion` de `redPartidas()` ou une adresse tapée (`"192.168.0.10"`, `"x.x.x.x:puerto"`). |
| `redEstado() → estado, error` | `"nada"`, `"buscando"`, `"esperando"` (vous êtes l'hôte), `"conectando"`, `"conectado"` ou `"error"` (avec la raison). |
| `redYo() → id` | Votre numéro dans la partie (0 = l'hôte ; -1 sans partie). |
| `redJugadores() → { {id, nombre, yo}, … }` | Ceux qui sont dans la partie (le nom est celui de leur appareil). |
| `redSilencio([id]) → segundos, limite` | Depuis combien de temps rien n'arrive de ce joueur (sans `id` : de celui dont c'est le plus long). Celui qui rejoint n'entend que l'hôte, donc pour lui c'est le silence de l'hôte. Passé la `limite` (8 s), on le considère comme parti : l'hôte le retire de la partie et, chez celui qui a rejoint, `redEstado()` renvoie `"error"`. |
| `redEnviar(valor [, para [, confiable]]) → bool` | Envoie une valeur Lua (nombre, texte, booléen ou table, avec des tables imbriquées) à un joueur (`para` = son id) ou à tous (`nil`). **Fiable** (par défaut) : elle arrive à coup sûr et dans l'ordre. Avec `false`, elle n'est pas renvoyée si elle se perd : pour ce que vous envoyez à chaque image (positions, manette), où l'envoi suivant la remplace. |
| `redRecibir() → { {de, datos}, … }` | Tout ce qui est arrivé depuis la dernière fois, dans l'ordre. |
| `redCerrar()` | Quitte la partie (pour l'hôte : la ferme pour tout le monde). Le Stop de l'éditeur la ferme aussi. |

## La permission

Un projet qui crée, cherche ou rejoint des parties ne sort pas sur le réseau tant que l'utilisateur ne l'a pas autorisé.
À l'ouverture (ou au Play, s'il a commencé à utiliser le réseau après) l'éditeur pose la question **une seule fois** par
fichier et se souvient de la réponse. On la change ensuite dans la carte **Fichier** : *Accès réseau*. Sans
permission, le jeu tourne quand même et `redDisponible()` renvoie `false, "sin permiso"` : affichez un message au lieu
de l'option de jeu en réseau.

## Un exemple : le salon

```lua
local JUEGO = "mi_juego"

function inicio()
  if not redDisponible() then info("sin red") return end
  if propiedad("anfitrion") then redHostear(redDispositivo(), 2, JUEGO) else redBuscar(JUEGO) end
end

function actualizar(dt)
  local estado = redEstado()
  if estado == "buscando" then
    local p = redPartidas()
    if p[1] then redUnirse(p[1].direccion) end
  end
  if teclaApretada("enter") then redEnviar({t = "listo"}) end
  for _, m in ipairs(redRecibir()) do
    if m.datos.t == "listo" then info("el jugador " .. m.de .. " esta listo") end
  end
end
```

## Conseils

- **Un seul calcul** : le plus simple, et ce qui marche le mieux entre des machines différentes (un PC et un téléphone),
  c'est que l'hôte calcule le jeu et envoie aux autres ce qu'ils doivent afficher ; les autres ne lui envoient que leur
  manette. C'est ainsi que fait la démo de Dragon Ball (`dbz2w3d/lua/pelea.lua`).
- Ce qui est envoyé à chaque image, **non fiable** ; ce qui se produit une seule fois (un avis, « commencer »), **fiable**.
- **Quand le réseau se bloque** : si, pendant un combat, `redSilencio()` dépasse quelques dixièmes de seconde, l'autre a
  cessé d'envoyer (le Wi-Fi a coupé, le téléphone est passé en arrière-plan). Mettez le jeu en pause et prévenez
  (« en attente de… », avec le temps restant avant `limite`) : s'il revient, la partie reprend où elle en était ; sinon,
  `redEstado()` ou `redJugadores()` le diront.
- Envoyez le strict nécessaire : des nombres et des textes courts. Un message ne peut pas dépasser 60000 octets.
- Dans le navigateur, il n'y a pas encore de réseau (il n'a pas d'UDP). Android demande la permission `INTERNET` (le jeu
  compilé l'inclut) ; Symbian, la capacité `NetworkServices` (le téléphone demande par quel point d'accès sortir).
