# 完整示例

## 你好，世界：用方向键移动一个 2D 方块

```lua
properties = { frame = "object" }

function start()
  beep(440, 120, 0.5)          -- 发出一声蜂鸣，表示脚本已经启动
end

function update(dt)
  local c = object("frame")
  local x, y = getPosPx(c)
  if key("up") then y = y - 200 * dt end
  if key("down")  then y = y + 200 * dt end
  setPosPx(c, x, y)
end
```

## 会走会跳的 3D 角色

```lua
properties = { speed = 4, jump = 6 }

local vz = 0                   -- 垂直速度（用于跳跃）

function update(dt)
  local yo_ = self()
  local v = property("speed")
  local dx, dy = 0, 0
  if key("left") then dx = dx - 1 end
  if key("right")   then dx = dx + 1 end
  if key("up")    then dy = dy - 1 end
  if key("down")     then dy = dy + 1 end
  move(yo_, dx * v * dt, dy * v * dt, 0)

  -- 简单跳跃：向上跳，重力把它拉下来，并且不会低于地面（z = 0）
  local x, y, z = getPosition(yo_)
  if keyPressed("space") and z <= 0 then vz = property("jump") end
  vz = vz - 9.8 * dt
  z = z + vz * dt
  if z < 0 then z = 0; vz = 0 end
  setPosition(yo_, x, y, z)
end
```

## 用碰撞盒收集金币

```lua
-- 这个脚本放在角色上，角色有一个子级碰撞盒
local monedas = 0

function onEnter(other, myHitbox, otherHitbox)
  if string.sub(getName(other), 1, 6) == "Moneda" then
    destroy(other)
    monedas = monedas + 1
    setShared("monedas", monedas)   -- 计分板的脚本会读取它
    sound("sounds/moneda.wav")
  end
end
```

```lua
-- 这个脚本放在计分板的 2D 文本上
function update(dt)
  setText(self(), "MONEDAS: " .. (getShared("monedas") or 0))
end
```

## 每 2 秒生成一个敌人

```lua
local time = 0

function update(dt)
  time = time + dt
  if time >= 2 then
    time = 0
    local x = random(-5, 5)
    instantiate("Enemigo", x, 0, -10, 180)   -- 预制体 "Enemigo"，面朝我们
  end
end
```

## 切换场景的按钮

```lua
properties = { button = "object" }

function update(dt)
  if pressed(object("button")) or keyPressed("enter") then
    changeScene("Nivel1")
  end
end
```

> [!TIP]
> 示例仓库中有一个完整且带注释的 Pong 游戏：`Whisk3D-Examples/ui/whiskpaddle/content/juego.lua`。它用到了 `speed`、`bounceOn`、`limit`、`finger`、`uiBox` 和 `random`。
