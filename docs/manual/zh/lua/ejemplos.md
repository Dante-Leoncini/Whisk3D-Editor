# 完整示例

## 你好，世界：用方向键移动一个 2D 方块

```lua
propiedades = { cuadro = "objeto" }

function inicio()
  beep(440, 120, 0.5)          -- 发出一声蜂鸣，表示脚本已经启动
end

function actualizar(dt)
  local c = objeto("cuadro")
  local x, y = posPx(c)
  if tecla("arriba") then y = y - 200 * dt end
  if tecla("abajo")  then y = y + 200 * dt end
  setPosPx(c, x, y)
end
```

## 会走会跳的 3D 角色

```lua
propiedades = { velocidad = 4, salto = 6 }

local vz = 0                   -- 垂直速度（用于跳跃）

function actualizar(dt)
  local yo_ = yo()
  local v = propiedad("velocidad")
  local dx, dy = 0, 0
  if tecla("izquierda") then dx = dx - 1 end
  if tecla("derecha")   then dx = dx + 1 end
  if tecla("arriba")    then dy = dy - 1 end
  if tecla("abajo")     then dy = dy + 1 end
  mover(yo_, dx * v * dt, dy * v * dt, 0)

  -- 简单跳跃：向上跳，重力把它拉下来，并且不会低于地面（z = 0）
  local x, y, z = posicion(yo_)
  if teclaApretada("espacio") and z <= 0 then vz = propiedad("salto") end
  vz = vz - 9.8 * dt
  z = z + vz * dt
  if z < 0 then z = 0; vz = 0 end
  setPosicion(yo_, x, y, z)
end
```

## 用碰撞盒收集金币

```lua
-- 这个脚本放在角色上，角色有一个子级碰撞盒
local monedas = 0

function alEntrar(otro, miHitbox, suHitbox)
  if string.sub(nombre(otro), 1, 6) == "Moneda" then
    destruir(otro)
    monedas = monedas + 1
    setCompartido("monedas", monedas)   -- 计分板的脚本会读取它
    sonido("sonidos/moneda.wav")
  end
end
```

```lua
-- 这个脚本放在计分板的 2D 文本上
function actualizar(dt)
  setTexto(yo(), "MONEDAS: " .. (compartido("monedas") or 0))
end
```

## 每 2 秒生成一个敌人

```lua
local tiempo = 0

function actualizar(dt)
  tiempo = tiempo + dt
  if tiempo >= 2 then
    tiempo = 0
    local x = azar(-5, 5)
    instanciar("Enemigo", x, 0, -10, 180)   -- 预制体 "Enemigo"，面朝我们
  end
end
```

## 切换场景的按钮

```lua
propiedades = { boton = "objeto" }

function actualizar(dt)
  if apretado(objeto("boton")) or teclaApretada("enter") then
    cambiarEscena("Nivel1")
  end
end
```

> [!TIP]
> 示例仓库中有一个完整且带注释的 Pong 游戏：`Whisk3D-Examples/ui/whiskpaddle/contenido/juego.lua`。它用到了 `velocidad`、`rebotarEn`、`limitar`、`dedo`、`cajaUI` 和 `azar`。
