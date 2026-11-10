# 联网游戏（局域网、Wi-Fi 或互联网）

一局联网游戏有一个**主机**（创建对局的人：玩家 0），以及**加入**的人（1、2……）。所有数据都经过主机：玩家把数据发给主机，再由主机分发。处在同一网络（同一个局域网或同一个 Wi-Fi）中的玩家会自动找到彼此；通过互联网则需要用主机的地址加入（而且主机要在路由器上开放 47023 端口）。

| 函数 | 作用 |
|---|---|
| `redDisponible() → bool [, motivo]` | 这个游戏能否使用网络。如果不能，返回原因：`"sin permiso"`（编辑器没有给这个项目授权）或 `"sin red"`（这个平台暂时还没有网络功能）。 |
| `redDispositivo() → texto` | 本设备的名称（其他人搜索对局时看到的名称）。在编辑器中修改：**Settings** 卡片，*Device name*。 |
| `redDireccion() → texto` | 本机在局域网中的 IP（用于通过互联网加入，或手动输入）。 |
| `redHostear(nombre [, maximo [, juego]]) → bool [, error]` | 创建一个对局并等待玩家（`maximo` 包括主机在内：默认为 2）。`juego` 是你的游戏名称：只会看到同一个游戏的对局。 |
| `redAbrir(bool)` | 主机是否出现在搜索结果中并接受玩家（`true`），或者不出现、不接受（例如对局已经开始时）。 |
| `redBuscar([juego]) → bool` | 开始在局域网中搜索对局。再次调用会**刷新**列表。 |
| `redPartidas() → { {nombre, dispositivo, jugadores, maximo, direccion}, … }` | 已经响应的对局（在你搜索期间会自动更新）。 |
| `redUnirse(direccion) → bool` | 加入一个对局：使用 `redPartidas()` 中的 `direccion`，或手动输入的地址（`"192.168.0.10"`、`"x.x.x.x:puerto"`）。 |
| `redEstado() → estado, error` | `"nada"`、`"buscando"`、`"esperando"`（你是主机）、`"conectando"`、`"conectado"` 或 `"error"`（附带原因）。 |
| `redYo() → id` | 你在对局中的编号（0 = 主机；-1 表示不在对局中）。 |
| `redJugadores() → { {id, nombre, yo}, … }` | 对局中的玩家（名称是其设备的名称）。 |
| `redSilencio([id]) → segundos, limite` | 距离上次收到该玩家的数据已经过了多久（不带 `id`：取最久的那个玩家）。加入的一方只收得到主机的数据，所以对他来说，这是主机的静默时间。超过 `limite`（8 秒）就视为已离开：主机会把他移出对局，而对加入的一方，`redEstado()` 会返回 `"error"`。 |
| `redEnviar(valor [, para [, confiable]]) → bool` | 把一个 Lua 值（数字、文本、布尔值或表，表中可以嵌套表）发送给某个玩家（`para` = 他的 id）或所有人（`nil`）。**可靠**（默认）：一定送达且保持顺序。用 `false` 时，丢失了也不会重发：适合每帧发送的内容（位置、手柄输入），下一条会取代它。 |
| `redRecibir() → { {de, datos}, … }` | 自上次调用以来收到的所有内容，按顺序排列。 |
| `redCerrar()` | 退出对局（如果是主机：为所有人关闭对局）。编辑器的 Stop 也会关闭对局。 |

## 权限

一个创建、搜索或加入对局的项目，在用户允许之前不会访问网络。打开项目时（或者按下 Play 时，如果它是后来才开始使用网络的），编辑器会针对每个文件**只询问一次**，并记住回答。之后可以在 **File** 卡片中修改：*Network access*。没有权限时，游戏照常运行，`redDisponible()` 返回 `false, "sin permiso"`：此时请显示一条消息，代替联网游戏的选项。

## 示例：房间

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

## 建议

- **只算一份**：最简单、在不同设备之间（一台 PC 和一部手机）效果也最好的做法，是由主机计算游戏，再把其他人需要显示的内容发给他们；其他人只把自己的手柄输入发给主机。Dragon Ball 演示（`dbz2w3d/lua/pelea.lua`）就是这样做的。
- 每帧发送的内容用**不可靠**方式；只发生一次的事情（一条通知、“开始”）用**可靠**方式。
- **网络卡住时**：如果在对战中 `redSilencio()` 超过零点几秒，说明对方停止了发送（Wi-Fi 断了，或者手机切到了后台）。暂停游戏并提示（“正在等待……”，并显示距离 `limite` 还剩多久）：如果对方回来了，就从中断处继续；如果没有，`redEstado()` 或 `redJugadores()` 会告诉你。
- 只发送必要的内容：数字和短文本。一条消息不能超过 60000 字节。
- 浏览器中暂时还没有网络功能（它不支持 UDP）。Android 需要 `INTERNET` 权限（编译出的游戏已包含）；Symbian 需要 `NetworkServices` 能力（手机会询问通过哪个接入点连接）。
