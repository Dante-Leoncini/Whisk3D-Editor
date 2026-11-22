# Network play (LAN, wifi or internet)

A network match has a **host** (the one who creates it: player 0) and those who **join** (1, 2…). Everything goes through
the host: a player sends to the host and the host passes it on. Players on the same network (the same LAN or the same
wifi) find each other on their own; over the internet you have to join with the host's address (and the host has to have
port 47023 open on their router).

| Function | What it does |
|---|---|
| `netAvailable() → bool [, reason]` | Whether this game can use the network. If not, the reason: `"no permission"` (the editor didn't give this project permission) or `"no network"` (this platform doesn't have it yet). |
| `netDevice() → text` | This device's name (the one others see when searching for matches). It's changed in the editor: **Settings** card, *Device name*. |
| `netAddress() → text` | This device's IP on the local network (for joining over the internet or by typing it). |
| `netHost(name [, max [, game]]) → bool [, error]` | Creates a match and waits for players (`max` counting the host: 2 by default). `game` is your game's name: only matches of the same game are shown. |
| `netOpen(bool)` | The host shows up in searches and accepts players (`true`) or not (for example, once the match has started). |
| `netFind([game]) → bool` | Starts searching for matches on the local network. Calling it again **refreshes** the list. |
| `netMatches() → { {name, device, players, max, address}, … }` | The matches that answered (it updates by itself while you search). |
| `netJoin(address) → bool` | Joins a match: the `address` from `netMatches()` or a typed one (`"192.168.0.10"`, `"x.x.x.x:puerto"`). |
| `netState() → state, error` | `"none"`, `"searching"`, `"waiting"` (you're the host), `"connecting"`, `"connected"` or `"error"` (with the reason). |
| `netSelf() → id` | Your number in the match (0 = the host; -1 with no match). |
| `netPlayers() → { {id, name, isSelf}, … }` | Who is in the match (the name is their device's). |
| `netSilence([id]) → seconds, limite` | How long it's been since anything arrived from that player (without `id`: from the one silent the longest). Whoever joins only hears the host, so for them it's the host's silence. Past the `limite` (8 s) the player is considered gone: the host removes them from the match and, for whoever joined, `netState()` returns `"error"`. |
| `netSend(value [, para [, reliable]]) → bool` | Sends a Lua value (number, string, boolean or table, with tables inside) to one player (`para` = their id) or to everyone (`nil`). **Reliable** (the default): it arrives for sure and in order. With `false` it isn't resent if it's lost: for what you send every frame (positions, the gamepad), where the next one replaces it. |
| `netReceive() → { {from, data}, … }` | Everything that arrived since the last time, in order. |
| `netClose()` | Leaves the match (for the host: closes it for everyone). The editor's Stop also closes it. |

## The permission

A project that creates, searches for or joins matches doesn't go out to the network until the user allows it. When you
open it (or when you press Play, if it started using the network later) the editor asks **only once** per file and
remembers the answer. It can be changed later in the **File** card: *Network access*. Without permission, the game runs
anyway and `netAvailable()` returns `false, "no permission"`: show a message instead of the network play option.

## An example: the lobby

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

## Tips

- **A single simulation**: the simplest approach, and the one that works best between different devices (a PC and a
  phone), is for the host to run the game and send the others what they have to show; the others only send their
  gamepad input. That's how the Dragon Ball demo does it (`dbz2w3d/lua/pelea.lua`).
- What's sent every frame, **unreliable**; what happens once (a notice, "start"), **reliable**.
- **When the network stalls**: if during a fight `netSilence()` goes beyond a few tenths of a second, the other player
  stopped sending (the wifi dropped, the phone went to the background). Pause the game and say so ("waiting for…", with
  the time left until `limite`): if they come back, it continues where it was; if not, `netState()` or `netPlayers()`
  will tell you.
- Send only what's needed: numbers and short strings. A message can't exceed 60000 bytes.
- There's no networking in the browser yet (it has no UDP). Android requires the `INTERNET` permission (the built game
  includes it); Symbian, the `NetworkServices` capability (the phone asks which access point to use).
