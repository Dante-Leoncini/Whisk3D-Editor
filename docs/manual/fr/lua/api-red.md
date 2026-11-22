# Jouer en réseau (LAN, Wi-Fi ou internet)

Une partie en réseau a un **hôte** (celui qui la crée : le joueur 0) et ceux qui la **rejoignent** (1, 2…). Tout passe
par l'hôte : un joueur envoie à l'hôte et l'hôte redistribue. Ceux qui sont sur le même réseau (le même LAN ou le même
Wi-Fi) se trouvent tout seuls ; par internet, il faut rejoindre avec l'adresse de l'hôte (et celui-ci doit avoir ouvert
le port 47023 sur son routeur).

| Fonction | Ce qu'elle fait |
|---|---|
| `netAvailable() → bool [, reason]` | Si ce jeu peut utiliser le réseau. Sinon, la raison : `"no permission"` (l'éditeur n'a pas donné la permission à ce projet) ou `"no network"` (cette plateforme n'en a pas encore). |
| `netDevice() → text` | Le nom de cet appareil (celui que voient les autres quand ils cherchent des parties). Il se change dans l'éditeur : carte **Réglages**, *Nom de l'appareil*. |
| `netAddress() → text` | L'IP de cette machine sur le réseau local (pour rejoindre par internet ou en la tapant). |
| `netHost(name [, max [, game]]) → bool [, error]` | Crée une partie et attend des joueurs (`max` en comptant l'hôte : 2 par défaut). `game` est le nom de votre jeu : on ne voit que les parties du même jeu. |
| `netOpen(bool)` | L'hôte apparaît dans les recherches et accepte des joueurs (`true`) ou non (par exemple, quand la partie a commencé). |
| `netFind([game]) → bool` | Commence à chercher des parties sur le réseau local. L'appeler de nouveau **met à jour** la liste. |
| `netMatches() → { {name, device, players, max, address}, … }` | Les parties qui ont répondu (la liste se met à jour toute seule pendant que vous cherchez). |
| `netJoin(address) → bool` | Entre dans une partie : la `address` de `netMatches()` ou une adresse tapée (`"192.168.0.10"`, `"x.x.x.x:puerto"`). |
| `netState() → state, error` | `"none"`, `"searching"`, `"waiting"` (vous êtes l'hôte), `"connecting"`, `"connected"` ou `"error"` (avec la raison). |
| `netSelf() → id` | Votre numéro dans la partie (0 = l'hôte ; -1 sans partie). |
| `netPlayers() → { {id, name, isSelf}, … }` | Ceux qui sont dans la partie (le nom est celui de leur appareil). |
| `netSilence([id]) → seconds, limite` | Depuis combien de temps rien n'arrive de ce joueur (sans `id` : de celui dont c'est le plus long). Celui qui rejoint n'entend que l'hôte, donc pour lui c'est le silence de l'hôte. Passé la `limite` (8 s), on le considère comme parti : l'hôte le retire de la partie et, chez celui qui a rejoint, `netState()` renvoie `"error"`. |
| `netSend(value [, para [, reliable]]) → bool` | Envoie une valeur Lua (nombre, texte, booléen ou table, avec des tables imbriquées) à un joueur (`para` = son id) ou à tous (`nil`). **Fiable** (par défaut) : elle arrive à coup sûr et dans l'ordre. Avec `false`, elle n'est pas renvoyée si elle se perd : pour ce que vous envoyez à chaque image (positions, manette), où l'envoi suivant la remplace. |
| `netReceive() → { {from, data}, … }` | Tout ce qui est arrivé depuis la dernière fois, dans l'ordre. |
| `netClose()` | Quitte la partie (pour l'hôte : la ferme pour tout le monde). Le Stop de l'éditeur la ferme aussi. |

## La permission

Un projet qui crée, cherche ou rejoint des parties ne sort pas sur le réseau tant que l'utilisateur ne l'a pas autorisé.
À l'ouverture (ou au Play, s'il a commencé à utiliser le réseau après) l'éditeur pose la question **une seule fois** par
fichier et se souvient de la réponse. On la change ensuite dans la carte **Fichier** : *Accès réseau*. Sans
permission, le jeu tourne quand même et `netAvailable()` renvoie `false, "no permission"` : affichez un message au lieu
de l'option de jeu en réseau.

## Un exemple : le salon

```lua
local GAME = "mi_juego"

function start()
  if not netAvailable() then info("sin red") return end
  if property("anfitrion") then netHost(netDevice(), 2, GAME) else netFind(GAME) end
end

function update(dt)
  local state = netState()
  if state == "searching" then
    local p = netMatches()
    if p[1] then netJoin(p[1].address) end
  end
  if keyPressed("enter") then netSend({t = "listo"}) end
  for _, m in ipairs(netReceive()) do
    if m.data.t == "listo" then info("el jugador " .. m.from .. " esta listo") end
  end
end
```

## Conseils

- **Un seul calcul** : le plus simple, et ce qui marche le mieux entre des machines différentes (un PC et un téléphone),
  c'est que l'hôte calcule le jeu et envoie aux autres ce qu'ils doivent afficher ; les autres ne lui envoient que leur
  manette. C'est ainsi que fait la démo de Dragon Ball (`dbz2w3d/lua/pelea.lua`).
- Ce qui est envoyé à chaque image, **non fiable** ; ce qui se produit une seule fois (un avis, « commencer »), **fiable**.
- **Quand le réseau se bloque** : si, pendant un combat, `netSilence()` dépasse quelques dixièmes de seconde, l'autre a
  cessé d'envoyer (le Wi-Fi a coupé, le téléphone est passé en arrière-plan). Mettez le jeu en pause et prévenez
  (« en attente de… », avec le temps restant avant `limite`) : s'il revient, la partie reprend où elle en était ; sinon,
  `netState()` ou `netPlayers()` le diront.
- Envoyez le strict nécessaire : des nombres et des textes courts. Un message ne peut pas dépasser 60000 octets.
- Dans le navigateur, il n'y a pas encore de réseau (il n'a pas d'UDP). Android demande la permission `INTERNET` (le jeu
  compilé l'inclut) ; Symbian, la capacité `NetworkServices` (le téléphone demande par quel point d'accès sortir).
