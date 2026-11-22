# Jugar en red (LAN, wifi o internet)

Una partida en red tiene un **anfitrión** (el que la crea: jugador 0) y los que se **unen** (1, 2…). Todo pasa por el
anfitrión: un jugador le manda al anfitrión y el anfitrión lo reparte. Los que están en la misma red (la misma LAN o el
mismo wifi) se encuentran solos; por internet hay que unirse con la dirección del anfitrión (y que él tenga abierto el
puerto 47023 en su router).

| Función | Qué hace |
|---|---|
| `netAvailable() → bool [, reason]` | Si este juego puede usar la red. Si no, el motivo: `"no permission"` (el editor no le dio permiso a este proyecto) o `"no network"` (esta plataforma todavía no tiene). |
| `netDevice() → text` | El nombre de este dispositivo (el que ven los demás al buscar partidas). Se cambia en el editor: tarjeta **Ajustes**, *Nombre del dispositivo*. |
| `netAddress() → text` | La IP de este equipo en la red local (para unirse por internet o escribiéndola). |
| `netHost(name [, max [, game]]) → bool [, error]` | Crea una partida y espera jugadores (`max` contando al anfitrión: 2 por defecto). `game` es el nombre de tu juego: solo se ven las partidas del mismo juego. |
| `netOpen(bool)` | El anfitrión aparece en las búsquedas y acepta jugadores (`true`) o no (por ejemplo, cuando la partida empezó). |
| `netFind([game]) → bool` | Empieza a buscar partidas en la red local. Llamarla otra vez **actualiza** la lista. |
| `netMatches() → { {name, device, players, max, address}, … }` | Las partidas que respondieron (se actualiza sola mientras buscás). |
| `netJoin(address) → bool` | Entra a una partida: la `address` de `netMatches()` o una escrita (`"192.168.0.10"`, `"x.x.x.x:puerto"`). |
| `netState() → state, error` | `"none"`, `"searching"`, `"waiting"` (sos el anfitrión), `"connecting"`, `"connected"` o `"error"` (con el porqué). |
| `netSelf() → id` | Tu número en la partida (0 = el anfitrión; -1 sin partida). |
| `netPlayers() → { {id, name, isSelf}, … }` | Los que están en la partida (el nombre es el de su dispositivo). |
| `netSilence([id]) → seconds, limite` | Hace cuánto no llega nada de ese jugador (sin `id`: del que hace más). El que se une solo oye al anfitrión, así que para él es el silencio del anfitrión. Pasado el `limite` (8 s) se lo da por ido: el anfitrión lo saca de la partida y al que se unió `netState()` le da `"error"`. |
| `netSend(value [, para [, reliable]]) → bool` | Manda un valor de Lua (número, texto, booleano o tabla, con tablas adentro) a un jugador (`para` = su id) o a todos (`nil`). **Confiable** (por defecto): llega seguro y en orden. Con `false` no se reenvía si se pierde: para lo que mandás cada cuadro (posiciones, el mando), donde el siguiente lo reemplaza. |
| `netReceive() → { {from, data}, … }` | Todo lo que llegó desde la última vez, en orden. |
| `netClose()` | Sale de la partida (al anfitrión: la cierra para todos). El Stop del editor también la cierra. |

## El permiso

Un proyecto que crea, busca o se une a partidas no sale a la red hasta que el usuario lo deja. Al abrirlo (o al darle
Play, si empezó a usar la red después) el editor pregunta **una sola vez** por archivo y recuerda la respuesta. Se
cambia después en la tarjeta **Juego**: *Acceso a la red*. Sin permiso, el juego corre igual y `netAvailable()`
devuelve `false, "no permission"`: mostrá un mensaje en vez de la opción de jugar en red.

## Un ejemplo: la sala

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

## Consejos

- **Un solo cálculo**: lo más simple y lo que mejor anda entre equipos distintos (una PC y un teléfono) es que el
  anfitrión calcule el juego y les mande a los demás lo que tienen que mostrar; los demás solo le mandan su mando. Así
  lo hace la demo de Dragon Ball (`dbz2w3d/lua/pelea.lua`).
- Lo que se manda cada cuadro, **no confiable**; lo que pasa una vez (un aviso, "empezar"), **reliable**.
- **Cuando la red se traba**: si en una pelea `netSilence()` pasa de unas décimas de segundo, el otro dejó de
  mandar (se cortó el wifi, el teléfono pasó a segundo plano). Pausá el juego y avisá ("esperando a…", con lo que
  falta para `limite`): si vuelve, sigue donde estaba; si no, `netState()` o `netPlayers()` lo dicen.
- Mandá lo justo: números y textos cortos. Un mensaje no puede pasar los 60000 bytes.
- En el navegador todavía no hay red (no tiene UDP). Android pide el permiso `INTERNET` (el juego compilado lo trae);
  Symbian, la capacidad `NetworkServices` (el teléfono pregunta por qué punto de acceso salir).
